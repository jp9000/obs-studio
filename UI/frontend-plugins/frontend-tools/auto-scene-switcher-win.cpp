#include <windows.h>
#include <util/platform.h>
#include "auto-scene-switcher.hpp"

using namespace std;

static bool GetWindowTitle(HWND window, string &title)
{
	size_t len = (size_t)GetWindowTextLengthW(window);
	wstring wtitle;

	wtitle.resize(len);
	if (!GetWindowTextW(window, &wtitle[0], (int)len + 1))
		return false;

	len = os_wcs_to_utf8(wtitle.c_str(), 0, nullptr, 0);
	title.resize(len);
	os_wcs_to_utf8(wtitle.c_str(), 0, &title[0], len + 1);
	return true;
}

static bool WindowValid(HWND window)
{
	LONG_PTR styles, ex_styles;
	RECT rect;
	DWORD id;

	if (!IsWindowVisible(window))
		return false;
	GetWindowThreadProcessId(window, &id);
	if (id == GetCurrentProcessId())
		return false;

	GetClientRect(window, &rect);
	styles = GetWindowLongPtr(window, GWL_STYLE);
	ex_styles = GetWindowLongPtr(window, GWL_EXSTYLE);

	if (ex_styles & WS_EX_TOOLWINDOW)
		return false;
	if (styles & WS_CHILD)
		return false;

	return true;
}

void GetWindowList(vector<string> &windows)
{
	HWND window = GetWindow(GetDesktopWindow(), GW_CHILD);

	while (window) {
		string title;
		if (WindowValid(window) && GetWindowTitle(window, title))
			windows.emplace_back(title);
		window = GetNextWindow(window, GW_HWNDNEXT);
	}
}

void GetCurrentWindowTitle(string &title)
{
	HWND window = GetForegroundWindow();
	DWORD id;

	GetWindowThreadProcessId(window, &id);
	if (id == GetCurrentProcessId()) {
		title = "";
		return;
	}
	GetWindowTitle(window, title);
}

void CleanupSceneSwitcher() {}
