// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Library.hpp"

#include <dlfcn.h>

#define LOAD_SYM(var)                               \
	*(void **) &var = dlsym(m_handle, "sio_" #var); \
	if (!var) {                                     \
		unload();                                   \
		return CROSSAUDIO_EC_SYMBOL;                \
	}

using namespace sndio;

Library::Library() : m_handle(nullptr) {
}

Library::~Library() {
	unload();
}

ErrorCode Library::load(const std::string_view libraryName) {
	if (!(m_handle = dlopen(libraryName.data(), RTLD_LAZY))) {
		return CROSSAUDIO_EC_LIBRARY;
	}

	LOAD_SYM(open)
	LOAD_SYM(close)

	LOAD_SYM(start)
	LOAD_SYM(stop)

	LOAD_SYM(read)
	LOAD_SYM(write)

	LOAD_SYM(initpar)
	LOAD_SYM(getpar)
	LOAD_SYM(setpar)

	LOAD_SYM(nfds)
	LOAD_SYM(pollfd)
	LOAD_SYM(revents)

	return CROSSAUDIO_EC_OK;
}

void Library::unload() {
	if (m_handle) {
		dlclose(m_handle);
		m_handle = nullptr;
	}
}
