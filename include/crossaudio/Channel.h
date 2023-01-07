// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_CHANNEL_H
#define CROSSAUDIO_CHANNEL_H

#define CROSSAUDIO_CH_NUM (18)

enum CrossAudio_Channel {
	CROSSAUDIO_CH_NONE               = 0,
	CROSSAUDIO_CH_FRONT_LEFT         = 1 << 0,
	CROSSAUDIO_CH_FRONT_RIGHT        = 1 << 1,
	CROSSAUDIO_CH_FRONT_CENTER       = 1 << 2,
	CROSSAUDIO_CH_LOW_FREQUENCY      = 1 << 3,
	CROSSAUDIO_CH_REAR_LEFT          = 1 << 4,
	CROSSAUDIO_CH_REAR_RIGHT         = 1 << 5,
	CROSSAUDIO_CH_FRONT_LEFT_CENTER  = 1 << 6,
	CROSSAUDIO_CH_FRONT_RIGHT_CENTER = 1 << 7,
	CROSSAUDIO_CH_REAR_CENTER        = 1 << 8,
	CROSSAUDIO_CH_SIDE_LEFT          = 1 << 9,
	CROSSAUDIO_CH_SIDE_RIGHT         = 1 << 10,
	CROSSAUDIO_CH_TOP_CENTER         = 1 << 11,
	CROSSAUDIO_CH_TOP_FRONT_LEFT     = 1 << 12,
	CROSSAUDIO_CH_TOP_FRONT_CENTER   = 1 << 13,
	CROSSAUDIO_CH_TOP_FRONT_RIGHT    = 1 << 14,
	CROSSAUDIO_CH_TOP_REAR_LEFT      = 1 << 15,
	CROSSAUDIO_CH_TOP_REAR_CENTER    = 1 << 16,
	CROSSAUDIO_CH_TOP_REAR_RIGHT     = 1 << 17
};

#endif
