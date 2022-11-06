# Copyright 2022 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

# CR krzmbrz: I'm reasonably sure that we don't need such a recent cmake version, do we?
cmake_minimum_required(VERSION 3.18.4)

# CR krzmbrzl: We should use set, instead of list(APPEND here as we don't want to use the super project's
# module path. This can cause some issues as I recently discovered.
list(APPEND CMAKE_MODULE_PATH
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

project(libcrossaudio
	VERSION "0.0.0.0"
	DESCRIPTION "A cross-platform audio library."
	HOMEPAGE_URL "https://www.mumble.info"
	LANGUAGES "C"
)

# CR krzmbrzl: We should prefix all our options with e.g. CROSSAUDIO_ and we should follow
# cmake conventions and make options all-uppercase. So this one should be CROSSAUDIO_BUILD_TESTS
option(build-tests "Build tests" ON)

include(CTest)

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(3RDPARTY_DIR "${PROJECT_SOURCE_DIR}/3rdparty")
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")

if(WIN32)
	# CR krzmbrzl: Do we include windows headers in our public headers? Otherwise, we should only add these add_definitions(
	# as private definitions to our project in order to not spill these definitions into downstream projects.
	)
	add_definitions(
		"-DNOMINMAX"
		"-DWIN32_LEAN_AND_MEAN"
	)
endif()

add_subdirectory(src)

# CR krzmbrzl: We should always include CTest, even if we don't build tests. That way we can always run ctest and not get an error,
# which is nice as you can just unconditionally run ctest without having to first check whether tests really have been built.
if(build-tests)
	add_subdirectory(tests)
endif()
