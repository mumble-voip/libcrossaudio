// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_DIRECTION_H
#define CROSSAUDIO_DIRECTION_H

enum CrossAudio_Direction { CROSSAUDIO_DIR_NONE, CROSSAUDIO_DIR_IN, CROSSAUDIO_DIR_OUT, CROSSAUDIO_DIR_BOTH };

static inline const char *CrossAudio_DirectionText(const enum CrossAudio_Direction dir) {
	switch (dir) {
		case CROSSAUDIO_DIR_NONE:
			return "CROSSAUDIO_DIR_NONE";
		case CROSSAUDIO_DIR_IN:
			return "CROSSAUDIO_DIR_IN";
		case CROSSAUDIO_DIR_OUT:
			return "CROSSAUDIO_DIR_OUT";
		case CROSSAUDIO_DIR_BOTH:
			return "CROSSAUDIO_DIR_BOTH";
	}

	return "";
}

#endif
