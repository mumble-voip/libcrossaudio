// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_WASAPI_DEVICE_HPP
#define CROSSAUDIO_SRC_BACKENDS_WASAPI_DEVICE_HPP

using Node = struct CrossAudio_Node;

struct IMMDevice;

namespace wasapi {
bool populateNode(Node &node, IMMDevice &device, const wchar_t *id);

char *utf16To8(const wchar_t *utf16);
wchar_t *utf8To16(const char *utf8);
} // namespace wasapi

#endif
