// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "EventManager.hpp"

#include "Device.hpp"

#include "Node.h"

using namespace wasapi;

EventManager::EventManager(IMMDeviceEnumerator *enumerator, const Feedback &feedback)
	: m_feedback(feedback), m_ref(0), m_enumerator(enumerator) {
	m_enumerator->AddRef();
	m_enumerator->RegisterEndpointNotificationCallback(this);
}

EventManager::~EventManager() {
	m_enumerator->UnregisterEndpointNotificationCallback(this);
	m_enumerator->Release();
}

ULONG STDMETHODCALLTYPE EventManager::AddRef() {
	return InterlockedIncrement(&m_ref);
}

ULONG STDMETHODCALLTYPE EventManager::Release() {
	return InterlockedDecrement(&m_ref);
}

HRESULT STDMETHODCALLTYPE EventManager::QueryInterface(const IID &iid, void **iface) {
	if (iid == IID_IUnknown) {
		*iface = static_cast< IUnknown * >(this);
		AddRef();
	} else if (iid == __uuidof(IMMNotificationClient)) {
		*iface = static_cast< IMMNotificationClient * >(this);
		AddRef();
	} else {
		*iface = nullptr;
		return E_NOINTERFACE;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE EventManager::OnDeviceAdded(const wchar_t *id) {
	IMMDevice *device;
	if (m_enumerator->GetDevice(id, &device) != S_OK) {
		return S_OK;
	}

	Node *node = nodeNew();
	if (populateNode(*node, *device, id)) {
		m_feedback.nodeAdded(node);
	}

	device->Release();

	return S_OK;
}

HRESULT STDMETHODCALLTYPE EventManager::OnDeviceRemoved(const wchar_t *id) {
	IMMDevice *device;
	if (m_enumerator->GetDevice(id, &device) != S_OK) {
		return S_OK;
	}

	Node *node = nodeNew();
	if (populateNode(*node, *device, id)) {
		m_feedback.nodeRemoved(node);
	}

	device->Release();

	return S_OK;
}

HRESULT STDMETHODCALLTYPE EventManager::OnDeviceStateChanged(const wchar_t *id, DWORD state) {
	switch (state) {
		case DEVICE_STATE_ACTIVE:
			return OnDeviceAdded(id);
		case DEVICE_STATE_DISABLED:
		case DEVICE_STATE_NOTPRESENT:
		case DEVICE_STATE_UNPLUGGED:
			return OnDeviceRemoved(id);
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE EventManager::OnDefaultDeviceChanged(EDataFlow, ERole, const wchar_t *) {
	return S_OK;
}

HRESULT STDMETHODCALLTYPE EventManager::OnPropertyValueChanged(const wchar_t *, const PROPERTYKEY) {
	return S_OK;
}
