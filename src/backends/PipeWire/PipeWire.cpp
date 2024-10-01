// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "PipeWire.hpp"

#include "EventManager.hpp"
#include "Library.hpp"

#include "Node.h"

#include "crossaudio/Macros.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include <spa/param/audio/raw-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/dict.h>

#include <pipewire/core.h>
#include <pipewire/keys.h>
#include <pipewire/node.h>
#include <pipewire/stream.h>

using namespace pipewire;

using FluxData = CrossAudio_FluxData;

static auto toImpl(BE_Engine *engine) {
	return reinterpret_cast< Engine * >(engine);
}

static auto toImpl(BE_Flux *flux) {
	return reinterpret_cast< Flux * >(flux);
}

static const char *name() {
	return "PipeWire";
}

static const char *version() {
	return lib().get_library_version();
}

static ErrorCode init() {
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
		if ((ret = lib().load(name)) != CROSSAUDIO_EC_LIBRARY) {
			break;
		}
	}

	if (lib()) {
		lib().init(nullptr, nullptr);
	}

	return ret;
}

static ErrorCode deinit() {
	lib().deinit();
	lib().unload();

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

static ErrorCode engineStart(BE_Engine *engine, const EngineFeedback *feedback) {
	return toImpl(engine)->start(feedback ? *feedback : EngineFeedback());
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

static Nodes *engineNodesGet(BE_Engine *engine) {
	return toImpl(engine)->engineNodesGet();
}

static BE_Flux *fluxNew(BE_Engine *engine) {
	if (auto flux = new Flux(*toImpl(engine))) {
		if (*flux) {
			return reinterpret_cast< BE_Flux * >(flux);
		}

		delete flux;
	}

	return nullptr;
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

	fluxNew,
	fluxFree,
	fluxStart,
	fluxStop,
	fluxPause,
	fluxNameGet,
	fluxNameSet
};
// clang-format on

Engine::Engine() : m_threadLoop(nullptr), m_context(nullptr), m_core(nullptr) {
	if ((m_threadLoop = lib().thread_loop_new(nullptr, nullptr))) {
		m_context = lib().context_new(lib().thread_loop_get_loop(m_threadLoop), nullptr, 0);
	}
}

Engine::~Engine() {
	stop();

	if (m_context) {
		const auto lock = locker();
		lib().context_destroy(m_context);
	}

	if (m_threadLoop) {
		lib().thread_loop_destroy(m_threadLoop);
	}
}

void Engine::lock() {
	if (m_threadLoop) {
		lib().thread_loop_lock(m_threadLoop);
	}
}

void Engine::unlock() {
	if (m_threadLoop) {
		lib().thread_loop_unlock(m_threadLoop);
	}
}

ErrorCode Engine::start(const EngineFeedback &feedback) {
	const EventManager::Feedback eventManagerFeedback{ .nodeAdded = [this](const uint32_t id) { addNode(id); },
													   .nodeRemoved = [this](const uint32_t id) { removeNode(id); },
													   .nodeUpdated =
														   [this](const pw_node_info *info) { updateNode(info); } };

	if (m_core) {
		return CROSSAUDIO_EC_INIT;
	}

	if (m_core = lib().context_connect(m_context, nullptr, 0); !m_core) {
		return CROSSAUDIO_EC_CONNECT;
	}

	m_feedback     = feedback;
	m_eventManager = std::make_unique< EventManager >(m_core, eventManagerFeedback);

	if (lib().thread_loop_start(m_threadLoop) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
	lock();

	m_eventManager.reset();

	m_nodes.clear();

	if (m_core) {
		lib().core_disconnect(m_core);
		m_core = nullptr;
	}

	unlock();

	if (m_threadLoop) {
		lib().thread_loop_stop(m_threadLoop);
	}

	return CROSSAUDIO_EC_OK;
}

const char *Engine::nameGet() const {
	if (const auto props = m_core ? lib().core_get_properties(m_core) : lib().context_get_properties(m_context)) {
		return lib().properties_get(props, PW_KEY_APP_NAME);
	}

	return nullptr;
}

ErrorCode Engine::nameSet(const char *name) {
	const spa_dict_item items[] = { { PW_KEY_APP_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = locker();

	auto ret = lib().context_update_properties(m_context, &dict);
	if (ret >= 1 && m_core) {
		ret = lib().core_update_properties(m_core, &dict);
	}

	return ret >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

Nodes *Engine::engineNodesGet() {
	const auto lock = locker();

	auto nodes = nodesNew(m_nodes.size());

	size_t i = 0;

	for (const auto &iter : m_nodes) {
		const auto &nodeIn = iter.second;
		if (!nodeIn.advertised) {
			continue;
		}

		auto &nodeOut = nodes->items[i++];

		nodeOut.id        = strdup(nodeIn.id.data());
		nodeOut.name      = strdup(nodeIn.name.data());
		nodeOut.direction = nodeIn.direction;
	}

	return nodes;
}

void Engine::addNode(const uint32_t id) {
	lock();
	m_nodes.try_emplace(id, Node());
	unlock();
}

void Engine::removeNode(const uint32_t id) {
	lock();
	const auto iter = m_nodes.extract(id);
	unlock();

	if (!iter.empty() && m_feedback.nodeRemoved) {
		const Node &node  = iter.mapped();
		::Node *nodeNotif = nodeNew();

		nodeNotif->id        = strdup(node.id.data());
		nodeNotif->name      = strdup(node.name.data());
		nodeNotif->direction = node.direction;

		m_feedback.nodeRemoved(m_feedback.userData, nodeNotif);
	}
}

void Engine::updateNode(const pw_node_info *info) {
	const auto lock = locker();

	const auto iter = m_nodes.find(info->id);
	if (iter == m_nodes.cend()) {
		return;
	}

	auto &node = iter->second;
	if (node.advertised) {
		return;
	}

	if (node.id.empty()) {
		const char *id = spa_dict_lookup(info->props, PW_KEY_NODE_NAME);
		if (id) {
			node.id = id;
		}
	}

	if (node.name.empty()) {
		const char *name = spa_dict_lookup(info->props, PW_KEY_NODE_DESCRIPTION);
		if (name) {
			node.name = name;
		}
	}

	if (!(info->n_input_ports || info->n_output_ports) || node.id.empty()) {
		// Don't advertise the node if it has no ports or ID.
		return;
	}

	uint8_t direction = CROSSAUDIO_DIR_NONE;
	if (info->n_input_ports > 0) {
		direction |= CROSSAUDIO_DIR_OUT;
	}
	if (info->n_output_ports > 0) {
		direction |= CROSSAUDIO_DIR_IN;
	}

	node.direction = static_cast< Direction >(direction);

	if (m_feedback.nodeAdded) {
		::Node *nodeNotif = nodeNew();

		nodeNotif->id        = strdup(node.id.data());
		nodeNotif->name      = strdup(node.name.data());
		nodeNotif->direction = node.direction;

		m_feedback.nodeAdded(m_feedback.userData, nodeNotif);

		node.advertised = true;
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

Flux::Flux(Engine &engine) : m_engine(engine), m_stream(nullptr), m_frameSize(0) {
	if (engine.m_core) {
		const auto lock = engine.locker();
		m_stream        = lib().stream_new(engine.m_core, nullptr, nullptr);
	}
}

Flux::~Flux() {
	if (m_stream) {
		const auto lock = m_engine.locker();
		lib().stream_destroy(m_stream);
	}
}

ErrorCode Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	if (lib().stream_get_state(m_stream, nullptr) != PW_STREAM_STATE_UNCONNECTED) {
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

	lib().stream_update_properties(m_stream, &dict);
	lib().stream_add_listener(m_stream, &m_listener, direction == PW_DIRECTION_INPUT ? &eventsInput : &eventsOutput,
							  this);
	lib().stream_connect(m_stream, direction, PW_ID_ANY, flags, &params, 1);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::stop() {
	const auto lock = m_engine.locker();

	lib().stream_disconnect(m_stream);
	spa_hook_remove(&m_listener);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::pause(const bool on) {
	const auto lock = m_engine.locker();

	lib().stream_set_active(m_stream, !on);

	return CROSSAUDIO_EC_OK;
}

const char *Flux::nameGet() const {
	if (const auto props = lib().stream_get_properties(m_stream)) {
		return lib().properties_get(props, PW_KEY_NODE_NAME);
	}

	return nullptr;
}

ErrorCode Flux::nameSet(const char *name) {
	const spa_dict_item items[] = { { PW_KEY_NODE_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = m_engine.locker();

	return lib().stream_update_properties(m_stream, &dict) >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

static void processInput(void *userData) {
	auto &flux = *static_cast< Flux * >(userData);

	pw_buffer *buf = lib().stream_dequeue_buffer(flux.m_stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->chunk->size / data->chunk->stride };

	flux.m_feedback.process(flux.m_feedback.userData, &fluxData);

	lib().stream_queue_buffer(flux.m_stream, buf);
}

static void processOutput(void *userData) {
	auto &flux = *static_cast< Flux * >(userData);

	pw_buffer *buf = lib().stream_dequeue_buffer(flux.m_stream);
	if (!buf) {
		return;
	}

	spa_data *data = &buf->buffer->datas[0];
	if (!data->data) {
		return;
	}

	FluxData fluxData = { data->data, data->maxsize / flux.m_frameSize };

	flux.m_feedback.process(flux.m_feedback.userData, &fluxData);

	if (fluxData.frames) {
		data->chunk->size = fluxData.frames * flux.m_frameSize;
	} else {
		// Telling PipeWire that we wrote 0 bytes results in an xrun,
		// which in turn results in this function being called continuously.
		memset(data->data, 0, data->maxsize);
		data->chunk->size = data->maxsize;
	}

	data->chunk->stride = flux.m_frameSize;

	lib().stream_queue_buffer(flux.m_stream, buf);
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
				case 24:
					return SPA_AUDIO_FORMAT_S24_32;
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
				case 24:
					return SPA_AUDIO_FORMAT_U24_32;
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
