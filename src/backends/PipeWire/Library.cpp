// Copyright 2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Library.hpp"

#include <dlfcn.h>

#define LOAD_SYM(var)                            \
	*(void **) &var = dlsym(handle, "pw_" #var); \
	if (!var) {                                  \
		unload();                                \
		return CROSSAUDIO_EC_SYMBOL;             \
	}

Library::Library() : handle(nullptr) {
}

Library::~Library() {
	unload();
}

ErrorCode Library::load(const std::string_view libraryName) {
	if (!(handle = dlopen(libraryName.data(), RTLD_LAZY))) {
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

	return CROSSAUDIO_EC_OK;
}

void Library::unload() {
	if (handle) {
		dlclose(handle);
		handle = nullptr;
	}
}
