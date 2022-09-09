#include "pch.h"

#include "resource.h"

#include <set>

import D2DApp;
import DisplayHelpers;
import SharedData;
import WindowHelpers;

using namespace DisplayHelpers;
using namespace DX;
using namespace Microsoft::WRL::Wrappers;
using namespace std;
using namespace WindowHelpers;

exception_ptr g_exception;

shared_ptr<WindowModeHelper> g_windowModeHelper;

unique_ptr<D2DApp> g_app;

int WINAPI wWinMain(
	[[maybe_unused]] _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE,
	[[maybe_unused]] _In_ LPWSTR lpCmdLine, [[maybe_unused]] _In_ int nShowCmd
) {
	int ret;
	
	try {
		LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
		const WNDCLASSEXW wndClassEx{
			.cbSize = sizeof(wndClassEx),
			.lpfnWndProc = WndProc,
			.hInstance = hInstance,
			.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON_DIRECTX)),
			.hCursor = LoadCursor(nullptr, IDC_ARROW),
			.lpszClassName = L"Direct2D"
		};
		ThrowIfFailed(static_cast<BOOL>(RegisterClassExW(&wndClassEx)));

		const auto window = CreateWindowExW(
			0,
			wndClassEx.lpszClassName,
			L"Flappy Bird",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			nullptr,
			nullptr,
			wndClassEx.hInstance,
			nullptr
		);
		ThrowIfFailed(static_cast<BOOL>(window != nullptr));

		g_windowModeHelper = make_shared<decltype(g_windowModeHelper)::element_type>(window);

		RECT clientRect;
		ThrowIfFailed(GetClientRect(window, &clientRect));

		g_windowModeHelper->SetResolution({ clientRect.right - clientRect.left, clientRect.bottom - clientRect.top });

		g_app = make_unique<decltype(g_app)::element_type>(g_windowModeHelper);

		ThrowIfFailed(g_windowModeHelper->Apply());

		MSG msg{ .message = WM_QUIT };
		do {
			if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);

				if (g_exception) rethrow_exception(g_exception);
			}
			else g_app->Tick();
		} while (msg.message != WM_QUIT);
		ret = static_cast<decltype(ret)>(msg.wParam);
	}
	catch (const system_error& e) {
		ret = e.code().value();
		MessageBoxA(nullptr, e.what(), nullptr, MB_OK | MB_ICONERROR);
	}
	catch (const exception& e) {
		ret = ERROR_CAN_NOT_COMPLETE;
		MessageBoxA(nullptr, e.what(), nullptr, MB_OK | MB_ICONERROR);
	}
	catch (...) {
		ret = static_cast<int>(GetLastError());
		if (ret != ERROR_SUCCESS) MessageBoxA(nullptr, system_category().message(ret).c_str(), nullptr, MB_OK | MB_ICONERROR);
	}

	g_app.reset();

	return ret;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	try {
		static HMONITOR s_hMonitor;

		static Resolution s_displayResolution;

		const auto GetDisplayResolutions = [&](bool forceUpdate = false) {
			const auto monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
			ThrowIfFailed(static_cast<BOOL>(monitor != nullptr));

			if (monitor != s_hMonitor || forceUpdate) {
				ThrowIfFailed(::GetDisplayResolutions(g_displayResolutions, monitor));

				/*if (const auto resolution = cbegin(g_displayResolutions)->IsPortrait() ? Resolution{ 600, 800 } : Resolution{ 800, 600 };
					*--cend(g_displayResolutions) > resolution) {
					erase_if(g_displayResolutions, [&](const auto& displayResolution) { return displayResolution < resolution; });
				}*/

				ThrowIfFailed(GetDisplayResolution(s_displayResolution, monitor));

				s_hMonitor = monitor;
			}
		};

		if (s_hMonitor == nullptr) GetDisplayResolutions();

		switch (uMsg) {
		case WM_GETMINMAXINFO: {
			if (lParam) {
				const auto style = GetWindowStyle(hWnd), exStyle = GetWindowExStyle(hWnd);
				const auto hasMenu = GetMenu(hWnd) != nullptr;
				const auto DPI = GetDpiForWindow(hWnd);

				const auto AdjustSize = [&](const auto& size, auto& newSize) {
					RECT rect{ 0, 0, size.cx, size.cy };
					if (AdjustWindowRectExForDpi(&rect, style, hasMenu, exStyle, DPI)) {
						newSize = { rect.right - rect.left, rect.bottom - rect.top };
					}
				};

				auto& minMaxInfo = *reinterpret_cast<PMINMAXINFO>(lParam);
				AdjustSize(*cbegin(g_displayResolutions), minMaxInfo.ptMinTrackSize);
				AdjustSize(s_displayResolution, minMaxInfo.ptMaxTrackSize);
			}
		} break;

		case WM_MOVE: GetDisplayResolutions(); break;

		case WM_MOVING:
		case WM_SIZING: g_app->Tick(); break;

		case WM_SIZE: {
			if (!g_app) return 0;

			switch (wParam) {
			case SIZE_MINIMIZED: g_app->OnSuspending(); break;

			case SIZE_RESTORED: g_app->OnResuming(); [[fallthrough]];
			default: {
				if (g_windowModeHelper->GetMode() != WindowMode::Fullscreen || g_windowModeHelper->IsFullscreenResolutionHandledByWindow()) {
					g_windowModeHelper->SetResolution({ LOWORD(lParam), HIWORD(lParam) });
				}

				g_app->OnWindowSizeChanged();
			} break;
			}
		} break;

		case WM_DISPLAYCHANGE: {
			GetDisplayResolutions(true);

			ThrowIfFailed(g_windowModeHelper->Apply());
		} break;

		case WM_DPICHANGED: {
			const auto& [left, top, right, bottom] = *reinterpret_cast<PRECT>(lParam);
			SetWindowPos(hWnd, nullptr, static_cast<int>(left), static_cast<int>(top), static_cast<int>(right - left), static_cast<int>(bottom - top), SWP_NOZORDER);
		} break;

		case WM_SYSKEYDOWN: {
			if (wParam == VK_RETURN && (HIWORD(lParam) & (KF_ALTDOWN | KF_REPEAT)) == KF_ALTDOWN) {
				g_windowModeHelper->ToggleMode();
				ThrowIfFailed(g_windowModeHelper->Apply());
			}
		} [[fallthrough]];
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: g_app->ProcessKeyboardMessage(uMsg, wParam, lParam); break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN: {
			SetCapture(hWnd);

			g_app->ProcessMouseMessage(uMsg, wParam, lParam);
		} break;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP: ReleaseCapture(); [[fallthrough]];
		case WM_INPUT:
		case WM_MOUSEMOVE:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHOVER: g_app->ProcessMouseMessage(uMsg, wParam, lParam); break;

			//case WM_MOUSEACTIVATE: return MA_ACTIVATEANDEAT;

		case WM_MENUCHAR: return MAKELRESULT(0, MNC_CLOSE);

		case WM_DESTROY: PostQuitMessage(ERROR_SUCCESS); break;

		default: return DefWindowProcW(hWnd, uMsg, wParam, lParam);
		}
	}
	catch (...) { g_exception = current_exception(); }

	return 0;
}
