# Copyright 2022 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

include(CrossAudioCompiler)

add_library(crossaudio SHARED)

target_pedantic_warnings(crossaudio)

set_target_properties(crossaudio
	PROPERTIES
		ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
		LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
		RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)

target_compile_definitions(crossaudio
	PRIVATE
		"CROSSAUDIO_BUILD"
)

target_include_directories(crossaudio
	PUBLIC
		${INCLUDE_DIR}
)

target_sources(crossaudio
	PRIVATE
		"Backend.c"
		"Backend.h"
		"Engine.c"
		"Engine.h"
		"Flux.c"
		"Flux.h"
		"Macros.h"
	PUBLIC
		"${INCLUDE_DIR}/crossaudio/Backend.h"
		"${INCLUDE_DIR}/crossaudio/Channel.h"
		"${INCLUDE_DIR}/crossaudio/Direction.h"
		"${INCLUDE_DIR}/crossaudio/Engine.h"
		"${INCLUDE_DIR}/crossaudio/ErrorCode.h"
		"${INCLUDE_DIR}/crossaudio/Flux.h"
)

add_subdirectory(backends)

target_link_libraries(crossaudio
	PRIVATE
		$<TARGET_NAME_IF_EXISTS:be_pipewire>
)
