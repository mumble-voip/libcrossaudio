# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	set(CROSSAUDIO_BACKEND_OSS_4FRONT ON)
endif()

option(CROSSAUDIO_BACKEND_OSS_4FRONT "Use 4Front Technologies' API instead of system's" OFF)

include(CrossAudioCompiler)

find_package(Threads)

add_library(be_oss OBJECT)

target_pedantic_warnings(be_oss)

target_compile_definitions(be_oss
	INTERFACE
		"HAS_OSS"
)

target_include_directories(be_oss
	PRIVATE
		${SRC_DIR}
		${INCLUDE_DIR}
)

if(CROSSAUDIO_BACKEND_OSS_4FRONT)
	target_include_directories(be_oss
		SYSTEM PRIVATE
			"${3RDPARTY_DIR}/OSS"
	)
endif()

target_sources(be_oss
	PRIVATE
		"FileDescriptor.cpp"
		"FileDescriptor.hpp"
		"OSS.cpp"
		"OSS.hpp"
)

target_link_libraries(be_oss
	PRIVATE
		Threads::Threads
)
