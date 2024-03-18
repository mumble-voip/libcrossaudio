// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "FileDescriptor.hpp"

#include <utility>

#include <unistd.h>

FileDescriptor::FileDescriptor() : m_handle(-1) {
}

FileDescriptor::FileDescriptor(FileDescriptor &&fd) : m_handle(std::exchange(fd.m_handle, -1)) {
}

FileDescriptor::FileDescriptor(const fd_t handle) : m_handle(handle) {
}

FileDescriptor::~FileDescriptor() {
	close();
}

FileDescriptor &FileDescriptor::operator=(FileDescriptor &&fd) {
	m_handle = std::exchange(fd.m_handle, -1);
	return *this;
}

void FileDescriptor::close() {
	if (*this) {
		::close(m_handle);
		m_handle = -1;
	}
}
