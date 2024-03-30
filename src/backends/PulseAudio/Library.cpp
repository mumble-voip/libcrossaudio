// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Library.hpp"

#include <dlfcn.h>

#define LOAD_SYM(var)                              \
	*(void **) &var = dlsym(m_handle, "pa_" #var); \
	if (!var) {                                    \
		unload();                                  \
		return CROSSAUDIO_EC_SYMBOL;               \
	}

using namespace pulseaudio;

Library::Library() : m_handle(nullptr) {
}

Library::~Library() {
	unload();
}

ErrorCode Library::load(const std::string_view libraryName) {
	if (!(m_handle = dlopen(libraryName.data(), RTLD_LAZY))) {
		return CROSSAUDIO_EC_LIBRARY;
	}

	LOAD_SYM(get_library_version)

	LOAD_SYM(operation_unref)

	LOAD_SYM(context_new_with_proplist)
	LOAD_SYM(context_unref)
	LOAD_SYM(context_connect)
	LOAD_SYM(context_disconnect)
	LOAD_SYM(context_subscribe)
	LOAD_SYM(context_get_state)
	LOAD_SYM(context_get_server_info)
	LOAD_SYM(context_get_sink_info_by_index)
	LOAD_SYM(context_get_source_info_by_index)
	LOAD_SYM(context_get_sink_info_list)
	LOAD_SYM(context_get_source_info_list)
	LOAD_SYM(context_set_name)
	LOAD_SYM(context_set_state_callback)
	LOAD_SYM(context_set_subscribe_callback)

	LOAD_SYM(proplist_new)
	LOAD_SYM(proplist_free)
	LOAD_SYM(proplist_sets)

	LOAD_SYM(stream_new)
	LOAD_SYM(stream_unref)
	LOAD_SYM(stream_connect_playback)
	LOAD_SYM(stream_connect_record)
	LOAD_SYM(stream_disconnect)
	LOAD_SYM(stream_cork)
	LOAD_SYM(stream_peek)
	LOAD_SYM(stream_begin_write)
	LOAD_SYM(stream_write)
	LOAD_SYM(stream_drop)
	LOAD_SYM(stream_set_name)
	LOAD_SYM(stream_set_read_callback)
	LOAD_SYM(stream_set_write_callback)

	LOAD_SYM(threaded_mainloop_new)
	LOAD_SYM(threaded_mainloop_free)
	LOAD_SYM(threaded_mainloop_lock)
	LOAD_SYM(threaded_mainloop_unlock)
	LOAD_SYM(threaded_mainloop_start)
	LOAD_SYM(threaded_mainloop_stop)
	LOAD_SYM(threaded_mainloop_get_api)

	return CROSSAUDIO_EC_OK;
}

void Library::unload() {
	if (m_handle) {
		dlclose(m_handle);
		m_handle = nullptr;
	}
}
