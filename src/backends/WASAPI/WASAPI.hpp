// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_WASAPI_WASAPI_HPP
#define CROSSAUDIO_SRC_BACKENDS_WASAPI_WASAPI_HPP

struct BE_Impl;

extern "C" {
extern const BE_Impl WASAPI_Impl;
}

#endif
