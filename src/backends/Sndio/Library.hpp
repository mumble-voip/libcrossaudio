// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_SNDIO_LIBRARY_HPP
#define CROSSAUDIO_SRC_BACKENDS_SNDIO_LIBRARY_HPP

#include "crossaudio/ErrorCode.h"

#include <cstddef>
#include <string_view>

using ErrorCode = CrossAudio_ErrorCode;

typedef struct pollfd PollFD;

struct sio_hdl;
struct sio_par;

namespace sndio {
class Library {
public:
	Library();
	~Library();

	explicit operator bool() const { return m_handle; }

	ErrorCode load(const std::string_view libraryName);
	void unload();

	void *m_handle;

	sio_hdl *(*open)(const char *name, unsigned int mode, int nbio_flag);
	void (*close)(sio_hdl *hdl);

	int (*start)(sio_hdl *hdl);
	int (*stop)(sio_hdl *hdl);

	size_t (*read)(sio_hdl *hdl, void *addr, size_t nbytes);
	size_t (*write)(sio_hdl *hdl, const void *addr, size_t nbytes);

	void (*initpar)(sio_par *par);
	int (*getpar)(sio_hdl *hdl, sio_par *par);
	int (*setpar)(sio_hdl *hdl, sio_par *par);

	int (*nfds)(sio_hdl *hdl);
	int (*pollfd)(sio_hdl *hdl, PollFD *pfd, int events);
	int (*revents)(sio_hdl *hdl, PollFD *pfd);

private:
	Library(const Library &)            = delete;
	Library &operator=(const Library &) = delete;
};
} // namespace sndio

#endif
