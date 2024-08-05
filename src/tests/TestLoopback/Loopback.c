// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Common.h"
#include "Key.h"
#include "RingBuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CHANNELS (2)
#define RATE (48000)
#define QUANTUM (2048)

#define SAMPLE_SIZE (sizeof(int32_t))
#define FRAME_SIZE (SAMPLE_SIZE * CHANNELS)
#define FRAG_SIZE (FRAME_SIZE * QUANTUM)
#define BUFFER_SIZE (FRAG_SIZE * 3)

typedef struct CrossAudio_FluxData FluxData;

static void inProcess(void *userData, FluxData *data) {
	RingBuffer *buffer = userData;

	ringBufferWrite(buffer, data->data, FRAME_SIZE * data->frames);
}

static void outProcess(void *userData, FluxData *data) {
	RingBuffer *buffer = userData;

	const uint32_t bytes = ringBufferRead(buffer, data->data, FRAME_SIZE * data->frames);

	data->frames = bytes / FRAME_SIZE;
}

static void parseOptions(const char **inputNodeID, const char **outputNodeID, const char *option, const char *value) {
	if (strcmp(option, "--input") == 0) {
		*inputNodeID = value;
	} else if (strcmp(option, "--output") == 0) {
		*outputNodeID = value;
	}
}

int main(const int argc, const char *argv[]) {
	const char *inputNodeID  = CROSSAUDIO_FLUX_DEFAULT_NODE;
	const char *outputNodeID = CROSSAUDIO_FLUX_DEFAULT_NODE;

	switch (argc) {
		case 5:
			parseOptions(&inputNodeID, &outputNodeID, argv[3], argv[4]);
		case 3:
			parseOptions(&inputNodeID, &outputNodeID, argv[1], argv[2]);
		case 1:
			break;
		default:
			printf("Usage: TestLoopback --input <input node ID> --output <output node ID>\n");
			return -1;
	}

	if (!initBackend()) {
		return 1;
	}

	Engine *engine = createEngine(NULL);
	if (!engine) {
		return 2;
	}

	RingBuffer *buffer = ringBufferNew(BUFFER_SIZE);
	if (!buffer) {
		printf("ringBufferNew() failed!\n");
		destroyEngine(engine);
		return 3;
	}

	int ret = 0;

	Flux *streams[]       = { NULL, NULL };
	FluxConfig config     = { .node       = inputNodeID,
							  .direction  = CROSSAUDIO_DIR_IN,
							  .bitFormat  = CROSSAUDIO_BF_INTEGER_SIGNED,
							  .sampleBits = SAMPLE_SIZE * 8,
							  .sampleRate = RATE,
							  .channels   = CHANNELS,
							  .position   = { CROSSAUDIO_CH_FRONT_LEFT, CROSSAUDIO_CH_FRONT_RIGHT } };
	FluxFeedback feedback = { .userData = buffer, .process = inProcess };

	streams[0] = createStream(engine, &config, &feedback);
	if (!streams[0]) {
		printf("createStream() failed to create input stream!\n");
		ret = 4;
		goto FINAL;
	}

	config.node      = outputNodeID;
	config.direction = CROSSAUDIO_DIR_OUT;
	feedback.process = outProcess;

	streams[1] = createStream(engine, &config, &feedback);
	if (!streams[1]) {
		printf("createStream() failed to create output stream!\n");
		ret = 4;
		goto FINAL;
	}

	bool halt   = false;
	bool paused = false;

	while (!halt) {
		switch (getKey()) {
			case KEY_BREAK:
				halt = true;
				break;
			case KEY_PAUSE:
				paused = !paused;
				CrossAudio_fluxPause(streams[0], paused);
				CrossAudio_fluxPause(streams[1], paused);

				printf("Paused: %s\n", paused ? "true" : "false");
			default:
				break;
		}
	}
FINAL:
	if (!destroyStream(streams[0]) || !destroyStream(streams[1])) {
		return 5;
	}

	if (!destroyEngine(engine)) {
		return 6;
	}

	if (!deinitBackend()) {
		return 7;
	}

	ringBufferFree(buffer);

	return ret;
}
