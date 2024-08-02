// Copyright 2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Common.h"

#include "crossaudio/Node.h"

typedef struct CrossAudio_Node Node;
typedef struct CrossAudio_Nodes Nodes;

int main() {
	if (!initBackend()) {
		return 1;
	}

	Engine *engine = createEngine();
	if (!engine) {
		return 2;
	}

	int ret = 0;

	do {
		Nodes *nodes = CrossAudio_engineNodesGet(engine);
		if (!nodes) {
			ret = 3;
			break;
		}

		for (size_t i = 0; i < nodes->count; ++i) {
			const Node *node = &nodes->items[i];
			if (!node->id) {
				break;
			}

			printf("[%s] %s (%s)\n", node->id, node->name, CrossAudio_DirectionText(node->direction));
		}

		CrossAudio_nodesFree(nodes);
	} while (getchar() != 'q');

	if (!destroyEngine(engine)) {
		return 4;
	}

	if (!deinitBackend()) {
		return 5;
	}

	return ret;
}
