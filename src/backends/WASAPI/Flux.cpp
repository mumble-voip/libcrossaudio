// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Flux.hpp"

#include "Device.hpp"
#include "Engine.hpp"

#include <bit>
#include <cstring>
#include <functional>
#include <thread>

#include <Windows.h>

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>

using namespace wasapi;

typedef CrossAudio_Direction Direction;
typedef CrossAudio_FluxData FluxData;

static constexpr WAVEFORMATEXTENSIBLE configToWaveFormat(const FluxConfig &config);
static FluxConfig waveFormatToConfig(const char *node, const Direction direction, const WAVEFORMATEXTENSIBLE &fmt);

Flux::Flux(Engine &engine)
	: m_engine(engine), m_feedback(), m_device(nullptr), m_client(nullptr),
	  m_event(CreateEvent(nullptr, false, false, nullptr)) {
}

Flux::~Flux() {
	stop();

	if (m_event) {
		CloseHandle(m_event);
	}
}

ErrorCode Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
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
	props.Options   = AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;
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
		case S_OK:
			break;
		case S_FALSE:
			config = waveFormatToConfig(config.node, config.direction, *fmtProposed);
			CoTaskMemFree(fmtProposed);
			return CROSSAUDIO_EC_NEGOTIATE;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	UINT32 framesDef, framesMul, framesMin, framesMax;
	if (m_client->GetSharedModeEnginePeriod(&fmtBasic, &framesDef, &framesMul, &framesMin, &framesMax) != S_OK) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	const auto hr = m_client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, framesDef, &fmtBasic,
														  reinterpret_cast< GUID * >(&m_engine.m_sessionID));
	if (hr != S_OK) {
		stop();
		return hr == E_ACCESSDENIED ? CROSSAUDIO_EC_PERMISSION : CROSSAUDIO_EC_GENERIC;
	}

	m_thread = std::make_unique< std::thread >(threadFunc);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::stop() {
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

ErrorCode Flux::pause(const bool on) {
	if (!m_client) {
		return CROSSAUDIO_EC_INIT;
	}

	switch (on ? m_client->Stop() : m_client->Start()) {
		case S_OK:
		case S_FALSE:
		case AUDCLNT_E_NOT_STOPPED:
			return CROSSAUDIO_EC_OK;
		case AUDCLNT_E_DEVICE_INVALIDATED:
			return CROSSAUDIO_EC_INIT;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}
}

const char *Flux::nameGet() const {
	// TODO: Implement this.
	return nullptr;
}

ErrorCode Flux::nameSet(const char *) {
	// TODO: Implement this.
	return CROSSAUDIO_EC_OK;
}

void Flux::processInput() {
	if (Engine::threadInit() != CROSSAUDIO_EC_OK) {
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

	Engine::threadDeinit();
}

void Flux::processOutput() {
	if (Engine::threadInit() != CROSSAUDIO_EC_OK) {
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
					memset(buffer + fluxData.frames, 0, frames - fluxData.frames);
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

	Engine::threadDeinit();
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
	fmtBasic.wBitsPerSample  = std::bit_ceil(config.sampleBits);
	fmtBasic.nBlockAlign     = fmtBasic.nChannels * fmtBasic.wBitsPerSample / 8;
	fmtBasic.nAvgBytesPerSec = fmtBasic.nBlockAlign * fmtBasic.nSamplesPerSec;

	return fmt;
}

static FluxConfig waveFormatToConfig(const char *node, const Direction direction, const WAVEFORMATEXTENSIBLE &fmt) {
	FluxConfig config{};

	config.node      = node;
	config.direction = direction;

	if (fmt.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
		config.bitFormat = CROSSAUDIO_BF_FLOAT;
	} else if (fmt.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
		config.bitFormat = CROSSAUDIO_BF_INTEGER_SIGNED;
	}

	config.sampleBits = static_cast< decltype(config.sampleBits) >(fmt.Samples.wValidBitsPerSample);

	const auto &fmtBasic = fmt.Format;
	config.sampleRate    = fmtBasic.nSamplesPerSec;
	config.channels      = static_cast< decltype(config.channels) >(fmtBasic.nChannels);
	// TODO: Set full channel mask and adapt positions automatically.
	config.position[0] = CROSSAUDIO_CH_FRONT_LEFT;
	config.position[1] = CROSSAUDIO_CH_FRONT_RIGHT;

	return config;
}
