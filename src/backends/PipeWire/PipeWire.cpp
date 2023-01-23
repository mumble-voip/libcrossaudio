// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "PipeWire.hpp"

#include "crossaudio/Macros.h"

#include <memory>

#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/dict.h>

#include <pipewire/core.h>
#include <pipewire/keys.h>
#include <pipewire/stream.h>

using FluxData = CrossAudio_FluxData;

static constexpr pw_stream_events eventsInput  = { PW_VERSION_STREAM_EVENTS,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   processInput,
												   nullptr,
												   nullptr,
												   nullptr };
static constexpr pw_stream_events eventsOutput = { PW_VERSION_STREAM_EVENTS,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   processOutput,
												   nullptr,
												   nullptr,
												   nullptr };

static Library lib;

const char *name() {
	return "PipeWire";
}

const char *version() {
	return lib.get_library_version();
}

ErrorCode init() {
	// clang-format off
	constexpr std::string_view names[] = {
		"libpipewire.so",
		"libpipewire.so.0",
		"libpipewire-0.3.so",
		"libpipewire-0.3.so.0"
	};
	// clang-format on

	ErrorCode ret;

	for (const auto name : names) {
		if ((ret = lib.load(name)) != CROSSAUDIO_EC_LIBRARY) {
			break;
		}
	}

	if (lib) {
		lib.init(nullptr, nullptr);
	}

	return ret;
}

ErrorCode deinit() {
	lib.deinit();
	lib.unload();

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
constexpr BE_Impl PipeWire_Impl = {
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

BE_Engine::BE_Engine() : m_threadLoop(nullptr), m_context(nullptr), m_core(nullptr) {
	if ((m_threadLoop = lib.thread_loop_new(nullptr, nullptr))) {
		m_context = lib.context_new(lib.thread_loop_get_loop(m_threadLoop), nullptr, 0);
	}
}

BE_Engine::~BE_Engine() {
	if (m_context) {
		const auto lock = locker();
		lib.context_destroy(m_context);
	}

	if (m_threadLoop) {
		lib.thread_loop_destroy(m_threadLoop);
	}
}

void BE_Engine::lock() {
	if (m_threadLoop) {
		lib.thread_loop_lock(m_threadLoop);
	}
}

void BE_Engine::unlock() {
	if (m_threadLoop) {
		lib.thread_loop_unlock(m_threadLoop);
	}
}

ErrorCode BE_Engine::start() {
	if (m_core) {
		return CROSSAUDIO_EC_INIT;
	}

	if (m_core = lib.context_connect(m_context, nullptr, 0); !m_core) {
		return CROSSAUDIO_EC_CONNECT;
	}

	if (lib.thread_loop_start(m_threadLoop) < 0) {
		lib.core_disconnect(m_core);
		m_core = nullptr;

		return CROSSAUDIO_EC_GENERIC;
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode BE_Engine::stop() {
	if (m_core) {
		const auto lock = locker();
		lib.core_disconnect(m_core);
		m_core = nullptr;
	}

	return lib.thread_loop_stop(m_threadLoop) >= 0 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

const char *BE_Engine::nameGet() const {
	if (const auto props = m_core ? lib.core_get_properties(m_core) : lib.context_get_properties(m_context)) {
		return lib.properties_get(props, PW_KEY_APP_NAME);
	}

	return nullptr;
}

ErrorCode BE_Engine::nameSet(const char *name) {
	const spa_dict_item items[] = { { PW_KEY_APP_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = locker();

	auto ret = lib.context_update_properties(m_context, &dict);
	if (ret >= 1 && m_core) {
		ret = lib.core_update_properties(m_core, &dict);
	}

	return ret >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

BE_Flux::BE_Flux(BE_Engine &engine) : m_engine(engine), m_stream(nullptr), m_frameSize(0) {
	if (engine.m_core) {
		const auto lock = engine.locker();
		m_stream        = lib.stream_new(engine.m_core, nullptr, nullptr);
	}
}

BE_Flux::~BE_Flux() {
	if (m_stream) {
		const auto lock = m_engine.locker();
		lib.stream_destroy(m_stream);
	}
}

ErrorCode BE_Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	if (lib.stream_get_state(m_stream, nullptr) != PW_STREAM_STATE_UNCONNECTED) {
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

	m_frameSize = sizeof(float) * config.channels;

	auto info = configToInfo(config);

	std::byte buffer[1024];
	spa_pod_builder b     = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	const spa_pod *params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

	const spa_dict_item items[] = { { PW_KEY_MEDIA_TYPE, "Audio" },
									{ PW_KEY_MEDIA_CATEGORY,
									  direction == PW_DIRECTION_INPUT ? "Capture" : "Playback" } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = m_engine.locker();

	lib.stream_update_properties(m_stream, &dict);
	lib.stream_add_listener(m_stream, &m_listener, direction == PW_DIRECTION_INPUT ? &eventsInput : &eventsOutput,
							this);
	lib.stream_connect(m_stream, direction, PW_ID_ANY,
					   PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS, &params, 1);

	return CROSSAUDIO_EC_OK;
}

ErrorCode BE_Flux::stop() {
	const auto lock = m_engine.locker();

	lib.stream_disconnect(m_stream);
	spa_hook_remove(&m_listener);

	return CROSSAUDIO_EC_OK;
}

const char *BE_Flux::nameGet() const {
	if (const auto props = lib.stream_get_properties(m_stream)) {
		return lib.properties_get(props, PW_KEY_NODE_NAME);
	}

	return nullptr;
}

ErrorCode BE_Flux::nameSet(const char *name) {
	const spa_dict_item items[] = { { PW_KEY_NODE_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = m_engine.locker();

	return lib.stream_update_properties(m_stream, &dict) >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

static void processInput(void *userData) {
	auto &flux = *static_cast< BE_Flux * >(userData);

	pw_buffer *buf = lib.stream_dequeue_buffer(flux.m_stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->chunk->size / data->chunk->stride };

	flux.m_feedback.process(flux.m_feedback.userData, &fluxData);

	lib.stream_queue_buffer(flux.m_stream, buf);
}

static void processOutput(void *userData) {
	auto &flux = *static_cast< BE_Flux * >(userData);

	pw_buffer *buf = lib.stream_dequeue_buffer(flux.m_stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->maxsize / flux.m_frameSize };

	flux.m_feedback.process(flux.m_feedback.userData, &fluxData);

	data->chunk->size   = fluxData.frames * flux.m_frameSize;
	data->chunk->stride = flux.m_frameSize;

	lib.stream_queue_buffer(flux.m_stream, buf);
}

static inline spa_audio_info_raw configToInfo(const FluxConfig &config) {
	spa_audio_info_raw info = {};

	info.channels = config.channels;
	info.format   = SPA_AUDIO_FORMAT_F32;
	info.rate     = config.sampleRate;

	for (uint8_t i = 0; i < SPA_MIN(config.channels, CROSSAUDIO_ARRAY_SIZE(config.position)); ++i) {
		spa_audio_channel channel;

		switch (config.position[i]) {
			default:
			case CROSSAUDIO_CH_NONE:
				channel = SPA_AUDIO_CHANNEL_UNKNOWN;
				break;
			case CROSSAUDIO_CH_FRONT_LEFT:
				channel = SPA_AUDIO_CHANNEL_FL;
				break;
			case CROSSAUDIO_CH_FRONT_RIGHT:
				channel = SPA_AUDIO_CHANNEL_FR;
				break;
			case CROSSAUDIO_CH_FRONT_CENTER:
				channel = SPA_AUDIO_CHANNEL_FC;
				break;
			case CROSSAUDIO_CH_LOW_FREQUENCY:
				channel = SPA_AUDIO_CHANNEL_LFE;
				break;
			case CROSSAUDIO_CH_REAR_LEFT:
				channel = SPA_AUDIO_CHANNEL_RL;
				break;
			case CROSSAUDIO_CH_REAR_RIGHT:
				channel = SPA_AUDIO_CHANNEL_RR;
				break;
			case CROSSAUDIO_CH_FRONT_LEFT_CENTER:
				channel = SPA_AUDIO_CHANNEL_FLC;
				break;
			case CROSSAUDIO_CH_FRONT_RIGHT_CENTER:
				channel = SPA_AUDIO_CHANNEL_FRC;
				break;
			case CROSSAUDIO_CH_REAR_CENTER:
				channel = SPA_AUDIO_CHANNEL_RC;
				break;
			case CROSSAUDIO_CH_SIDE_LEFT:
				channel = SPA_AUDIO_CHANNEL_SL;
				break;
			case CROSSAUDIO_CH_SIDE_RIGHT:
				channel = SPA_AUDIO_CHANNEL_SR;
				break;
			case CROSSAUDIO_CH_TOP_CENTER:
				channel = SPA_AUDIO_CHANNEL_TC;
				break;
			case CROSSAUDIO_CH_TOP_FRONT_LEFT:
				channel = SPA_AUDIO_CHANNEL_TFL;
				break;
			case CROSSAUDIO_CH_TOP_FRONT_CENTER:
				channel = SPA_AUDIO_CHANNEL_TFC;
				break;
			case CROSSAUDIO_CH_TOP_FRONT_RIGHT:
				channel = SPA_AUDIO_CHANNEL_TFR;
				break;
			case CROSSAUDIO_CH_TOP_REAR_LEFT:
				channel = SPA_AUDIO_CHANNEL_TRL;
				break;
			case CROSSAUDIO_CH_TOP_REAR_CENTER:
				channel = SPA_AUDIO_CHANNEL_TRC;
				break;
			case CROSSAUDIO_CH_TOP_REAR_RIGHT:
				channel = SPA_AUDIO_CHANNEL_TRR;
				break;
		}

		info.position[i] = channel;
	}

	return info;
}
