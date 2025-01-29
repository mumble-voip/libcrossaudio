// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Flux.hpp"

#include "Engine.hpp"
#include "Library.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <spa/param/audio/raw-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/dict.h>

#include <pipewire/keys.h>

using namespace pipewire;

typedef CrossAudio_FluxData FluxData;

static constexpr spa_audio_info_raw configToInfo(const FluxConfig &config);
static constexpr spa_audio_format translateFormat(CrossAudio_BitFormat format, uint8_t sampleBits);

static constexpr pw_stream_events eventsInput = {
	PW_VERSION_STREAM_EVENTS, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	Flux::processInput,       nullptr, nullptr, nullptr
};
static constexpr pw_stream_events eventsOutput = {
	PW_VERSION_STREAM_EVENTS, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	Flux::processOutput,      nullptr, nullptr, nullptr
};

Flux::Flux(Engine &engine) : m_engine(engine), m_stream(nullptr), m_frameSize(0) {
	if (engine.m_core) {
		const auto lock = engine.locker();
		m_stream        = lib().stream_new(engine.m_core, nullptr, nullptr);
	}
}

Flux::~Flux() {
	if (m_stream) {
		const auto lock = m_engine.locker();
		lib().stream_destroy(m_stream);
	}
}

ErrorCode Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	if (lib().stream_get_state(m_stream, nullptr) != PW_STREAM_STATE_UNCONNECTED) {
		return CROSSAUDIO_EC_INIT;
	}

	m_feedback = feedback;

	pw_direction direction;

	switch (config.direction) {
		case CROSSAUDIO_DIR_IN:
			direction = PW_DIRECTION_INPUT;
			break;
		case CROSSAUDIO_DIR_OUT:
			direction = PW_DIRECTION_OUTPUT;
			break;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	m_frameSize = config.sampleBits / 8 * config.channels;

	auto info = configToInfo(config);

	std::byte buffer[1024];
	spa_pod_builder b     = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	const spa_pod *params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

	spa_dict_item items[] = { { PW_KEY_MEDIA_TYPE, "Audio" },
							  { PW_KEY_MEDIA_CATEGORY, direction == PW_DIRECTION_INPUT ? "Capture" : "Playback" },
							  { PW_KEY_TARGET_OBJECT, config.node } };
	const spa_dict dict   = SPA_DICT_INIT_ARRAY(items);

	uint32_t flags = PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS;
	if (config.node) {
		flags |= PW_STREAM_FLAG_AUTOCONNECT;
	}

	const auto lock = m_engine.locker();

	lib().stream_update_properties(m_stream, &dict);
	lib().stream_add_listener(m_stream, &m_listener, direction == PW_DIRECTION_INPUT ? &eventsInput : &eventsOutput,
							  this);
	lib().stream_connect(m_stream, direction, PW_ID_ANY, flags, &params, 1);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::stop() {
	const auto lock = m_engine.locker();

	lib().stream_disconnect(m_stream);
	spa_hook_remove(&m_listener);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::pause(const bool on) {
	const auto lock = m_engine.locker();

	lib().stream_set_active(m_stream, !on);

	return CROSSAUDIO_EC_OK;
}

const char *Flux::nameGet() const {
	if (const auto props = lib().stream_get_properties(m_stream)) {
		return lib().properties_get(props, PW_KEY_NODE_NAME);
	}

	return nullptr;
}

ErrorCode Flux::nameSet(const char *name) {
	const spa_dict_item items[] = { { PW_KEY_NODE_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = m_engine.locker();

	return lib().stream_update_properties(m_stream, &dict) >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

void Flux::processInput(void *userData) {
	auto &flux = *static_cast< Flux * >(userData);

	pw_buffer *buf = lib().stream_dequeue_buffer(flux.m_stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->chunk->size / data->chunk->stride };

	flux.m_feedback.process(flux.m_feedback.userData, &fluxData);

	lib().stream_queue_buffer(flux.m_stream, buf);
}

void Flux::processOutput(void *userData) {
	auto &flux = *static_cast< Flux * >(userData);

	pw_buffer *buf = lib().stream_dequeue_buffer(flux.m_stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->maxsize / flux.m_frameSize };

	flux.m_feedback.process(flux.m_feedback.userData, &fluxData);

	if (fluxData.frames) {
		data->chunk->size = fluxData.frames * flux.m_frameSize;
	} else {
		// Telling PipeWire that we wrote 0 bytes results in an xrun,
		// which in turn results in this function being called continuously.
		memset(data->data, 0, data->maxsize);
		data->chunk->size = data->maxsize;
	}

	data->chunk->stride = flux.m_frameSize;

	lib().stream_queue_buffer(flux.m_stream, buf);
}

static constexpr spa_audio_info_raw configToInfo(const FluxConfig &config) {
	spa_audio_info_raw info = {};

	info.format   = translateFormat(config.bitFormat, config.sampleBits);
	info.channels = config.channels;
	info.rate     = config.sampleRate;

	for (uint8_t i = 0; i < SPA_MIN(config.channels, CROSSAUDIO_ARRAY_SIZE(config.position)); ++i) {
		info.position[i] = config.position[i];
	}

	return info;
}

static constexpr spa_audio_format translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits) {
	switch (format) {
		default:
		case CROSSAUDIO_BF_NONE:
			break;
		case CROSSAUDIO_BF_INTEGER_SIGNED:
			switch (sampleBits) {
				case 8:
					return SPA_AUDIO_FORMAT_S8;
				case 16:
					return SPA_AUDIO_FORMAT_S16;
				case 24:
					return SPA_AUDIO_FORMAT_S24_32;
				case 32:
					return SPA_AUDIO_FORMAT_S32;
			}

			break;
		case CROSSAUDIO_BF_INTEGER_UNSIGNED:
			switch (sampleBits) {
				case 8:
					return SPA_AUDIO_FORMAT_U8;
				case 16:
					return SPA_AUDIO_FORMAT_U16;
				case 24:
					return SPA_AUDIO_FORMAT_U24_32;
				case 32:
					return SPA_AUDIO_FORMAT_U32;
			}

			break;
		case CROSSAUDIO_BF_FLOAT:
			switch (sampleBits) {
				case 32:
					return SPA_AUDIO_FORMAT_F32;
				case 64:
					return SPA_AUDIO_FORMAT_F64;
			}

			break;
	}

	return SPA_AUDIO_FORMAT_UNKNOWN;
}
