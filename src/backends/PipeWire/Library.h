// Copyright 2022 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_LIBRARY_H
#define CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#include <pipewire/stream.h>

typedef enum pw_stream_state pw_stream_state;

typedef struct spa_dict spa_dict;
typedef struct spa_hook spa_hook;
typedef struct spa_pod spa_pod;

typedef struct pw_buffer pw_buffer;
typedef struct pw_context pw_context;
typedef struct pw_core pw_core;
typedef struct pw_loop pw_loop;
typedef struct pw_properties pw_properties;
typedef struct pw_stream pw_stream;
typedef struct pw_stream_events pw_stream_events;
typedef struct pw_thread_loop pw_thread_loop;

typedef struct Library {
	void *handle;

	const char *(*get_library_version)();

	void (*init)(int *argc, char **argv[]);
	void (*deinit)();

	pw_context *(*context_new)(pw_loop *main_loop, pw_properties *props, size_t user_data_size);
	void (*context_destroy)(pw_context *context);
	pw_core *(*context_connect)(pw_context *context, pw_properties *properties, size_t user_data_size);
	const pw_properties *(*context_get_properties)(pw_context *context);
	int (*context_update_properties)(pw_context *context, const spa_dict *dict);

	int (*core_disconnect)(pw_core *core);
	const pw_properties *(*core_get_properties)(pw_core *core);
	int (*core_update_properties)(pw_core *core, const spa_dict *dict);

	const char *(*properties_get)(const pw_properties *properties, const char *key);

	pw_stream *(*stream_new)(pw_core *core, const char *name, pw_properties *props);
	void (*stream_destroy)(pw_stream *stream);
	int (*stream_connect)(pw_stream *stream, uint32_t direction, uint32_t target_id, uint32_t flags,
						  const spa_pod **params, uint32_t n_params);
	int (*stream_disconnect)(pw_stream *stream);
	pw_buffer *(*stream_dequeue_buffer)(pw_stream *stream);
	int (*stream_queue_buffer)(pw_stream *stream, pw_buffer *buffer);
	const pw_properties *(*stream_get_properties)(pw_stream *stream);
	int (*stream_update_properties)(pw_stream *stream, const spa_dict *dict);
	pw_stream_state (*stream_get_state)(pw_stream *stream, const char **error);
	void (*stream_add_listener)(pw_stream *stream, spa_hook *listener, const pw_stream_events *events, void *data);

	pw_thread_loop *(*thread_loop_new)(const char *name, const spa_dict *props);
	void (*thread_loop_destroy)(pw_thread_loop *loop);
	void (*thread_loop_lock)(pw_thread_loop *loop);
	void (*thread_loop_unlock)(pw_thread_loop *loop);
	int (*thread_loop_start)(pw_thread_loop *loop);
	int (*thread_loop_stop)(pw_thread_loop *loop);
	pw_loop *(*thread_loop_get_loop)(pw_thread_loop *loop);
} Library;

#endif
