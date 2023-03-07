// Copyright 2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "WASAPI.hpp"

#include "Backend.h"

#include <cstring>
#include <functional>
#include <thread>
#include <vector>

#include <Windows.h>

#include <audioclient.h>
#include <avrt.h>
#include <functiondiscoverykeys.h>
#include <mmdeviceapi.h>

using FluxData = CrossAudio_FluxData;

static constexpr WAVEFORMATEXTENSIBLE configToWaveFormat(const FluxConfig &config);

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

Node *engineNodesGet(BE_Engine *engine) {
	return engine->engineNodesGet();
}

ErrorCode engineNodesFree(BE_Engine *engine, Node *nodes) {
	return engine->engineNodesFree(nodes);
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
	engineNodesGet,
	engineNodesFree,

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

::Node *BE_Engine::engineNodesGet() {
	IMMDeviceCollection *collection;
	if (m_enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &collection) != S_OK) {
		return nullptr;
	}

	UINT count = 0;
	collection->GetCount(&count);

	auto nodes = static_cast< ::Node * >(calloc(count + 1, sizeof(::Node)));

	for (decltype(count) i = 0; i < count; ++i) {
		IMMDevice *device;
		if (collection->Item(i, &device) != S_OK) {
			continue;
		}

		IPropertyStore *store;
		if (device->OpenPropertyStore(STGM_READ, &store) != S_OK) {
			device->Release();
			continue;
		}

		LPWSTR id;
		if (device->GetId(&id) == S_OK) {
			auto &node = nodes[i];

			IMMEndpoint *endpoint;
			if (device->QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast< void ** >(&endpoint)) == S_OK) {
				EDataFlow dataflow;
				if (endpoint->GetDataFlow(&dataflow) == S_OK) {
					switch (dataflow) {
						case eRender:
							node.direction = CROSSAUDIO_DIR_OUT;
							break;
						case eCapture:
							node.direction = CROSSAUDIO_DIR_IN;
							break;
						case eAll:
							node.direction = CROSSAUDIO_DIR_BOTH;
							break;
					}
				}

				endpoint->Release();
			}

			node.id = utf16To8(id);
			CoTaskMemFree(id);

			PROPVARIANT varName{};
			if (store->GetValue(PKEY_Device_FriendlyName, &varName) == S_OK) {
				node.name = utf16To8(varName.pwszVal);
				PropVariantClear(&varName);
			}
		}

		store->Release();
		device->Release();
	}

	collection->Release();

	return nodes;
}

ErrorCode BE_Engine::engineNodesFree(::Node *nodes) {
	for (size_t i = 0; i < std::numeric_limits< size_t >::max(); ++i) {
		auto &node = nodes[i];
		if (!node.id) {
			break;
		}

		free(node.id);
		free(node.name);
	}

	free(nodes);

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

	if (config.node && strcmp(config.node, CROSSAUDIO_FLUX_DEFAULT_NODE) != 0) {
		const auto node = utf8To16(config.node);
		const auto ret  = m_engine.m_enumerator->GetDevice(node, &m_device);
		free(node);

		if (ret != S_OK) {
			return CROSSAUDIO_EC_GENERIC;
		}
	} else if (m_engine.m_enumerator->GetDefaultAudioEndpoint(dataflow, role, &m_device) != S_OK) {
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

	auto fmt       = configToWaveFormat(config);
	auto &fmtBasic = fmt.Format;
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

		while (auto frames = framesMax - framesPending) {
			BYTE *buffer;
			if (client->GetBuffer(frames, &buffer) != S_OK) {
				goto cleanup;
			}

			FluxData fluxData = { buffer, frames };
			m_feedback.process(m_feedback.userData, &fluxData);

			DWORD flags = 0;

			if (!fluxData.data) {
				flags |= AUDCLNT_BUFFERFLAGS_SILENT;
			} else if (fluxData.frames < frames) {
				if (fluxData.frames) {
					memset(buffer + fluxData.frames, frames - fluxData.frames, 0);
				} else {
					frames = 0;
				}
			}

			if (client->ReleaseBuffer(frames, flags) != S_OK) {
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

static char *utf16To8(const wchar_t *utf16) {
	const auto utf16Size = static_cast< int >(wcslen(utf16) + 1);

	const auto utf8Size = WideCharToMultiByte(CP_UTF8, 0, utf16, utf16Size, nullptr, 0, nullptr, nullptr);
	auto utf8           = static_cast< char           *>(malloc(utf8Size));
	if (WideCharToMultiByte(CP_UTF8, 0, utf16, utf16Size, utf8, utf8Size, nullptr, nullptr) <= 0) {
		free(utf8);
		return nullptr;
	}

	return utf8;
}

static wchar_t *utf8To16(const char *utf8) {
	const auto utf8Size = static_cast< int >(strlen(utf8) + 1);

	const int utf16Size = sizeof(wchar_t) * MultiByteToWideChar(CP_UTF8, 0, utf8, utf8Size, nullptr, 0);
	auto utf16          = static_cast< wchar_t          *>(malloc(utf16Size));
	if (MultiByteToWideChar(CP_UTF8, 0, utf8, utf8Size, utf16, utf16Size / sizeof(wchar_t)) <= 0) {
		free(utf16);
		return nullptr;
	}

	return utf16;
}

static constexpr WAVEFORMATEXTENSIBLE configToWaveFormat(const FluxConfig &config) {
	WAVEFORMATEXTENSIBLE fmt{};

	switch (config.bitFormat) {
		default:
		case CROSSAUDIO_BF_NONE:
			break;
		case CROSSAUDIO_BF_INTEGER_SIGNED:
		case CROSSAUDIO_BF_INTEGER_UNSIGNED:
			fmt.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			break;
		case CROSSAUDIO_BF_FLOAT:
			fmt.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
			break;
	}

	fmt.Samples.wValidBitsPerSample = config.sampleBits;
	// TODO: Set full channel mask and adapt positions automatically.
	fmt.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;

	auto &fmtBasic           = fmt.Format;
	fmtBasic.cbSize          = sizeof(fmt) - sizeof(fmtBasic);
	fmtBasic.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
	fmtBasic.nChannels       = config.channels;
	fmtBasic.nSamplesPerSec  = config.sampleRate;
	fmtBasic.wBitsPerSample  = fmt.Samples.wValidBitsPerSample;
	fmtBasic.nBlockAlign     = fmtBasic.nChannels * fmtBasic.wBitsPerSample / 8;
	fmtBasic.nAvgBytesPerSec = fmtBasic.nBlockAlign * fmtBasic.nSamplesPerSec;

	return fmt;
}
