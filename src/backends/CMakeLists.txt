# Copyright 2022-2023 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

if(UNIX)
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
		add_subdirectory(ALSA)
	endif()

	add_subdirectory(OSS)
	add_subdirectory(PipeWire)
	add_subdirectory(PulseAudio)
endif()

if(WIN32)
	add_subdirectory(WASAPI)
endif()
