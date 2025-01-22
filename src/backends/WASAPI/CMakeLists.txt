# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

enable_language(CXX)

add_library(be_wasapi OBJECT)

target_pedantic_warnings(be_wasapi)

target_compile_definitions(be_wasapi
	INTERFACE
		"HAS_WASAPI"
)

target_include_directories(be_wasapi
	PRIVATE
		${SRC_DIR}
		${INCLUDE_DIR}
)

target_sources(be_wasapi
	PRIVATE
		"Device.cpp"
		"Device.hpp"
		"EventManager.cpp"
		"EventManager.hpp"
		"WASAPI.cpp"
		"WASAPI.hpp"
)

target_link_libraries(be_wasapi
	PRIVATE
		"avrt.lib"
		"Version.lib"
)
