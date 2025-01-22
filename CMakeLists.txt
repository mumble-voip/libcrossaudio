# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

cmake_minimum_required(VERSION 3.18.4)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(libcrossaudio
	VERSION "0.0.0.0"
	DESCRIPTION "A cross-platform audio library."
	HOMEPAGE_URL "https://www.mumble.info"
	LANGUAGES "C" "CXX"
)

option(CROSSAUDIO_BUILD_TESTS "Build tests" ON)

option(CROSSAUDIO_STATIC "Build the library as static instead of shared" OFF)

include(CTest)

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)

set(3RDPARTY_DIR "${PROJECT_SOURCE_DIR}/3rdparty")
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")

if(WIN32)
	add_definitions(
		"-D_CRT_SECURE_NO_WARNINGS"
		"-DNOMINMAX"
		"-DWIN32_LEAN_AND_MEAN"
	)
endif()

add_subdirectory(src)
