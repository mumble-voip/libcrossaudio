// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "PipeWire.h"

#include "Backend.h"

#include "crossaudio/Macros.h"

#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/dict.h>

#include <pipewire/core.h>
#include <pipewire/keys.h>
#include <pipewire/stream.h>

#define LOAD_SYM(var)                                    \
	*(void **) &lib.var = dlsym(lib.handle, "pw_" #var); \
	if (!lib.var) {                                      \
		UNLOAD_LIB                                       \
		return CROSSAUDIO_EC_SYMBOL;                     \
	}

#define UNLOAD_LIB       \
	dlclose(lib.handle); \
	memset(&lib, 0, sizeof(lib));

typedef struct CrossAudio_FluxData FluxData;

typedef enum spa_audio_channel spa_audio_channel;
typedef enum spa_direction pw_direction;

typedef struct spa_data spa_data;
typedef struct spa_dict_item spa_dict_item;
typedef struct spa_pod_builder spa_pod_builder;

static const pw_stream_events eventsInput  = { .version = PW_VERSION_STREAM_EVENTS, .process = processInput };
static const pw_stream_events eventsOutput = { .version = PW_VERSION_STREAM_EVENTS, .process = processOutput };

static Library lib;

const char *name() {
	return "PipeWire";
}

const char *version() {
	return lib.get_library_version();
}

ErrorCode init() {
	// clang-format off
	const char *names[] = {
		"libpipewire.so",
		"libpipewire.so.0",
		"libpipewire-0.3.so",
		"libpipewire-0.3.so.0"
	};
	// clang-format on

	for (uint8_t i = 0; i < CROSSAUDIO_ARRAY_SIZE(names); ++i) {
		if ((lib.handle = dlopen(names[i], RTLD_LAZY))) {
			break;
		}
	}

	if (!lib.handle) {
		return CROSSAUDIO_EC_LIBRARY;
	}

	LOAD_SYM(get_library_version)

	LOAD_SYM(init)
	LOAD_SYM(deinit)

	LOAD_SYM(context_new)
	LOAD_SYM(context_destroy)
	LOAD_SYM(context_connect)
	LOAD_SYM(context_get_properties)
	LOAD_SYM(context_update_properties)

	LOAD_SYM(core_disconnect)
	LOAD_SYM(core_get_properties)
	LOAD_SYM(core_update_properties)

	LOAD_SYM(properties_get)

	LOAD_SYM(stream_new)
	LOAD_SYM(stream_destroy)
	LOAD_SYM(stream_connect)
	LOAD_SYM(stream_disconnect)
	LOAD_SYM(stream_dequeue_buffer)
	LOAD_SYM(stream_queue_buffer)
	LOAD_SYM(stream_get_properties)
	LOAD_SYM(stream_update_properties)
	LOAD_SYM(stream_get_state)
	LOAD_SYM(stream_add_listener)

	LOAD_SYM(thread_loop_new)
	LOAD_SYM(thread_loop_destroy)
	LOAD_SYM(thread_loop_lock)
	LOAD_SYM(thread_loop_unlock)
	LOAD_SYM(thread_loop_start)
	LOAD_SYM(thread_loop_stop)
	LOAD_SYM(thread_loop_get_loop)

	lib.init(NULL, NULL);

	return CROSSAUDIO_EC_OK;
}

ErrorCode deinit() {
	lib.deinit();

	UNLOAD_LIB

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

	BE_Engine *engine = CROSSAUDIO_ZERO_ALLOC(sizeof(*engine));
	if (!engine) {
		lib.context_destroy(context);
		lib.thread_loop_destroy(threadLoop);
		return NULL;
	}

	engine->threadLoop = threadLoop;
	engine->context    = context;

	return engine;
}

ErrorCode engineFree(BE_Engine *engine) {
	engineLock(engine);
	lib.context_destroy(engine->context);
	engineUnlock(engine);

	lib.thread_loop_destroy(engine->threadLoop);

	free(engine);

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
	const spa_dict_item items[] = { SPA_DICT_ITEM_INIT(PW_KEY_APP_NAME, name) };
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
	if (!stream) {
		engineUnlock(engine);
		return NULL;
	}

	BE_Flux *flux = CROSSAUDIO_ZERO_ALLOC(sizeof(*flux));
	if (!flux) {
		lib.stream_destroy(stream);
		goto RET;
	}

	flux->engine = engine;
	flux->stream = stream;
RET:
	engineUnlock(engine);

	return flux;
}

ErrorCode fluxFree(BE_Flux *flux) {
	engineLock(flux->engine);
	lib.stream_destroy(flux->stream);
	engineUnlock(flux->engine);

	free(flux);

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

	const spa_dict_item items[] = { SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_TYPE, "Audio"),
									SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_CATEGORY,
													   direction == PW_DIRECTION_INPUT ? "Capture" : "Playback") };

	engineLock(flux->engine);

	lib.stream_update_properties(flux->stream, &SPA_DICT_INIT_ARRAY(items));
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
	const spa_dict_item items[] = { SPA_DICT_ITEM_INIT(PW_KEY_NODE_NAME, name) };

	engineLock(flux->engine);
	const int ret = lib.stream_update_properties(flux->stream, &SPA_DICT_INIT_ARRAY(items));
	engineUnlock(flux->engine);

	return ret >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

// clang-format off
const BE_Impl PipeWire_Impl = {
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
	BE_Flux *flux = userData;

	pw_buffer *buf = lib.stream_dequeue_buffer(flux->stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { .data = data->data, .frames = data->chunk->size / data->chunk->stride };

	flux->feedback.process(flux->feedback.userData, &fluxData);

	lib.stream_queue_buffer(flux->stream, buf);
}

static void processOutput(void *userData) {
	BE_Flux *flux = userData;

	pw_buffer *buf = lib.stream_dequeue_buffer(flux->stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { .data = data->data, .frames = data->maxsize / flux->frameSize };

	flux->feedback.process(flux->feedback.userData, &fluxData);

	data->chunk->size   = fluxData.frames * flux->frameSize;
	data->chunk->stride = flux->frameSize;

	lib.stream_queue_buffer(flux->stream, buf);
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
