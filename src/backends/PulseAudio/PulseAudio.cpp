// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "PulseAudio.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

using namespace pulseaudio;

using FluxData = CrossAudio_FluxData;

static Library lib;

static auto toImpl(BE_Engine *engine) {
	return reinterpret_cast< Engine * >(engine);
}

static auto toImpl(BE_Flux *flux) {
	return reinterpret_cast< Flux * >(flux);
}

static const char *name() {
	return "PulseAudio";
}

static const char *version() {
	return lib.get_library_version();
}

static ErrorCode init() {
	// clang-format off
	constexpr std::string_view names[] = {
		"libpulse.so",
		"libpulse.so.0"
	};
	// clang-format on

	ErrorCode ret;

	for (const auto name : names) {
		if ((ret = lib.load(name)) != CROSSAUDIO_EC_LIBRARY) {
			break;
		}
	}

	return ret;
}

static ErrorCode deinit() {
	lib.unload();

	return CROSSAUDIO_EC_OK;
}

static BE_Engine *engineNew() {
	if (auto engine = new Engine()) {
		if (*engine) {
			return reinterpret_cast< BE_Engine * >(engine);
		}

		delete engine;
	}

	return nullptr;
}

static ErrorCode engineFree(BE_Engine *engine) {
	delete toImpl(engine);

	return CROSSAUDIO_EC_OK;
}

static ErrorCode engineStart(BE_Engine *engine) {
	return toImpl(engine)->start();
}

static ErrorCode engineStop(BE_Engine *engine) {
	return toImpl(engine)->stop();
}

static const char *engineNameGet(BE_Engine *engine) {
	return toImpl(engine)->nameGet();
}

static ErrorCode engineNameSet(BE_Engine *engine, const char *name) {
	return toImpl(engine)->nameSet(name);
}

static Node *engineNodesGet(BE_Engine *engine) {
	return toImpl(engine)->engineNodesGet();
}

static ErrorCode engineNodesFree(BE_Engine *engine, Node *nodes) {
	return toImpl(engine)->engineNodesFree(nodes);
}

static BE_Flux *fluxNew(BE_Engine *engine) {
	return reinterpret_cast< BE_Flux * >(new Flux(*toImpl(engine)));
}

static ErrorCode fluxFree(BE_Flux *flux) {
	delete toImpl(flux);

	return CROSSAUDIO_EC_OK;
}

static ErrorCode fluxStart(BE_Flux *flux, FluxConfig *config, const FluxFeedback *feedback) {
	return toImpl(flux)->start(*config, feedback ? *feedback : FluxFeedback());
}

static ErrorCode fluxStop(BE_Flux *flux) {
	return toImpl(flux)->stop();
}

static ErrorCode fluxPause(BE_Flux *flux, const bool on) {
	return toImpl(flux)->pause(on);
}

static const char *fluxNameGet(BE_Flux *flux) {
	return toImpl(flux)->nameGet();
}

static ErrorCode fluxNameSet(BE_Flux *flux, const char *name) {
	return toImpl(flux)->nameSet(name);
}

// clang-format off
constexpr BE_Impl PulseAudio_Impl = {
	name,
	version,

	init,
	deinit,

	engineNew,
	engineFree,
	engineStart,
	engineStop,
	engineNameGet,
	engineNameSet,
	engineNodesGet,
	engineNodesFree,

	fluxNew,
	fluxFree,
	fluxStart,
	fluxStop,
	fluxPause,
	fluxNameGet,
	fluxNameSet
};
// clang-format on

Engine::Engine() : m_context(nullptr) {
	if ((m_threadLoop = lib.threaded_mainloop_new())) {
		const auto api = lib.threaded_mainloop_get_api(m_threadLoop);

		auto props = lib.proplist_new();
		lib.proplist_sets(props, PA_PROP_MEDIA_SOFTWARE, "libcrossaudio");

		m_context = lib.context_new_with_proplist(api, nullptr, props);

		lib.proplist_free(props);
	}
}

Engine::~Engine() {
	stop();

	if (m_context) {
		const auto lock = locker();
		lib.context_unref(m_context);
	}

	if (m_threadLoop) {
		lib.threaded_mainloop_free(m_threadLoop);
	}
}

void Engine::lock() {
	if (m_threadLoop) {
		lib.threaded_mainloop_lock(m_threadLoop);
	}
}

void Engine::unlock() {
	if (m_threadLoop) {
		lib.threaded_mainloop_unlock(m_threadLoop);
	}
}

ErrorCode Engine::start() {
	switch (lib.context_get_state(m_context)) {
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			break;
		default:
			return CROSSAUDIO_EC_INIT;
	}

	lib.context_set_state_callback(m_context, contextState, this);
	lib.context_set_subscribe_callback(m_context, contextEvent, this);

	if (lib.context_connect(m_context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0) {
		return CROSSAUDIO_EC_CONNECT;
	}

	if (lib.threaded_mainloop_start(m_threadLoop) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	m_connectComplete.wait(false);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
	lock();

	m_nodes.clear();

	if (m_context) {
		lib.context_disconnect(m_context);
	}

	unlock();

	if (m_threadLoop) {
		lib.threaded_mainloop_stop(m_threadLoop);
	}

	return CROSSAUDIO_EC_OK;
}

const char *Engine::nameGet() const {
	return m_name.data();
}

ErrorCode Engine::nameSet(const char *name) {
	m_name = name;

	const auto lock = locker();

	lib.context_set_name(m_context, name, nullptr, nullptr);

	return CROSSAUDIO_EC_OK;
}

::Node *Engine::engineNodesGet() {
	const std::unique_lock lock(m_nodesLock);

	auto nodes = static_cast< ::Node * >(calloc(m_nodes.size() + 1, sizeof(::Node)));

	size_t i = 0;

	for (const auto &iter : m_nodes) {
		const auto &nodeIn = iter.second;
		auto &nodeOut      = nodes[i++];

		nodeOut.id        = strdup(nodeIn.name.data());
		nodeOut.name      = strdup(nodeIn.description.data());
		nodeOut.direction = nodeIn.direction;
	}

	return nodes;
}

ErrorCode Engine::engineNodesFree(::Node *nodes) {
	for (size_t i = 0; i < std::numeric_limits< size_t >::max(); ++i) {
		auto &node = nodes[i];
		if (!node.id) {
			break;
		}

		free(node.id);
		free(node.name);
	}

	free(nodes);

	return CROSSAUDIO_EC_OK;
}

std::string Engine::defaultInName() {
	std::unique_lock lock(m_nodesLock);

	return m_defaultInName;
}

std::string Engine::defaultOutName() {
	std::unique_lock lock(m_nodesLock);

	return m_defaultOutName;
}

void Engine::fixNameIfMonitor(std::string &name) {
	std::unique_lock lock(m_nodesLock);

	if (const auto iter = m_nodeMonitors.find(name); iter != m_nodeMonitors.cend()) {
		name = iter->second;
	}
}

void Engine::serverInfo(pa_context *, const pa_server_info *info, void *userData) {
	auto &engine = *static_cast< Engine * >(userData);

	std::unique_lock lock(engine.m_nodesLock);

	engine.m_defaultInName  = info->default_source_name;
	engine.m_defaultOutName = info->default_sink_name;
}

void Engine::sinkInfo(pa_context *, const pa_sink_info *info, const int eol, void *userData) {
	if (eol) {
		return;
	}

	auto &engine = *static_cast< Engine * >(userData);

	const std::unique_lock lock(engine.m_nodesLock);

	auto direction = CROSSAUDIO_DIR_OUT;

	if (info->monitor_source != PA_INVALID_INDEX) {
		direction = CROSSAUDIO_DIR_BOTH;
		engine.m_nodeMonitors.emplace(info->name, info->monitor_source_name);
	}

	engine.m_nodes.emplace(info->index, Node(info->name, info->description, direction));
}

void Engine::sourceInfo(pa_context *, const pa_source_info *info, const int eol, void *userData) {
	if (eol || info->monitor_of_sink != PA_INVALID_INDEX) {
		return;
	}

	auto &engine = *static_cast< Engine * >(userData);

	const std::unique_lock lock(engine.m_nodesLock);

	engine.m_nodes.emplace(info->index, Node(info->name, info->description, CROSSAUDIO_DIR_IN));
}

void Engine::contextEvent(pa_context *context, pa_subscription_event_type_t type, unsigned int index, void *userData) {
	auto &engine = *static_cast< Engine * >(userData);

	bool source;
	switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
		case PA_SUBSCRIPTION_EVENT_SINK:
			source = false;
			break;
		case PA_SUBSCRIPTION_EVENT_SOURCE:
			source = true;
			break;
		default:
			return;
	}

	bool add;
	switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
		case PA_SUBSCRIPTION_EVENT_NEW:
			add = true;
			break;
		case PA_SUBSCRIPTION_EVENT_REMOVE:
			add = false;
			break;
		default:
			return;
	}

	if (add) {
		if (source) {
			lib.operation_unref(lib.context_get_source_info_by_index(context, index, sourceInfo, userData));
		} else {
			lib.operation_unref(lib.context_get_sink_info_by_index(context, index, sinkInfo, userData));
		}
	} else {
		std::unique_lock lock(engine.m_nodesLock);

		auto &nodes = engine.m_nodes;

		const auto iter = nodes.find(index);
		if (iter == nodes.cend()) {
			return;
		}

		if (source) {
			nodes.erase(iter);
		} else {
			auto &monitors = engine.m_nodeMonitors;

			const auto node = nodes.extract(iter);
			if (const auto monitorIter = monitors.find(node.mapped().name); monitorIter != monitors.cend()) {
				engine.m_nodeMonitors.erase(monitorIter);
			}
		}
	}
}

void Engine::contextState(pa_context *context, void *userData) {
	auto &engine = *static_cast< Engine * >(userData);

	switch (lib.context_get_state(context)) {
		case PA_CONTEXT_READY: {
			const auto mask =
				static_cast< pa_subscription_mask_t >(PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE);
			lib.operation_unref(lib.context_subscribe(context, mask, nullptr, userData));

			lib.operation_unref(lib.context_get_server_info(context, serverInfo, userData));
			lib.operation_unref(lib.context_get_sink_info_list(context, sinkInfo, userData));
			lib.operation_unref(lib.context_get_source_info_list(context, sourceInfo, userData));

			[[fallthrough]];
		}
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			engine.m_connectComplete.test_and_set();
			engine.m_connectComplete.notify_all();
		default:
			break;
	}
}

Engine::Node::Node(Node &&node)
	: name(std::move(node.name)), description(std::move(node.description)), direction(node.direction) {
}

Engine::Node::Node(const char *name, const char *description, Direction direction)
	: name(name), description(description), direction(direction) {
}

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

	if (m_stream = lib.stream_new(m_engine.m_context, "", &sampleSpec, &channelMap); !m_stream) {
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

			lib.stream_set_read_callback(
				m_stream,
				[](pa_stream *, size_t bytes, void *userData) { static_cast< Flux * >(userData)->processInput(bytes); },
				this);

			ret = lib.stream_connect_record(m_stream, nodeID.data(), &bufferAttr, PA_STREAM_NOFLAGS);
			break;
		case CROSSAUDIO_DIR_OUT: {
			if (nodeID.empty()) {
				nodeID = m_engine.defaultOutName();
			}

			lib.stream_set_write_callback(
				m_stream,
				[](pa_stream *, size_t bytes, void *userData) {
					static_cast< Flux * >(userData)->processOutput(bytes);
				},
				this);

			ret =
				lib.stream_connect_playback(m_stream, nodeID.data(), &bufferAttr, PA_STREAM_NOFLAGS, nullptr, nullptr);
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
		lib.stream_unref(m_stream);
		m_stream = nullptr;
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::pause(const bool on) {
	const auto lock = m_engine.locker();

	lib.operation_unref(lib.stream_cork(m_stream, on, nullptr, nullptr));

	return CROSSAUDIO_EC_OK;
}

const char *Flux::nameGet() const {
	return m_name.data();
}

ErrorCode Flux::nameSet(const char *name) {
	m_name = name;

	const auto lock = m_engine.locker();

	lib.stream_set_name(m_stream, name, nullptr, nullptr);

	return CROSSAUDIO_EC_OK;
}

void Flux::processInput(size_t bytes) {
	const void *data;
	if (lib.stream_peek(m_stream, &data, &bytes) < 0) {
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

	lib.stream_drop(m_stream);
}

void Flux::processOutput(size_t bytes) {
	void *data;
	if ((lib.stream_begin_write(m_stream, &data, &bytes) < 0) || !data) {
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

	lib.stream_write(m_stream, data, bytes, nullptr, 0, PA_SEEK_RELATIVE);
}

constexpr pa_channel_map Flux::configToMap(const FluxConfig &config) {
	pa_channel_map ret = {};

	ret.channels = config.channels;

	for (uint8_t i = 0; i < config.channels; ++i) {
		ret.map[i] = translateChannel(config.position[i]);
	}

	return ret;
}

constexpr pa_sample_format Flux::translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits) {
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

constexpr pa_channel_position Flux::translateChannel(const CrossAudio_Channel channel) {
	switch (channel) {
		default:
		case CROSSAUDIO_CH_NONE:
			return PA_CHANNEL_POSITION_INVALID;
		case CROSSAUDIO_CH_FRONT_LEFT:
			return PA_CHANNEL_POSITION_FRONT_LEFT;
		case CROSSAUDIO_CH_FRONT_RIGHT:
			return PA_CHANNEL_POSITION_FRONT_RIGHT;
		case CROSSAUDIO_CH_FRONT_CENTER:
			return PA_CHANNEL_POSITION_FRONT_CENTER;
		case CROSSAUDIO_CH_LOW_FREQUENCY:
			return PA_CHANNEL_POSITION_LFE;
		case CROSSAUDIO_CH_REAR_LEFT:
			return PA_CHANNEL_POSITION_REAR_LEFT;
		case CROSSAUDIO_CH_REAR_RIGHT:
			return PA_CHANNEL_POSITION_REAR_RIGHT;
		case CROSSAUDIO_CH_FRONT_LEFT_CENTER:
			return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
		case CROSSAUDIO_CH_FRONT_RIGHT_CENTER:
			return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
		case CROSSAUDIO_CH_REAR_CENTER:
			return PA_CHANNEL_POSITION_REAR_CENTER;
		case CROSSAUDIO_CH_SIDE_LEFT:
			return PA_CHANNEL_POSITION_SIDE_LEFT;
		case CROSSAUDIO_CH_SIDE_RIGHT:
			return PA_CHANNEL_POSITION_SIDE_RIGHT;
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
