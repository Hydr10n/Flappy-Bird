module;
#include <Windows.h>

#include <memory>

export module D2DApp;

import WindowHelpers;

using namespace WindowHelpers;

export struct D2DApp {
	D2DApp(const std::shared_ptr<WindowModeHelper>& windowModeHelper) noexcept(false);
	~D2DApp();

	SIZE GetOutputSize() const noexcept;

	void Tick();

	void OnWindowSizeChanged();

	void OnResuming();
	void OnSuspending();

	void OnActivated();
	void OnDeactivated();

	void ProcessKeyboardMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void ProcessMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	struct Impl;
	const std::unique_ptr<Impl> m_impl;
};
