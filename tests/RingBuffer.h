// Copyright 2022 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_TESTS_RINGBUFFER_H
#define CROSSAUDIO_TESTS_RINGBUFFER_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct RingBuffer {
	uint8_t *buf;
	uint32_t head;
	uint32_t tail;
	uint32_t size;
	atomic_uint_fast32_t pending;
} RingBuffer;

static inline RingBuffer *ringBufferNew(const uint32_t size) {
	RingBuffer *ringBuffer = calloc(1, sizeof(*ringBuffer));
	if (!ringBuffer)
		return NULL;

	ringBuffer->buf = malloc(size);
	if (!ringBuffer->buf) {
		free(ringBuffer);
		return NULL;
	}

	ringBuffer->size = size;

	return ringBuffer;
}

static inline void ringBufferFree(RingBuffer *ringBuffer) {
	free(ringBuffer->buf);
	free(ringBuffer);
}

static inline void ringBufferReset(RingBuffer *ringBuffer) {
	ringBuffer->head    = 0;
	ringBuffer->tail    = 0;
	ringBuffer->pending = 0;
}

static inline uint32_t ringBufferSize(const RingBuffer *ringBuffer) {
	return ringBuffer->size;
}

static inline uint32_t ringBufferReadable(const RingBuffer *ringBuffer) {
	return ringBuffer->pending;
}

static inline uint32_t ringBufferWritable(const RingBuffer *ringBuffer) {
	return ringBuffer->size - ringBuffer->pending;
}

static inline uint32_t ringBufferRead(RingBuffer *ringBuffer, void *dst, uint32_t size) {
	uint8_t *buf         = dst;
	const uint32_t avail = ringBufferReadable(ringBuffer);

	if (size > avail) {
		size = avail;
	}

	if (!size) {
		return 0;
	}

	if (ringBuffer->head + size <= ringBuffer->size) {
		memcpy(buf, &ringBuffer->buf[ringBuffer->head], size);

		ringBuffer->head += size;
	} else {
		uint32_t sizePart = ringBuffer->size - ringBuffer->head;

		memcpy(buf, &ringBuffer->buf[ringBuffer->head], sizePart);

		buf += sizePart;
		sizePart = size - sizePart;
		memcpy(buf, ringBuffer->buf, sizePart);

		ringBuffer->head = sizePart;
	}

	if (ringBuffer->head == ringBuffer->size) {
		ringBuffer->head = 0;
	}

	atomic_fetch_sub(&ringBuffer->pending, size);

	return size;
}

static inline uint32_t ringBufferWrite(RingBuffer *ringBuffer, const void *src, uint32_t size) {
	const uint8_t *buf   = src;
	const uint32_t avail = ringBufferWritable(ringBuffer);

	if (size > avail) {
		size = avail;
	}

	if (!size) {
		return size;
	}

	if (ringBuffer->tail + size <= ringBuffer->size) {
		memcpy(&ringBuffer->buf[ringBuffer->tail], buf, size);

		ringBuffer->tail += size;
	} else {
		uint32_t sizePart = ringBuffer->size - ringBuffer->tail;

		memcpy(&ringBuffer->buf[ringBuffer->tail], buf, sizePart);

		buf += sizePart;
		sizePart = size - sizePart;
		memcpy(ringBuffer->buf, buf, sizePart);

		ringBuffer->tail = sizePart;
	}

	if (ringBuffer->tail == ringBuffer->size) {
		ringBuffer->tail = 0;
	}

	atomic_fetch_add(&ringBuffer->pending, size);

	return size;
}

#endif
