// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_OSS_MIXER_HPP
#define CROSSAUDIO_SRC_BACKENDS_OSS_MIXER_HPP

#include "FileDescriptor.hpp"

struct oss_audioinfo;
struct oss_sysinfo;

namespace oss {
class Mixer : public FileDescriptor {
public:
	bool open();

	bool getAudioInfo(oss_audioinfo &info);
	bool getSysInfo(oss_sysinfo &info);
};
} // namespace oss

#endif
