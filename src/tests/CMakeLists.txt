# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

set(TESTS "")

macro(use_test TEST_NAME)
	add_subdirectory("Test${TEST_NAME}")
	list(APPEND TESTS "Test${TEST_NAME}")
endmacro()

use_test("Enum")
use_test("Loopback")

foreach(CURRENT_TEST IN LISTS TESTS)
	set_target_properties(${CURRENT_TEST}
		PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests"
	)

	target_include_directories(${CURRENT_TEST}
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}
	)

	target_compile_definitions(${CURRENT_TEST}
		PRIVATE
			$<$<PLATFORM_ID:Linux>:"OS_LINUX">
			$<$<PLATFORM_ID:Windows>:"OS_WINDOWS">
	)

	target_link_libraries(${CURRENT_TEST}
		PRIVATE
			crossaudio
	)
endforeach()
