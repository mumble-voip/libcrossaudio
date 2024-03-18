// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_OSS_PAUSEFLAG_HPP
#define CROSSAUDIO_SRC_BACKENDS_OSS_PAUSEFLAG_HPP

#include <atomic>
#include <condition_variable>
#include <mutex>

class PauseFlag {
public:
	PauseFlag() : m_paused(false) {}
	~PauseFlag() {}

	explicit operator bool() const { return m_paused; }

	void set(const bool value) {
		m_paused = value;
		m_change.notify_all();
	}

	void wait(const bool value) {
		std::mutex mutex;
		std::unique_lock lock(mutex);
		m_change.wait(lock, [this, value] { return m_paused == value; });
	}

private:
	std::atomic_bool m_paused;
	std::condition_variable m_change;
};

#endif
