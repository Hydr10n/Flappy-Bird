//
// pch.h
// Header for standard system include files.
//

#pragma once

#include <winsdkver.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <sdkddkver.h>

// Use the C++ standard templated min/max
#define NOMINMAX

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>

#include <d2d1_1.h>
#include <dwrite.h>

#include <wrl.h>

#include <memory>

#include <string>

#include <algorithm>

#include <system_error>

namespace DX {
	[[noreturn]] inline void throw_std_system_error(int code, const char* message = "") {
		throw std::system_error(code, std::system_category(), message);
	}

	void ThrowIfFailed(std::same_as<BOOL> auto value, LPCSTR lpMessage = "") {
		if (!value) throw_std_system_error(static_cast<int>(GetLastError()), lpMessage);
	}

	void ThrowIfFailed(std::same_as<HRESULT> auto value, LPCSTR lpMessage = "") {
		if (FAILED(value)) throw_std_system_error(static_cast<int>(value), lpMessage);
	}
}
