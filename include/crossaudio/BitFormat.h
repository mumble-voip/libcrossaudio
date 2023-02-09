// Copyright 2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_BITFORMAT_H
#define CROSSAUDIO_BITFORMAT_H

enum CrossAudio_BitFormat {
	CROSSAUDIO_BF_NONE,
	CROSSAUDIO_BF_INTEGER_SIGNED,
	CROSSAUDIO_BF_INTEGER_UNSIGNED,
	CROSSAUDIO_BF_FLOAT
};

static inline const char *CrossAudio_BitFormatText(const enum CrossAudio_BitFormat bf) {
	switch (bf) {
		case CROSSAUDIO_BF_NONE:
			return "CROSSAUDIO_BF_NONE";
		case CROSSAUDIO_BF_INTEGER_SIGNED:
			return "CROSSAUDIO_BF_INTEGER_SIGNED";
		case CROSSAUDIO_BF_INTEGER_UNSIGNED:
			return "CROSSAUDIO_BF_INTEGER_UNSIGNED";
		case CROSSAUDIO_BF_FLOAT:
			return "CROSSAUDIO_BF_FLOAT";
	}

	return "";
}

#endif
