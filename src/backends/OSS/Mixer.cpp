// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Mixer.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <sys/soundcard.h>

using namespace oss;

bool Mixer::open() {
	m_handle = ::open("/dev/mixer", O_RDONLY, 0);
	return static_cast< bool >(*this);
}

bool Mixer::getAudioInfo(oss_audioinfo &info) {
	return ioctl(m_handle, SNDCTL_AUDIOINFO, &info) >= 0;
}

bool Mixer::getSysInfo(oss_sysinfo &info) {
	return ioctl(m_handle, SNDCTL_SYSINFO, &info) >= 0;
}
