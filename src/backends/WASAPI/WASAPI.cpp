// Copyright 2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "WASAPI.hpp"

#include "Backend.h"

#include <functional>
#include <thread>
#include <vector>

#include <Windows.h>

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>

using FluxData = CrossAudio_FluxData;

const char *name() {
	return "WASAPI";
}

const char *version() {
	constexpr auto moduleName = "MMDevAPI.dll";

	DWORD handle;
	const auto infoSize = GetFileVersionInfoSize(moduleName, &handle);
	if (!infoSize) {
		return nullptr;
	}

	std::vector< std::byte > infoBuffer(infoSize);
	if (!GetFileVersionInfo(moduleName, handle, infoSize, infoBuffer.data())) {
		return nullptr;
	}

	void *versionBuffer;
	UINT versionSize;
	if (!VerQueryValue(infoBuffer.data(), "\\", &versionBuffer, &versionSize) || !versionSize) {
		return nullptr;
	}

	const auto version = static_cast< VS_FIXEDFILEINFO * >(versionBuffer);
	if (version->dwSignature != 0xfeef04bd) {
		return nullptr;
	}

	// Max: "65535.65535.65535.65535"
	static char versionString[24];
	sprintf(versionString, "%hu.%hu.%hu.%hu", (version->dwFileVersionMS >> 16) & 0xffff,
			(version->dwFileVersionMS >> 0) & 0xffff, (version->dwFileVersionLS >> 16) & 0xffff,
			(version->dwFileVersionLS >> 0) & 0xffff);

	return versionString;
}

ErrorCode init() {
	switch (CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY)) {
		case S_OK:
		case S_FALSE:
		case RPC_E_CHANGED_MODE:
			return CROSSAUDIO_EC_OK;
		default:
			return CROSSAUDIO_EC_INIT;
	}
}

ErrorCode deinit() {
	CoUninitialize();

	return CROSSAUDIO_EC_OK;
}

BE_Engine *engineNew() {
	if (auto engine = new BE_Engine()) {
		if (*engine) {
			return engine;
		}

		delete engine;
	}

	return nullptr;
}


ErrorCode engineFree(BE_Engine *engine) {
	delete engine;

	return CROSSAUDIO_EC_OK;
}

ErrorCode engineStart(BE_Engine *engine) {
	return engine->start();
}

ErrorCode engineStop(BE_Engine *engine) {
	return engine->stop();
}

const char *engineNameGet(BE_Engine *engine) {
	return engine->nameGet();
}

ErrorCode engineNameSet(BE_Engine *engine, const char *name) {
	return engine->nameSet(name);
}

BE_Flux *fluxNew(BE_Engine *engine) {
	if (auto flux = new BE_Flux(*engine)) {
		if (*flux) {
			return flux;
		}

		delete flux;
	}

	return nullptr;
}

ErrorCode fluxFree(BE_Flux *flux) {
	delete flux;

	return CROSSAUDIO_EC_OK;
}

ErrorCode fluxStart(BE_Flux *flux, FluxConfig *config, const FluxFeedback *feedback) {
	return flux->start(*config, feedback ? *feedback : FluxFeedback());
}

ErrorCode fluxStop(BE_Flux *flux) {
	return flux->stop();
}

const char *fluxNameGet(BE_Flux *flux) {
	return flux->nameGet();
}

ErrorCode fluxNameSet(BE_Flux *flux, const char *name) {
	return flux->nameSet(name);
}

// clang-format off
const BE_Impl WASAPI_Impl = {
	name,
	version,

	init,
	deinit,

	engineNew,
	engineFree,
	engineStart,
	engineStop,
	engineNameGet,
	engineNameSet,

	fluxNew,
	fluxFree,
	fluxStart,
	fluxStop,
	fluxNameGet,
	fluxNameSet
};
// clang-format on

BE_Engine::BE_Engine() : m_enumerator(nullptr) {
	CoCreateGuid(reinterpret_cast< GUID * >(&m_sessionID));

	CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
					 reinterpret_cast< void ** >(&m_enumerator));
}

BE_Engine::~BE_Engine() {
	if (m_enumerator) {
		m_enumerator->Release();
	}
}

ErrorCode BE_Engine::start() {
	return CROSSAUDIO_EC_OK;
}

ErrorCode BE_Engine::stop() {
	return CROSSAUDIO_EC_OK;
}

const char *BE_Engine::nameGet() const {
	return m_name.data();
}

ErrorCode BE_Engine::nameSet(const char *name) {
	m_name = name;

	return CROSSAUDIO_EC_OK;
}

BE_Flux::BE_Flux(BE_Engine &engine)
	: m_engine(engine), m_feedback(), m_device(nullptr), m_client(nullptr),
	  m_event(CreateEvent(nullptr, false, false, nullptr)) {
}

BE_Flux::~BE_Flux() {
	stop();

	if (m_event) {
		CloseHandle(m_event);
	}
}

ErrorCode BE_Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	constexpr auto role     = eCommunications;
	constexpr auto category = AudioCategory_Communications;

	if (m_thread) {
		return CROSSAUDIO_EC_INIT;
	}

	m_halt     = false;
	m_feedback = feedback;

	EDataFlow dataflow;
	std::function< void() > threadFunc;

	switch (config.direction) {
		case CROSSAUDIO_DIR_IN:
			dataflow   = eCapture;
			threadFunc = [this]() { processInput(); };
			break;
		case CROSSAUDIO_DIR_OUT:
			dataflow   = eRender;
			threadFunc = [this]() { processOutput(); };
			break;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	if (m_engine.m_enumerator->GetDefaultAudioEndpoint(dataflow, role, &m_device) != S_OK) {
		return CROSSAUDIO_EC_GENERIC;
	}

	if (m_device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, reinterpret_cast< void ** >(&m_client))
		!= S_OK) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	AudioClientProperties props{};
	props.cbSize    = sizeof(props);
	props.eCategory = category;
	m_client->IsOffloadCapable(category, &props.bIsOffload);

	if (m_client->SetClientProperties(&props) != S_OK) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	WAVEFORMATEXTENSIBLE fmt{};
	WAVEFORMATEX &fmtBasic          = fmt.Format;
	fmt.Samples.wValidBitsPerSample = sizeof(uint32_t) * 8;
	fmt.dwChannelMask               = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	fmt.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;
	fmtBasic.cbSize                 = sizeof(fmt) - sizeof(fmtBasic);
	fmtBasic.wFormatTag             = WAVE_FORMAT_EXTENSIBLE;
	fmtBasic.nChannels              = config.channels;
	fmtBasic.nSamplesPerSec         = config.sampleRate;
	fmtBasic.nBlockAlign            = fmtBasic.nChannels * fmtBasic.wBitsPerSample / 8;
	fmtBasic.nAvgBytesPerSec        = fmtBasic.nBlockAlign * fmtBasic.nSamplesPerSec;
	fmtBasic.wBitsPerSample         = fmt.Samples.wValidBitsPerSample;

	WAVEFORMATEXTENSIBLE *fmtProposed;
	switch (m_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &fmtBasic,
										reinterpret_cast< WAVEFORMATEX ** >(&fmtProposed))) {
		case S_FALSE:
			fmt = *fmtProposed;
			CoTaskMemFree(fmtProposed);
			[[fallthrough]];
		case S_OK:
			break;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	UINT32 framesDef, framesMul, framesMin, framesMax;
	if (m_client->GetSharedModeEnginePeriod(&fmtBasic, &framesDef, &framesMul, &framesMin, &framesMax) != S_OK) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	if (m_client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, framesDef, &fmtBasic,
											  reinterpret_cast< GUID * >(&m_engine.m_sessionID))
		!= S_OK) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	m_thread = std::make_unique< std::thread >(threadFunc);

	return CROSSAUDIO_EC_OK;
}

ErrorCode BE_Flux::stop() {
	m_halt = true;
	SetEvent(m_event);

	if (m_thread) {
		m_thread->join();
		m_thread.reset();
	}

	if (m_client) {
		m_client->Release();
		m_client = nullptr;
	}

	if (m_device) {
		m_device->Release();
		m_device = nullptr;
	}

	return CROSSAUDIO_EC_OK;
}

const char *BE_Flux::nameGet() const {
	// TODO: Implement this.
	return nullptr;
}

ErrorCode BE_Flux::nameSet(const char *name) {
	// TODO: Implement this.
	(name);

	return CROSSAUDIO_EC_OK;
}

void BE_Flux::processInput() {
	if (init() != CROSSAUDIO_EC_OK) {
		return;
	}

	DWORD taskIndex       = 0;
	const auto avrtHandle = AvSetMmThreadCharacteristics("Pro Audio", &taskIndex);

	IAudioCaptureClient *client;
	if (m_client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast< void ** >(&client)) != S_OK) {
		goto cleanup;
	}

	if (m_client->SetEventHandle(m_event) != S_OK) {
		goto cleanup;
	}

	if (m_client->Start() != S_OK) {
		goto cleanup;
	}

	while (!m_halt) {
		UINT32 frames;
		if (client->GetNextPacketSize(&frames) != S_OK) {
			goto cleanup;
		}

		while (frames) {
			BYTE *buffer;
			DWORD flags;
			if (client->GetBuffer(&buffer, &frames, &flags, nullptr, nullptr) != S_OK) {
				goto cleanup;
			}

			FluxData fluxData = { flags & AUDCLNT_BUFFERFLAGS_SILENT ? nullptr : buffer, frames };
			m_feedback.process(m_feedback.userData, &fluxData);

			if (client->ReleaseBuffer(frames) != S_OK) {
				goto cleanup;
			}

			if (m_halt || client->GetNextPacketSize(&frames) != S_OK) {
				goto cleanup;
			}
		}

		if (WaitForSingleObject(m_event, INFINITE) == WAIT_FAILED) {
			goto cleanup;
		}
	}

cleanup:
	m_client->Stop();

	if (client) {
		client->Release();
	}

	if (avrtHandle) {
		AvRevertMmThreadCharacteristics(avrtHandle);
	}

	deinit();
}

void BE_Flux::processOutput() {
	if (init() != CROSSAUDIO_EC_OK) {
		return;
	}

	DWORD taskIndex       = 0;
	const auto avrtHandle = AvSetMmThreadCharacteristics("Pro Audio", &taskIndex);

	IAudioRenderClient *client;
	if (m_client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast< void ** >(&client)) != S_OK) {
		goto cleanup;
	}

	if (m_client->SetEventHandle(m_event) != S_OK) {
		goto cleanup;
	}

	if (m_client->Start() != S_OK) {
		goto cleanup;
	}

	UINT32 framesMax;
	if (m_client->GetBufferSize(&framesMax) != S_OK) {
		goto cleanup;
	}

	while (!m_halt) {
		UINT32 framesPending;
		if (m_client->GetCurrentPadding(&framesPending) != S_OK) {
			goto cleanup;
		}

		while (const auto frames = framesMax - framesPending) {
			BYTE *buffer;
			if (client->GetBuffer(frames, &buffer) != S_OK) {
				goto cleanup;
			}

			FluxData fluxData = { buffer, frames };
			m_feedback.process(m_feedback.userData, &fluxData);

			if (client->ReleaseBuffer(frames, 0) != S_OK) {
				goto cleanup;
			}

			if (m_halt || m_client->GetCurrentPadding(&framesPending) != S_OK) {
				goto cleanup;
			}
		}

		if (WaitForSingleObject(m_event, INFINITE) == WAIT_FAILED) {
			goto cleanup;
		}
	}

cleanup:
	m_client->Stop();

	if (client) {
		client->Release();
	}

	if (avrtHandle) {
		AvRevertMmThreadCharacteristics(avrtHandle);
	}

	deinit();
}
