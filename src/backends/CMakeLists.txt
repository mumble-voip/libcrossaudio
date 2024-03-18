# Copyright 2022-2023 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

if(UNIX)
	add_subdirectory(OSS)
	add_subdirectory(PipeWire)
endif()

if(WIN32)
	add_subdirectory(WASAPI)
endif()
