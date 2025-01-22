// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_NODE_H
#define CROSSAUDIO_SRC_NODE_H

#include "crossaudio/Node.h"

typedef struct CrossAudio_Node Node;
typedef struct CrossAudio_Nodes Nodes;

#ifdef __cplusplus
extern "C" {
#endif

Node *nodeNew(void);
Nodes *nodesNew(size_t count);

#ifdef __cplusplus
}
#endif

#endif
