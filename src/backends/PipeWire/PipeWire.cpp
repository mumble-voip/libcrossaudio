// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "PipeWire.hpp"

#include "crossaudio/Macros.h"

#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/dict.h>

#include <pipewire/core.h>
#include <pipewire/keys.h>
#include <pipewire/stream.h>

typedef struct CrossAudio_FluxData FluxData;

typedef enum spa_audio_channel spa_audio_channel;
typedef enum spa_direction pw_direction;

typedef struct spa_data spa_data;
typedef struct spa_dict_item spa_dict_item;
typedef struct spa_pod_builder spa_pod_builder;

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
		lib.init(NULL, NULL);
	}

	return ret;
}

ErrorCode deinit() {
	lib.deinit();
	lib.unload();

	return CROSSAUDIO_EC_OK;
}

BE_Engine *engineNew() {
	pw_thread_loop *threadLoop = lib.thread_loop_new(NULL, NULL);
	if (!threadLoop) {
		return NULL;
	}

	pw_context *context = lib.context_new(lib.thread_loop_get_loop(threadLoop), NULL, 0);
	if (!context) {
		lib.thread_loop_destroy(threadLoop);
		return NULL;
	}

	auto engine = new BE_Engine();

	engine->threadLoop = threadLoop;
	engine->context    = context;

	return engine;
}

ErrorCode engineFree(BE_Engine *engine) {
	engineLock(engine);
	lib.context_destroy(engine->context);
	engineUnlock(engine);

	lib.thread_loop_destroy(engine->threadLoop);

	delete engine;

	return CROSSAUDIO_EC_OK;
}

ErrorCode engineStart(BE_Engine *engine) {
	if (engine->core) {
		return CROSSAUDIO_EC_INIT;
	}

	pw_core *core = lib.context_connect(engine->context, NULL, 0);
	if (!core) {
		return CROSSAUDIO_EC_CONNECT;
	}

	if (lib.thread_loop_start(engine->threadLoop) < 0) {
		lib.core_disconnect(core);
		return CROSSAUDIO_EC_GENERIC;
	}

	engine->core = core;

	return CROSSAUDIO_EC_OK;
}

ErrorCode engineStop(BE_Engine *engine) {
	if (engine->core) {
		engineLock(engine);
		lib.core_disconnect(engine->core);
		engineUnlock(engine);

		engine->core = NULL;
	}

	return lib.thread_loop_stop(engine->threadLoop) >= 0 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

const char *engineNameGet(BE_Engine *engine) {
	const pw_properties *props =
		engine->core ? lib.core_get_properties(engine->core) : lib.context_get_properties(engine->context);
	if (!props) {
		return NULL;
	}

	return lib.properties_get(props, PW_KEY_APP_NAME);
}

ErrorCode engineNameSet(BE_Engine *engine, const char *name) {
	const spa_dict_item items[] = { { PW_KEY_APP_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	engineLock(engine);
	int ret = lib.context_update_properties(engine->context, &dict);
	if (ret < 1) {
		goto RET;
	}

	if (engine->core) {
		ret = lib.core_update_properties(engine->core, &dict);
	}
RET:
	engineUnlock(engine);

	return ret >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

BE_Flux *fluxNew(BE_Engine *engine) {
	if (!engine->core) {
		return NULL;
	}

	engineLock(engine);
	pw_stream *stream = lib.stream_new(engine->core, NULL, NULL);
	engineUnlock(engine);

	if (!stream) {
		return NULL;
	}

	auto flux = new BE_Flux();

	flux->engine = engine;
	flux->stream = stream;

	return flux;
}

ErrorCode fluxFree(BE_Flux *flux) {
	engineLock(flux->engine);
	lib.stream_destroy(flux->stream);
	engineUnlock(flux->engine);

	delete flux;

	return CROSSAUDIO_EC_OK;
}

ErrorCode fluxStart(BE_Flux *flux, FluxConfig *config, const FluxFeedback *feedback) {
	if (lib.stream_get_state(flux->stream, NULL) != PW_STREAM_STATE_UNCONNECTED) {
		return CROSSAUDIO_EC_INIT;
	}

	if (feedback) {
		flux->feedback = *feedback;
	}

	pw_direction direction;

	switch (config->direction) {
		case CROSSAUDIO_DIR_IN:
			direction = PW_DIRECTION_INPUT;
			break;
		case CROSSAUDIO_DIR_OUT:
			direction = PW_DIRECTION_OUTPUT;
			break;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	flux->frameSize = sizeof(float) * config->channels;

	spa_audio_info_raw info = configToInfo(config);

	uint8_t buffer[1024];
	spa_pod_builder b     = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	const spa_pod *params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

	const spa_dict_item items[] = { { PW_KEY_MEDIA_TYPE, "Audio" },
									{ PW_KEY_MEDIA_CATEGORY,
									  direction == PW_DIRECTION_INPUT ? "Capture" : "Playback" } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	engineLock(flux->engine);

	lib.stream_update_properties(flux->stream, &dict);
	lib.stream_add_listener(flux->stream, &flux->listener,
							direction == PW_DIRECTION_INPUT ? &eventsInput : &eventsOutput, flux);
	lib.stream_connect(flux->stream, direction, PW_ID_ANY,
					   PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS, &params, 1);

	engineUnlock(flux->engine);

	return CROSSAUDIO_EC_OK;
}

ErrorCode fluxStop(BE_Flux *flux) {
	engineLock(flux->engine);

	lib.stream_disconnect(flux->stream);
	spa_hook_remove(&flux->listener);

	engineUnlock(flux->engine);

	return CROSSAUDIO_EC_OK;
}

const char *fluxNameGet(BE_Flux *flux) {
	const pw_properties *props = lib.stream_get_properties(flux->stream);
	if (!props) {
		return NULL;
	}

	return lib.properties_get(props, PW_KEY_NODE_NAME);
}

ErrorCode fluxNameSet(BE_Flux *flux, const char *name) {
	const spa_dict_item items[] = { { PW_KEY_NODE_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	engineLock(flux->engine);
	const int ret = lib.stream_update_properties(flux->stream, &dict);
	engineUnlock(flux->engine);

	return ret >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
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

static inline void engineLock(BE_Engine *engine) {
	lib.thread_loop_lock(engine->threadLoop);
}

static inline void engineUnlock(BE_Engine *engine) {
	lib.thread_loop_unlock(engine->threadLoop);
}

static void processInput(void *userData) {
	auto &flux = *static_cast< BE_Flux * >(userData);

	pw_buffer *buf = lib.stream_dequeue_buffer(flux.stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->chunk->size / data->chunk->stride };

	flux.feedback.process(flux.feedback.userData, &fluxData);

	lib.stream_queue_buffer(flux.stream, buf);
}

static void processOutput(void *userData) {
	auto &flux = *static_cast< BE_Flux * >(userData);

	pw_buffer *buf = lib.stream_dequeue_buffer(flux.stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->maxsize / flux.frameSize };

	flux.feedback.process(flux.feedback.userData, &fluxData);

	data->chunk->size   = fluxData.frames * flux.frameSize;
	data->chunk->stride = flux.frameSize;

	lib.stream_queue_buffer(flux.stream, buf);
}

static inline spa_audio_info_raw configToInfo(const FluxConfig *config) {
	spa_audio_info_raw info = {};

	info.channels = config->channels;
	info.format   = SPA_AUDIO_FORMAT_F32;
	info.rate     = config->sampleRate;

	for (uint8_t i = 0; i < SPA_MIN(config->channels, CROSSAUDIO_ARRAY_SIZE(config->position)); ++i) {
		spa_audio_channel channel;

		switch (config->position[i]) {
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
