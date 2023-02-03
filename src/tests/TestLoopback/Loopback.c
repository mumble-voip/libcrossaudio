// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Common.h"
#include "RingBuffer.h"

#include <stdint.h>
#include <stdio.h>

#define CHANNELS (2)
#define RATE (48000)
#define QUANTUM (2048)

#define SAMPLE_SIZE (sizeof(float))
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

int main() {
	if (!initBackend()) {
		return 1;
	}

	Engine *engine = createEngine();
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
	FluxConfig config     = { .direction = CROSSAUDIO_DIR_IN, .channels = CHANNELS, .sampleRate = RATE };
	FluxFeedback feedback = { .userData = buffer, .process = inProcess };

	streams[0] = createStream(engine, &config, &feedback);
	if (!streams[0]) {
		printf("createStream() failed to create input stream!\n");
		ret = 4;
		goto FINAL;
	}

	config.direction = CROSSAUDIO_DIR_OUT;
	feedback.process = outProcess;

	streams[1] = createStream(engine, &config, &feedback);
	if (!streams[1]) {
		printf("createStream() failed to create output stream!\n");
		ret = 4;
		goto FINAL;
	}

	getchar();
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
