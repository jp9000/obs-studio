#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#undef Bool
#undef CursorShape
#undef Expose
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef None
#undef Status
#undef Unsorted
#include <util/platform.h>
#include "auto-scene-switcher.hpp"

using namespace std;

static Display *xdisplay = 0;

Display *disp()
{
	if (!xdisplay)
		xdisplay = XOpenDisplay(NULL);

	return xdisplay;
}

void CleanupSceneSwitcher()
{
	if (!xdisplay)
		return;

	XCloseDisplay(xdisplay);
	xdisplay = 0;
}

static bool ewmhIsSupported()
{
	Display *display = disp();
	Atom netSupportingWmCheck =
		XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", true);
	Atom actualType;
	int format = 0;
	unsigned long num = 0, bytes = 0;
	unsigned char *data = NULL;
	Window ewmh_window = 0;

	int status = XGetWindowProperty(display, DefaultRootWindow(display),
					netSupportingWmCheck, 0L, 1L, false,
					XA_WINDOW, &actualType, &format, &num,
					&bytes, &data);

	if (status == Success) {
		if (num > 0) {
			ewmh_window = ((Window *)data)[0];
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
	}

	if (ewmh_window) {
		status = XGetWindowProperty(display, ewmh_window,
					    netSupportingWmCheck, 0L, 1L, false,
					    XA_WINDOW, &actualType, &format,
					    &num, &bytes, &data);
		if (status != Success || num == 0 ||
		    ewmh_window != ((Window *)data)[0]) {
			ewmh_window = 0;
		}
		if (status == Success && data) {
			XFree(data);
		}
	}

	return ewmh_window != 0;
}

static std::vector<Window> getTopLevelWindows()
{
	std::vector<Window> res;

	res.resize(0);

	if (!ewmhIsSupported()) {
		return res;
	}

	Atom netClList = XInternAtom(disp(), "_NET_CLIENT_LIST", true);
	Atom actualType;
	int format;
	unsigned long num, bytes;
	Window *data = 0;

	for (int i = 0; i < ScreenCount(disp()); ++i) {
		Window rootWin = RootWindow(disp(), i);

		int status = XGetWindowProperty(disp(), rootWin, netClList, 0L,
						~0L, false, AnyPropertyType,
						&actualType, &format, &num,
						&bytes, (uint8_t **)&data);

		if (status != Success) {
			continue;
		}

		for (unsigned long i = 0; i < num; ++i)
			res.emplace_back(data[i]);

		XFree(data);
	}

	return res;
}

static std::string GetWindowTitle(size_t i)
{
	Window w = getTopLevelWindows().at(i);
	std::string windowTitle;
	char *name;

	int status = XFetchName(disp(), w, &name);
	if (status >= Success && name != nullptr) {
		std::string str(name);
		windowTitle = str;
		XFree(name);
	} else {
		XTextProperty xtp_new_name;
		if (XGetWMName(disp(), w, &xtp_new_name) != 0 &&
		    xtp_new_name.value != nullptr) {
			std::string str((const char *)xtp_new_name.value);
			windowTitle = str;
			XFree(xtp_new_name.value);
		}
	}

	return windowTitle;
}

void GetWindowList(vector<string> &windows)
{
	windows.resize(0);

	for (size_t i = 0; i < getTopLevelWindows().size(); ++i) {
		if (GetWindowTitle(i) != "")
			windows.emplace_back(GetWindowTitle(i));
	}
}

void GetCurrentWindowTitle(string &title)
{
	if (!ewmhIsSupported()) {
		return;
	}

	Atom active = XInternAtom(disp(), "_NET_ACTIVE_WINDOW", true);
	Atom actualType;
	int format;
	unsigned long num, bytes;
	Window *data = 0;
	char *name;

	Window rootWin = RootWindow(disp(), 0);

	XGetWindowProperty(disp(), rootWin, active, 0L, ~0L, false,
			   AnyPropertyType, &actualType, &format, &num, &bytes,
			   (uint8_t **)&data);

	int status = XFetchName(disp(), data[0], &name);

	if (status >= Success && name != nullptr) {
		std::string str(name);
		title = str;
	} else {
		XTextProperty xtp_new_name;
		if (XGetWMName(disp(), data[0], &xtp_new_name) != 0 &&
		    xtp_new_name.value != nullptr) {
			std::string str((const char *)xtp_new_name.value);
			title = str;
			XFree(xtp_new_name.value);
		}
	}

	XFree(name);
}
