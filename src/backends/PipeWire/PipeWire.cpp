// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "PipeWire.hpp"

#include "crossaudio/Macros.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#include <spa/param/audio/raw-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/dict.h>

#include <pipewire/core.h>
#include <pipewire/keys.h>
#include <pipewire/node.h>
#include <pipewire/stream.h>

using FluxData = CrossAudio_FluxData;

static Library lib;

const char *name() {
	return "PipeWire";
}

const char *version() {
	return lib.get_library_version();
}

ErrorCode init() {
	// clang-format off
	constexpr std::string_view names[] = {
		"libpipewire.so",
		"libpipewire.so.0",
		"libpipewire-0.3.so",
		"libpipewire-0.3.so.0"
	};
	// clang-format on

	ErrorCode ret;

	for (const auto name : names) {
		if ((ret = lib.load(name)) != CROSSAUDIO_EC_LIBRARY) {
			break;
		}
	}

	if (lib) {
		lib.init(nullptr, nullptr);
	}

	return ret;
}

ErrorCode deinit() {
	lib.deinit();
	lib.unload();

	return CROSSAUDIO_EC_OK;
}

BE_Engine *engineNew() {
	if (auto engine = new BE_Engine()) {
		if (*engine) {
			return engine;
		}

		delete engine;
	}

	return nullptr;
}

ErrorCode engineFree(BE_Engine *engine) {
	delete engine;

	return CROSSAUDIO_EC_OK;
}

ErrorCode engineStart(BE_Engine *engine) {
	return engine->start();
}

ErrorCode engineStop(BE_Engine *engine) {
	return engine->stop();
}

const char *engineNameGet(BE_Engine *engine) {
	return engine->nameGet();
}

ErrorCode engineNameSet(BE_Engine *engine, const char *name) {
	return engine->nameSet(name);
}

Node *engineNodesGet(BE_Engine *engine) {
	return engine->engineNodesGet();
}

ErrorCode engineNodesFree(BE_Engine *engine, Node *nodes) {
	return engine->engineNodesFree(nodes);
}

BE_Flux *fluxNew(BE_Engine *engine) {
	if (auto flux = new BE_Flux(*engine)) {
		if (*flux) {
			return flux;
		}

		delete flux;
	}

	return nullptr;
}

ErrorCode fluxFree(BE_Flux *flux) {
	delete flux;

	return CROSSAUDIO_EC_OK;
}

ErrorCode fluxStart(BE_Flux *flux, FluxConfig *config, const FluxFeedback *feedback) {
	return flux->start(*config, feedback ? *feedback : FluxFeedback());
}

ErrorCode fluxStop(BE_Flux *flux) {
	return flux->stop();
}

const char *fluxNameGet(BE_Flux *flux) {
	return flux->nameGet();
}

ErrorCode fluxNameSet(BE_Flux *flux, const char *name) {
	return flux->nameSet(name);
}

// clang-format off
constexpr BE_Impl PipeWire_Impl = {
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
	fluxNameGet,
	fluxNameSet
};
// clang-format on

static constexpr auto NODE_TYPE_ID = "PipeWire:Interface:Node";

static constexpr pw_registry_events eventsRegistry = {
	PW_VERSION_REGISTRY_EVENTS,
	[](void *userData, const uint32_t id, const uint32_t /*permissions*/, const char *type, const uint32_t /*version*/,
	   const spa_dict *props) {
		if (spa_streq(type, NODE_TYPE_ID)) {
			static_cast< BE_Engine * >(userData)->addNode(id, props);
		}
	},
	[](void *userData, const uint32_t id) { static_cast< BE_Engine * >(userData)->removeNode(id); }
};

static constexpr pw_node_events eventsNode = { PW_VERSION_NODE_EVENTS,
											   [](void *userData, const pw_node_info *info) {
												   auto &node= *static_cast< BE_Engine::Node*                    >(userData);

												   uint8_t direction= CROSSAUDIO_DIR_NONE;
												   if (info->n_input_ports > 0) {
													   direction |= CROSSAUDIO_DIR_OUT;
												   }
												   if (info->n_output_ports > 0) {
													   direction |= CROSSAUDIO_DIR_IN;
												   }

												   node.direction= static_cast< Direction >(direction);
											   },
											   nullptr };

BE_Engine::BE_Engine() : m_threadLoop(nullptr), m_context(nullptr), m_core(nullptr) {
	if ((m_threadLoop = lib.thread_loop_new(nullptr, nullptr))) {
		m_context = lib.context_new(lib.thread_loop_get_loop(m_threadLoop), nullptr, 0);
	}
}

BE_Engine::~BE_Engine() {
	stop();

	if (m_context) {
		const auto lock = locker();
		lib.context_destroy(m_context);
	}

	if (m_threadLoop) {
		lib.thread_loop_destroy(m_threadLoop);
	}
}

void BE_Engine::lock() {
	if (m_threadLoop) {
		lib.thread_loop_lock(m_threadLoop);
	}
}

void BE_Engine::unlock() {
	if (m_threadLoop) {
		lib.thread_loop_unlock(m_threadLoop);
	}
}

ErrorCode BE_Engine::start() {
	if (m_core) {
		return CROSSAUDIO_EC_INIT;
	}

	if (m_core = lib.context_connect(m_context, nullptr, 0); !m_core) {
		return CROSSAUDIO_EC_CONNECT;
	}

	m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
	pw_registry_add_listener(m_registry, &m_registryListener, &eventsRegistry, this);

	if (lib.thread_loop_start(m_threadLoop) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode BE_Engine::stop() {
	lock();

	spa_hook_remove(&m_registryListener);

	m_nodes.clear();

	if (m_registry) {
		lib.proxy_destroy(reinterpret_cast< pw_proxy * >(m_registry));
	}

	if (m_core) {
		lib.core_disconnect(m_core);
		m_core = nullptr;
	}

	unlock();

	if (m_threadLoop) {
		lib.thread_loop_stop(m_threadLoop);
	}

	return CROSSAUDIO_EC_OK;
}

const char *BE_Engine::nameGet() const {
	if (const auto props = m_core ? lib.core_get_properties(m_core) : lib.context_get_properties(m_context)) {
		return lib.properties_get(props, PW_KEY_APP_NAME);
	}

	return nullptr;
}

ErrorCode BE_Engine::nameSet(const char *name) {
	const spa_dict_item items[] = { { PW_KEY_APP_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = locker();

	auto ret = lib.context_update_properties(m_context, &dict);
	if (ret >= 1 && m_core) {
		ret = lib.core_update_properties(m_core, &dict);
	}

	return ret >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

::Node *BE_Engine::engineNodesGet() {
	const auto lock = locker();

	auto nodes = static_cast< ::Node * >(calloc(m_nodes.size() + 1, sizeof(::Node)));

	size_t i = 0;

	for (const auto &iter : m_nodes) {
		const auto &nodeIn = iter.second;
		auto &nodeOut      = nodes[i++];

		nodeOut.direction = nodeIn.direction;

		const auto size = snprintf(nullptr, 0, "%u", nodeIn.serial) + 1;
		nodeOut.id      = static_cast< char      *>(malloc(size));
		snprintf(nodeOut.id, size, "%u", nodeIn.serial);

		nodeOut.name = strdup(nodeIn.name.data());
	}

	return nodes;
}

ErrorCode BE_Engine::engineNodesFree(::Node *nodes) {
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

void BE_Engine::addNode(const uint32_t id, const spa_dict *props) {
	const char *serialStr = spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL);
	if (!serialStr) {
		return;
	}

	const uint32_t serial = std::strtoul(serialStr, nullptr, 10);
	if (errno != 0) {
		return;
	}

	const char *name;
	if (!(name = spa_dict_lookup(props, PW_KEY_NODE_NAME)) && !(name = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION))
		&& !(name = spa_dict_lookup(props, PW_KEY_APP_NAME))) {
		return;
	}

	auto proxy = static_cast< pw_proxy * >(pw_registry_bind(m_registry, id, NODE_TYPE_ID, PW_VERSION_NODE, 0));
	if (!proxy) {
		return;
	}

	const auto lock = locker();

	if (const auto ret = m_nodes.emplace(id, Node(proxy, serial, name)); ret.second) {
		auto &node = ret.first->second;
		lib.proxy_add_object_listener(node.proxy, &node.listener, &eventsNode, &node);
	}
}

void BE_Engine::removeNode(const uint32_t id) {
	const auto lock = locker();

	m_nodes.erase(id);
}

BE_Engine::Node::Node(Node &&node)
	: proxy(std::exchange(node.proxy, nullptr)), listener(std::exchange(node.listener, {})), serial(node.serial),
	  name(std::move(node.name)), direction(node.direction) {
}

BE_Engine::Node::Node(pw_proxy *proxy, const uint32_t serial, const char *name)
	: proxy(proxy), listener(), serial(serial), name(name), direction(CROSSAUDIO_DIR_NONE) {
}

BE_Engine::Node::~Node() {
	spa_hook_remove(&listener);

	if (proxy) {
		lib.proxy_destroy(proxy);
	}
}

static constexpr pw_stream_events eventsInput  = { PW_VERSION_STREAM_EVENTS,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   processInput,
												   nullptr,
												   nullptr,
												   nullptr };
static constexpr pw_stream_events eventsOutput = { PW_VERSION_STREAM_EVENTS,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   nullptr,
												   processOutput,
												   nullptr,
												   nullptr,
												   nullptr };

BE_Flux::BE_Flux(BE_Engine &engine) : m_engine(engine), m_stream(nullptr), m_frameSize(0) {
	if (engine.m_core) {
		const auto lock = engine.locker();
		m_stream        = lib.stream_new(engine.m_core, nullptr, nullptr);
	}
}

BE_Flux::~BE_Flux() {
	if (m_stream) {
		const auto lock = m_engine.locker();
		lib.stream_destroy(m_stream);
	}
}

ErrorCode BE_Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	if (lib.stream_get_state(m_stream, nullptr) != PW_STREAM_STATE_UNCONNECTED) {
		return CROSSAUDIO_EC_INIT;
	}

	m_feedback = feedback;

	pw_direction direction;

	switch (config.direction) {
		case CROSSAUDIO_DIR_IN:
			direction = PW_DIRECTION_INPUT;
			break;
		case CROSSAUDIO_DIR_OUT:
			direction = PW_DIRECTION_OUTPUT;
			break;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	m_frameSize = config.sampleBits / 8 * config.channels;

	auto info = configToInfo(config);

	std::byte buffer[1024];
	spa_pod_builder b     = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	const spa_pod *params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

	spa_dict_item items[] = { { PW_KEY_MEDIA_TYPE, "Audio" },
							  { PW_KEY_MEDIA_CATEGORY, direction == PW_DIRECTION_INPUT ? "Capture" : "Playback" },
							  { PW_KEY_TARGET_OBJECT, config.node } };
	const spa_dict dict   = SPA_DICT_INIT_ARRAY(items);

	uint32_t flags = PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS;
	if (config.node) {
		flags |= PW_STREAM_FLAG_AUTOCONNECT;
	}

	const auto lock = m_engine.locker();

	lib.stream_update_properties(m_stream, &dict);
	lib.stream_add_listener(m_stream, &m_listener, direction == PW_DIRECTION_INPUT ? &eventsInput : &eventsOutput,
							this);
	lib.stream_connect(m_stream, direction, PW_ID_ANY, flags, &params, 1);

	return CROSSAUDIO_EC_OK;
}

ErrorCode BE_Flux::stop() {
	const auto lock = m_engine.locker();

	lib.stream_disconnect(m_stream);
	spa_hook_remove(&m_listener);

	return CROSSAUDIO_EC_OK;
}

const char *BE_Flux::nameGet() const {
	if (const auto props = lib.stream_get_properties(m_stream)) {
		return lib.properties_get(props, PW_KEY_NODE_NAME);
	}

	return nullptr;
}

ErrorCode BE_Flux::nameSet(const char *name) {
	const spa_dict_item items[] = { { PW_KEY_NODE_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = m_engine.locker();

	return lib.stream_update_properties(m_stream, &dict) >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

static void processInput(void *userData) {
	auto &flux = *static_cast< BE_Flux * >(userData);

	pw_buffer *buf = lib.stream_dequeue_buffer(flux.m_stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->chunk->size / data->chunk->stride };

	flux.m_feedback.process(flux.m_feedback.userData, &fluxData);

	lib.stream_queue_buffer(flux.m_stream, buf);
}

static void processOutput(void *userData) {
	auto &flux = *static_cast< BE_Flux * >(userData);

	pw_buffer *buf = lib.stream_dequeue_buffer(flux.m_stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->maxsize / flux.m_frameSize };

	flux.m_feedback.process(flux.m_feedback.userData, &fluxData);

	data->chunk->size   = fluxData.frames * flux.m_frameSize;
	data->chunk->stride = flux.m_frameSize;

	lib.stream_queue_buffer(flux.m_stream, buf);
}

static constexpr spa_audio_info_raw configToInfo(const FluxConfig &config) {
	spa_audio_info_raw info = {};

	info.format   = translateFormat(config.bitFormat, config.sampleBits);
	info.channels = config.channels;
	info.rate     = config.sampleRate;

	for (uint8_t i = 0; i < SPA_MIN(config.channels, CROSSAUDIO_ARRAY_SIZE(config.position)); ++i) {
		info.position[i] = translateChannel(config.position[i]);
	}

	return info;
}

static constexpr spa_audio_format translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits) {
	switch (format) {
		default:
		case CROSSAUDIO_BF_NONE:
			break;
		case CROSSAUDIO_BF_INTEGER_SIGNED:
			switch (sampleBits) {
				case 8:
					return SPA_AUDIO_FORMAT_S8;
				case 16:
					return SPA_AUDIO_FORMAT_S16;
				case 18:
					return SPA_AUDIO_FORMAT_S18;
				case 20:
					return SPA_AUDIO_FORMAT_S20;
				case 24:
					return SPA_AUDIO_FORMAT_S24;
				case 32:
					return SPA_AUDIO_FORMAT_S32;
			}

			break;
		case CROSSAUDIO_BF_INTEGER_UNSIGNED:
			switch (sampleBits) {
				case 8:
					return SPA_AUDIO_FORMAT_U8;
				case 16:
					return SPA_AUDIO_FORMAT_U16;
				case 18:
					return SPA_AUDIO_FORMAT_U18;
				case 20:
					return SPA_AUDIO_FORMAT_U20;
				case 24:
					return SPA_AUDIO_FORMAT_U24;
				case 32:
					return SPA_AUDIO_FORMAT_U32;
			}

			break;
		case CROSSAUDIO_BF_FLOAT:
			switch (sampleBits) {
				case 32:
					return SPA_AUDIO_FORMAT_F32;
				case 64:
					return SPA_AUDIO_FORMAT_F64;
			}

			break;
	}

	return SPA_AUDIO_FORMAT_UNKNOWN;
}

static constexpr spa_audio_channel translateChannel(const CrossAudio_Channel channel) {
	switch (channel) {
		default:
		case CROSSAUDIO_CH_NONE:
			return SPA_AUDIO_CHANNEL_UNKNOWN;
		case CROSSAUDIO_CH_FRONT_LEFT:
			return SPA_AUDIO_CHANNEL_FL;
		case CROSSAUDIO_CH_FRONT_RIGHT:
			return SPA_AUDIO_CHANNEL_FR;
		case CROSSAUDIO_CH_FRONT_CENTER:
			return SPA_AUDIO_CHANNEL_FC;
		case CROSSAUDIO_CH_LOW_FREQUENCY:
			return SPA_AUDIO_CHANNEL_LFE;
		case CROSSAUDIO_CH_REAR_LEFT:
			return SPA_AUDIO_CHANNEL_RL;
		case CROSSAUDIO_CH_REAR_RIGHT:
			return SPA_AUDIO_CHANNEL_RR;
		case CROSSAUDIO_CH_FRONT_LEFT_CENTER:
			return SPA_AUDIO_CHANNEL_FLC;
		case CROSSAUDIO_CH_FRONT_RIGHT_CENTER:
			return SPA_AUDIO_CHANNEL_FRC;
		case CROSSAUDIO_CH_REAR_CENTER:
			return SPA_AUDIO_CHANNEL_RC;
		case CROSSAUDIO_CH_SIDE_LEFT:
			return SPA_AUDIO_CHANNEL_SL;
		case CROSSAUDIO_CH_SIDE_RIGHT:
			return SPA_AUDIO_CHANNEL_SR;
		case CROSSAUDIO_CH_TOP_CENTER:
			return SPA_AUDIO_CHANNEL_TC;
		case CROSSAUDIO_CH_TOP_FRONT_LEFT:
			return SPA_AUDIO_CHANNEL_TFL;
		case CROSSAUDIO_CH_TOP_FRONT_CENTER:
			return SPA_AUDIO_CHANNEL_TFC;
		case CROSSAUDIO_CH_TOP_FRONT_RIGHT:
			return SPA_AUDIO_CHANNEL_TFR;
		case CROSSAUDIO_CH_TOP_REAR_LEFT:
			return SPA_AUDIO_CHANNEL_TRL;
		case CROSSAUDIO_CH_TOP_REAR_CENTER:
			return SPA_AUDIO_CHANNEL_TRC;
		case CROSSAUDIO_CH_TOP_REAR_RIGHT:
			return SPA_AUDIO_CHANNEL_TRR;
	}
}
