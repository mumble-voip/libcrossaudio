# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

include(CrossAudioCompiler)

find_package(ALSA REQUIRED)

add_library(be_alsa OBJECT)

target_pedantic_warnings(be_alsa)

target_compile_definitions(be_alsa
	INTERFACE
		"HAS_ALSA"
)

target_include_directories(be_alsa
	PRIVATE
		${SRC_DIR}
		${INCLUDE_DIR}
)

target_sources(be_alsa
	PRIVATE
		"ALSA.cpp"
		"ALSA.hpp"

		"Engine.cpp"
		"Engine.hpp"

		"Flux.cpp"
		"Flux.hpp"
)

target_link_libraries(be_alsa
	PRIVATE
		ALSA::ALSA
)
