// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PULSEAUDIO_LIBRARY_HPP
#define CROSSAUDIO_SRC_BACKENDS_PULSEAUDIO_LIBRARY_HPP

#include "crossaudio/ErrorCode.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <pulse/introspect.h>
#include <pulse/stream.h>
#include <pulse/subscribe.h>

using ErrorCode = CrossAudio_ErrorCode;

struct pa_threaded_mainloop;

namespace pulseaudio {
class Library {
public:
	Library();
	~Library();

	explicit operator bool() const { return m_handle; }

	ErrorCode load(const std::string_view libraryName);
	void unload();

	void *m_handle;

	const char *(*get_library_version)();

	void (*operation_unref)(pa_operation *o);

	pa_context *(*context_new_with_proplist)(pa_mainloop_api *mainloop, const char *name, const pa_proplist *proplist);
	void (*context_unref)(pa_context *c);
	int (*context_connect)(pa_context *c, const char *server, pa_context_flags_t flags, const pa_spawn_api *api);
	void (*context_disconnect)(pa_context *c);
	pa_operation *(*context_subscribe)(pa_context *c, pa_subscription_mask_t m, pa_context_success_cb_t cb,
									   void *userdata);
	pa_context_state_t (*context_get_state)(const pa_context *c);
	pa_operation *(*context_get_server_info)(pa_context *c, pa_server_info_cb_t cb, void *userdata);
	pa_operation *(*context_get_sink_info_by_index)(pa_context *c, uint32_t idx, pa_sink_info_cb_t cb, void *userdata);
	pa_operation *(*context_get_source_info_by_index)(pa_context *c, uint32_t idx, pa_source_info_cb_t cb,
													  void *userdata);
	pa_operation *(*context_get_sink_info_list)(pa_context *c, pa_sink_info_cb_t cb, void *userdata);
	pa_operation *(*context_get_source_info_list)(pa_context *c, pa_source_info_cb_t cb, void *userdata);
	pa_operation *(*context_set_name)(pa_context *c, const char *name, pa_context_success_cb_t cb, void *userdata);
	void (*context_set_state_callback)(pa_context *c, pa_context_notify_cb_t cb, void *userdata);
	void (*context_set_subscribe_callback)(pa_context *c, pa_context_subscribe_cb_t cb, void *userdata);

	pa_proplist *(*proplist_new)();
	void (*proplist_free)(pa_proplist *p);
	int (*proplist_sets)(pa_proplist *p, const char *key, const char *value);

	pa_stream *(*stream_new)(pa_context *c, const char *name, const pa_sample_spec *ss, const pa_channel_map *map);
	void (*stream_unref)(pa_stream *s);
	int (*stream_connect_playback)(pa_stream *s, const char *dev, const pa_buffer_attr *attr, pa_stream_flags_t flags,
								   const pa_cvolume *volume, pa_stream *sync_stream);
	int (*stream_connect_record)(pa_stream *s, const char *dev, const pa_buffer_attr *attr, pa_stream_flags_t flags);
	int (*stream_disconnect)(pa_stream *s);
	pa_operation *(*stream_cork)(pa_stream *s, int b, pa_stream_success_cb_t cb, void *userdata);
	int (*stream_peek)(pa_stream *p, const void **data, size_t *nbytes);
	int (*stream_begin_write)(pa_stream *p, void **data, size_t *nbytes);
	int (*stream_write)(pa_stream *p, const void *data, size_t nbytes, pa_free_cb_t free_cb, int64_t offset,
						pa_seek_mode_t seek);
	int (*stream_drop)(pa_stream *p);
	pa_operation *(*stream_set_name)(pa_stream *s, const char *name, pa_stream_success_cb_t cb, void *userdata);
	void (*stream_set_read_callback)(pa_stream *p, pa_stream_request_cb_t cb, void *userdata);
	void (*stream_set_write_callback)(pa_stream *p, pa_stream_request_cb_t cb, void *userdata);

	pa_threaded_mainloop *(*threaded_mainloop_new)();
	void (*threaded_mainloop_free)(pa_threaded_mainloop *m);
	void (*threaded_mainloop_lock)(pa_threaded_mainloop *m);
	void (*threaded_mainloop_unlock)(pa_threaded_mainloop *m);
	int (*threaded_mainloop_start)(pa_threaded_mainloop *m);
	void (*threaded_mainloop_stop)(pa_threaded_mainloop *m);
	pa_mainloop_api *(*threaded_mainloop_get_api)(pa_threaded_mainloop *m);

private:
	Library(const Library &)            = delete;
	Library &operator=(const Library &) = delete;
};
} // namespace pulseaudio

#endif
