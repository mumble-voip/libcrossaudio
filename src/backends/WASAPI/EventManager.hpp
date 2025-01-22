// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_WASAPI_EVENTMANAGER_HPP
#define CROSSAUDIO_SRC_BACKENDS_WASAPI_EVENTMANAGER_HPP

#include <functional>

#include <mmdeviceapi.h>

using Node = struct CrossAudio_Node;

namespace wasapi {
class EventManager : public IMMNotificationClient {
public:
	struct Feedback {
		std::function< void(Node *node) > nodeAdded;
		std::function< void(Node *node) > nodeRemoved;
	};

	EventManager(IMMDeviceEnumerator *enumerator, const Feedback &feedback);
	virtual ~EventManager();

private:
	EventManager(const EventManager &)            = delete;
	EventManager &operator=(const EventManager &) = delete;

	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;

	HRESULT STDMETHODCALLTYPE QueryInterface(const IID &iid, void **iface) override;
	HRESULT STDMETHODCALLTYPE OnDeviceAdded(const wchar_t *id) override;
	HRESULT STDMETHODCALLTYPE OnDeviceRemoved(const wchar_t *id) override;
	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(const wchar_t *id, DWORD state) override;
	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, const wchar_t *id) override;
	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(const wchar_t *id, const PROPERTYKEY key) override;

	Feedback m_feedback;
	LONG m_ref;
	IMMDeviceEnumerator *m_enumerator;
};
} // namespace wasapi

#endif
