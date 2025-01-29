// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Flux.hpp"

#include "Engine.hpp"
#include "Library.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

using namespace pulseaudio;

typedef CrossAudio_FluxData FluxData;

static constexpr pa_channel_map configToMap(const FluxConfig &config);
static constexpr pa_sample_format translateFormat(CrossAudio_BitFormat format, uint8_t sampleBits);
static constexpr pa_channel_position translateChannel(CrossAudio_Channel channel);

Flux::Flux(Engine &engine) : m_engine(engine), m_stream(nullptr), m_frameSize(0) {
}

Flux::~Flux() {
	stop();
}

ErrorCode Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	if (m_stream) {
		return CROSSAUDIO_EC_INIT;
	}

	m_feedback = feedback;

	config.channels = std::min(config.channels, static_cast< uint8_t >(PA_CHANNELS_MAX));

	const pa_sample_spec sampleSpec{ .format   = translateFormat(config.bitFormat, config.sampleBits),
									 .rate     = config.sampleRate,
									 .channels = config.channels };

	const pa_channel_map channelMap = configToMap(config);

	m_frameSize = config.sampleBits / 8 * config.channels;

	if (m_stream = lib().stream_new(m_engine.m_context, "", &sampleSpec, &channelMap); !m_stream) {
		return CROSSAUDIO_EC_GENERIC;
	}

	std::string nodeID;
	if (config.node && strcmp(config.node, CROSSAUDIO_FLUX_DEFAULT_NODE) != 0) {
		nodeID = config.node;
	}

	pa_buffer_attr bufferAttr;
	const uint32_t bytes = m_frameSize * config.sampleRate / 100;
	bufferAttr.tlength   = bytes;
	bufferAttr.minreq    = bytes;
	bufferAttr.maxlength = static_cast< decltype(bufferAttr.maxlength) >(-1);
	bufferAttr.prebuf    = static_cast< decltype(bufferAttr.prebuf) >(-1);
	bufferAttr.fragsize  = bytes;

	int ret;
	switch (config.direction) {
		case CROSSAUDIO_DIR_IN:
			if (nodeID.empty()) {
				nodeID = m_engine.defaultInName();
			} else {
				m_engine.fixNameIfMonitor(nodeID);
			}

			lib().stream_set_read_callback(
				m_stream,
				[](pa_stream *, size_t bytes, void *userData) { static_cast< Flux * >(userData)->processInput(bytes); },
				this);

			ret = lib().stream_connect_record(m_stream, nodeID.data(), &bufferAttr, PA_STREAM_NOFLAGS);
			break;
		case CROSSAUDIO_DIR_OUT: {
			if (nodeID.empty()) {
				nodeID = m_engine.defaultOutName();
			}

			lib().stream_set_write_callback(
				m_stream,
				[](pa_stream *, size_t bytes, void *userData) {
					static_cast< Flux * >(userData)->processOutput(bytes);
				},
				this);

			ret = lib().stream_connect_playback(m_stream, nodeID.data(), &bufferAttr, PA_STREAM_NOFLAGS, nullptr,
												nullptr);
			break;
		}
		default:
			ret = -1;
	}

	if (ret < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::stop() {
	const auto lock = m_engine.locker();

	if (m_stream) {
		lib().stream_unref(m_stream);
		m_stream = nullptr;
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::pause(const bool on) {
	const auto lock = m_engine.locker();

	lib().operation_unref(lib().stream_cork(m_stream, on, nullptr, nullptr));

	return CROSSAUDIO_EC_OK;
}

const char *Flux::nameGet() const {
	return m_name.data();
}

ErrorCode Flux::nameSet(const char *name) {
	m_name = name;

	const auto lock = m_engine.locker();

	lib().stream_set_name(m_stream, name, nullptr, nullptr);

	return CROSSAUDIO_EC_OK;
}

void Flux::processInput(size_t bytes) {
	const void *data;
	if (lib().stream_peek(m_stream, &data, &bytes) < 0) {
		return;
	}

	if (data) {
		FluxData fluxData = { const_cast< void * >(data), static_cast< uint32_t >(bytes / m_frameSize) };

		m_feedback.process(m_feedback.userData, &fluxData);
	} else if (!bytes) {
		// According to the official documentation:
		// 1. We should not call pa_stream_drop() if the buffer is empty.
		// 2. We should call pa_stream_drop() if there is a "hole". 'bytes' represents its size when not 0.
		return;
	}

	lib().stream_drop(m_stream);
}

void Flux::processOutput(size_t bytes) {
	void *data;
	if ((lib().stream_begin_write(m_stream, &data, &bytes) < 0) || !data) {
		return;
	}

	FluxData fluxData = { data, static_cast< uint32_t >(bytes / m_frameSize) };

	m_feedback.process(m_feedback.userData, &fluxData);

	if (fluxData.frames) {
		bytes = m_frameSize * fluxData.frames;
	} else {
		// Telling PulseAudio that we wrote 0 bytes results in an xrun,
		// which in turn results in this function not being called anymore.
		memset(data, 0, bytes);
	}

	lib().stream_write(m_stream, data, bytes, nullptr, 0, PA_SEEK_RELATIVE);
}

static constexpr pa_channel_map configToMap(const FluxConfig &config) {
	pa_channel_map ret = {};

	ret.channels = config.channels;

	for (uint8_t i = 0; i < config.channels; ++i) {
		ret.map[i] = translateChannel(config.position[i]);
	}

	return ret;
}

static constexpr pa_sample_format translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits) {
	switch (format) {
		default:
		case CROSSAUDIO_BF_NONE:
			break;
		case CROSSAUDIO_BF_INTEGER_SIGNED:
			switch (sampleBits) {
				case 16:
					return PA_SAMPLE_S16NE;
				case 24:
					return PA_SAMPLE_S24_32NE;
				case 32:
					return PA_SAMPLE_S32NE;
			}

			break;
		case CROSSAUDIO_BF_INTEGER_UNSIGNED:
			if (sampleBits == 8) {
				return PA_SAMPLE_U8;
			}

			break;
		case CROSSAUDIO_BF_FLOAT:
			if (sampleBits == 32) {
				return PA_SAMPLE_FLOAT32;
			}

			break;
	}

	return PA_SAMPLE_INVALID;
}

static constexpr pa_channel_position translateChannel(const CrossAudio_Channel channel) {
	switch (channel) {
		default:
		case CROSSAUDIO_CH_UNKNOWN:
			return PA_CHANNEL_POSITION_INVALID;
		case CROSSAUDIO_CH_MONO:
			return PA_CHANNEL_POSITION_MONO;
		case CROSSAUDIO_CH_FRONT_LEFT:
			return PA_CHANNEL_POSITION_FRONT_LEFT;
		case CROSSAUDIO_CH_FRONT_RIGHT:
			return PA_CHANNEL_POSITION_FRONT_RIGHT;
		case CROSSAUDIO_CH_FRONT_CENTER:
			return PA_CHANNEL_POSITION_FRONT_CENTER;
		case CROSSAUDIO_CH_LFE:
			return PA_CHANNEL_POSITION_LFE;
		case CROSSAUDIO_CH_SIDE_LEFT:
			return PA_CHANNEL_POSITION_SIDE_LEFT;
		case CROSSAUDIO_CH_SIDE_RIGHT:
			return PA_CHANNEL_POSITION_SIDE_RIGHT;
		case CROSSAUDIO_CH_FRONT_LEFT_CENTER:
			return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
		case CROSSAUDIO_CH_FRONT_RIGHT_CENTER:
			return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
		case CROSSAUDIO_CH_REAR_CENTER:
			return PA_CHANNEL_POSITION_REAR_CENTER;
		case CROSSAUDIO_CH_REAR_LEFT:
			return PA_CHANNEL_POSITION_REAR_LEFT;
		case CROSSAUDIO_CH_REAR_RIGHT:
			return PA_CHANNEL_POSITION_REAR_RIGHT;
		case CROSSAUDIO_CH_TOP_CENTER:
			return PA_CHANNEL_POSITION_TOP_CENTER;
		case CROSSAUDIO_CH_TOP_FRONT_LEFT:
			return PA_CHANNEL_POSITION_TOP_FRONT_LEFT;
		case CROSSAUDIO_CH_TOP_FRONT_CENTER:
			return PA_CHANNEL_POSITION_TOP_FRONT_CENTER;
		case CROSSAUDIO_CH_TOP_FRONT_RIGHT:
			return PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;
		case CROSSAUDIO_CH_TOP_REAR_LEFT:
			return PA_CHANNEL_POSITION_TOP_REAR_LEFT;
		case CROSSAUDIO_CH_TOP_REAR_CENTER:
			return PA_CHANNEL_POSITION_TOP_REAR_CENTER;
		case CROSSAUDIO_CH_TOP_REAR_RIGHT:
			return PA_CHANNEL_POSITION_TOP_REAR_RIGHT;
	}
}
