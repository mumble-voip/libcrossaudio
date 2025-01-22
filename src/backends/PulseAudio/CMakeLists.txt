# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

include(CrossAudioCompiler)

add_library(be_pulseaudio OBJECT)

target_pedantic_warnings(be_pulseaudio)

target_compile_definitions(be_pulseaudio
	INTERFACE
		"HAS_PULSEAUDIO"
)

target_include_directories(be_pulseaudio
	PRIVATE
		${SRC_DIR}
		${INCLUDE_DIR}
)

target_include_directories(be_pulseaudio
	SYSTEM PRIVATE
		"${3RDPARTY_DIR}/PulseAudio"
)

target_sources(be_pulseaudio
	PRIVATE
		"Library.cpp"
		"Library.hpp"
		"PulseAudio.cpp"
		"PulseAudio.hpp"
)
