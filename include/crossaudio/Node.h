// Copyright 2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_NODE_H
#define CROSSAUDIO_NODE_H

#include "Direction.h"

struct CrossAudio_Node {
	char *id;
	char *name;
	enum CrossAudio_Direction direction;
};

#endif
