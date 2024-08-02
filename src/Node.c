// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Node.h"

#include <stdlib.h>

typedef enum CrossAudio_ErrorCode ErrorCode;

static void freeNodeInner(Node *node) {
	free(node->id);
	free(node->name);
}

Nodes *nodesNew(const size_t count) {
	Nodes *nodes = malloc(sizeof(Nodes));

	nodes->count = count;
	nodes->items = calloc(count, sizeof(Node));

	return nodes;
}

ErrorCode CrossAudio_nodeFree(Node *node) {
	freeNodeInner(node);

	free(node);

	return CROSSAUDIO_EC_OK;
}

ErrorCode CrossAudio_nodesFree(Nodes *nodes) {
	for (size_t i = 0; i < nodes->count; ++i) {
		freeNodeInner(&nodes->items[i]);
	}

	free(nodes->items);
	free(nodes);

	return CROSSAUDIO_EC_OK;
}
