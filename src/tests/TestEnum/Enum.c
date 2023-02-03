// Copyright 2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Common.h"

#include "crossaudio/Node.h"

typedef struct CrossAudio_Node Node;

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
		Node *nodes = CrossAudio_engineNodesGet(engine);
		if (!nodes) {
			ret = 3;
			break;
		}

		for (size_t i = 0; i < SIZE_MAX; ++i) {
			const Node *node = &nodes[i];
			if (!node->id) {
				break;
			}

			printf("[%s] %s (%s)\n", node->id, node->name, CrossAudio_DirectionText(node->direction));
		}

		CrossAudio_engineNodesFree(engine, nodes);
	} while (getchar() != 'q');

	if (!destroyEngine(engine)) {
		return 4;
	}

	if (!deinitBackend()) {
		return 5;
	}

	return ret;
}
