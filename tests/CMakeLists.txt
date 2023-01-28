# Copyright 2022-2023 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

add_executable(test_loopback "Loopback.c")

set_target_properties(test_loopback
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests"
)

target_compile_definitions(test_loopback
	PRIVATE
		$<$<PLATFORM_ID:Linux>:"OS_LINUX">
		$<$<PLATFORM_ID:Windows>:"OS_WINDOWS">
)

target_link_libraries(test_loopback
	PRIVATE
		crossaudio
)
