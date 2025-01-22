// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_NODE_H
#define CROSSAUDIO_NODE_H

#include "Direction.h"
#include "ErrorCode.h"
#include "Macros.h"

#include <stddef.h>

struct CrossAudio_Node {
	char *id;
	char *name;
	enum CrossAudio_Direction direction;
};

struct CrossAudio_Nodes {
	size_t count;
	struct CrossAudio_Node *items;
};

#ifdef __cplusplus
extern "C" {
#endif

CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_nodeFree(struct CrossAudio_Node *node);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_nodesFree(struct CrossAudio_Nodes *nodes);

#ifdef __cplusplus
}
#endif

#endif
