# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

include(CrossAudioCompiler)

add_library(be_sndio OBJECT)

target_pedantic_warnings(be_sndio)

target_compile_definitions(be_sndio
	INTERFACE
		"HAS_SNDIO"
)

target_include_directories(be_sndio
	PRIVATE
		${SRC_DIR}
		${INCLUDE_DIR}
)

target_include_directories(be_sndio
	SYSTEM PRIVATE
		"${3RDPARTY_DIR}/Sndio"
)

target_sources(be_sndio
	PRIVATE
		"Library.cpp"
		"Library.hpp"
		"Sndio.cpp"
		"Sndio.hpp"
)
