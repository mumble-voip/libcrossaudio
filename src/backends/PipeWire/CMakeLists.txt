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
		# The following warnings are disabled as they're caused by PipeWire's headers.
		"-Wno-gnu-empty-initializer"
		"-Wno-gnu-statement-expression"
		"-Wno-gnu-zero-variadic-macro-arguments"
)

target_include_directories(be_pipewire
	PRIVATE
		${SRC_DIR}
		${INCLUDE_DIR}

		"${3RDPARTY_DIR}/PipeWire"
)

target_sources(be_pipewire
	PRIVATE
		"Library.h"
		"PipeWire.c"
		"PipeWire.h"
)
