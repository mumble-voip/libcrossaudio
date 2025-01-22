// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Device.hpp"

#include "Node.h"

#include <cstdlib>
#include <cstring>

#include <Windows.h>

#include <audioclient.h>
#include <functiondiscoverykeys.h>
#include <mmdeviceapi.h>

using Direction = CrossAudio_Direction;

namespace wasapi {
static Direction getDirection(IMMDevice &device) {
	IMMEndpoint *endpoint;
	if (device.QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast< void ** >(&endpoint)) != S_OK) {
		return CROSSAUDIO_DIR_NONE;
	}

	EDataFlow dataflow;
	if (endpoint->GetDataFlow(&dataflow) != S_OK) {
		dataflow = EDataFlow_enum_count;
	}

	endpoint->Release();

	switch (dataflow) {
		case eRender:
			return CROSSAUDIO_DIR_OUT;
		case eCapture:
			return CROSSAUDIO_DIR_IN;
		case eAll:
			return CROSSAUDIO_DIR_BOTH;
		case EDataFlow_enum_count:
			break;
	}

	return CROSSAUDIO_DIR_NONE;
}

static char *getID(IMMDevice &device) {
	wchar_t *deviceID;
	if (device.GetId(&deviceID) != S_OK) {
		return nullptr;
	}

	char *id = utf16To8(deviceID);

	CoTaskMemFree(deviceID);

	return id;
}

static char *getName(IMMDevice &device) {
	IPropertyStore *store;
	if (device.OpenPropertyStore(STGM_READ, &store) != S_OK) {
		return nullptr;
	}

	char *name = nullptr;

	PROPVARIANT varName{};
	if (store->GetValue(PKEY_Device_FriendlyName, &varName) == S_OK) {
		name = utf16To8(varName.pwszVal);
		PropVariantClear(&varName);
	}

	store->Release();

	return name;
}

bool populateNode(Node &node, IMMDevice &device, const wchar_t *id) {
	if (!(node.id = id ? utf16To8(id) : getID(device))) {
		return false;
	}

	node.name      = getName(device);
	node.direction = getDirection(device);

	return true;
}

char *utf16To8(const wchar_t *utf16) {
	const auto utf16Size = static_cast< int >(wcslen(utf16) + 1);

	const auto utf8Size = WideCharToMultiByte(CP_UTF8, 0, utf16, utf16Size, nullptr, 0, nullptr, nullptr);
	auto utf8           = static_cast< char           *>(malloc(utf8Size));
	if (WideCharToMultiByte(CP_UTF8, 0, utf16, utf16Size, utf8, utf8Size, nullptr, nullptr) <= 0) {
		free(utf8);
		return nullptr;
	}

	return utf8;
}

wchar_t *utf8To16(const char *utf8) {
	const auto utf8Size = static_cast< int >(strlen(utf8) + 1);

	const int utf16Size = sizeof(wchar_t) * MultiByteToWideChar(CP_UTF8, 0, utf8, utf8Size, nullptr, 0);
	auto utf16          = static_cast< wchar_t          *>(malloc(utf16Size));
	if (MultiByteToWideChar(CP_UTF8, 0, utf8, utf8Size, utf16, utf16Size / sizeof(wchar_t)) <= 0) {
		free(utf16);
		return nullptr;
	}

	return utf16;
}
} // namespace wasapi
