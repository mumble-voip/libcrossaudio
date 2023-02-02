# Copyright 2022-2023 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

include(CrossAudioCompiler)

add_library(be_pipewire OBJECT)

target_pedantic_warnings(be_pipewire)

target_compile_definitions(be_pipewire
	PRIVATE
		# For POSIX.1-2008 locale stuff in <spa/utils/string.h>.
		"_XOPEN_SOURCE=700"
	INTERFACE
		"HAS_PIPEWIRE"
)

target_compile_options(be_pipewire
	PRIVATE
		# The following warnings are disabled because caused by macros in PipeWire's headers.
		"-Wno-c99-extensions"
		"-Wno-gnu-statement-expression"
)

target_include_directories(be_pipewire
	PRIVATE
		${SRC_DIR}
		${INCLUDE_DIR}
)

target_include_directories(be_pipewire
	SYSTEM PRIVATE
		"${3RDPARTY_DIR}/PipeWire"
)

target_sources(be_pipewire
	PRIVATE
		"Library.cpp"
		"Library.hpp"
		"PipeWire.cpp"
		"PipeWire.hpp"
)
