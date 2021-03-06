#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>

#include <unordered_set>
#include <map>
#include <pthread.h>

#include <obs-module.h>
#include <util/platform.h>

#include "xcompcap-helper.hpp"

namespace XCompcap {
static Display *xdisplay = 0;

Display *disp()
{
	if (!xdisplay)
		xdisplay = XOpenDisplay(NULL);

	return xdisplay;
}

void cleanupDisplay()
{
	if (!xdisplay)
		return;

	XCloseDisplay(xdisplay);
	xdisplay = 0;
}

// Specification for checking for ewmh support at
// http://standards.freedesktop.org/wm-spec/wm-spec-latest.html#idm140200472693600

bool ewmhIsSupported()
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

std::list<Window> getTopLevelWindows()
{
	std::list<Window> res;

	if (!ewmhIsSupported()) {
		blog(LOG_WARNING, "Unable to query window list "
				  "because window manager "
				  "does not support extended "
				  "window manager Hints");
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
			blog(LOG_WARNING, "Failed getting root "
					  "window properties");
			continue;
		}

		for (unsigned long i = 0; i < num; ++i)
			res.push_back(data[i]);

		XFree(data);
	}

	return res;
}

int getRootWindowScreen(Window root)
{
	XWindowAttributes attr;

	if (!XGetWindowAttributes(disp(), root, &attr))
		return DefaultScreen(disp());

	return XScreenNumberOfScreen(attr.screen);
}

std::string getWindowAtom(Window win, const char *atom)
{
	Atom netWmName = XInternAtom(disp(), atom, false);
	int n;
	char **list = 0;
	XTextProperty tp;
	std::string res = "unknown";

	XGetTextProperty(disp(), win, &tp, netWmName);

	if (!tp.nitems)
		XGetWMName(disp(), win, &tp);

	if (!tp.nitems)
		return "error";

	if (tp.encoding == XA_STRING) {
		res = (char *)tp.value;
	} else {
		int ret = XmbTextPropertyToTextList(disp(), &tp, &list, &n);

		if (ret >= Success && n > 0 && *list) {
			res = *list;
			XFreeStringList(list);
		}
	}

	char *conv = nullptr;
	if (os_mbs_to_utf8_ptr(res.c_str(), 0, &conv))
		res = conv;
	bfree(conv);

	XFree(tp.value);

	return res;
}

static std::map<XCompcapMain *, Window> windowForSource;
static std::unordered_set<XCompcapMain *> changedSources;
static pthread_mutex_t changeLock = PTHREAD_MUTEX_INITIALIZER;

void registerSource(XCompcapMain *source, Window win)
{
	PLock lock(&changeLock);

	blog(LOG_DEBUG, "registerSource(source=%p, win=%ld)", source, win);

	auto it = windowForSource.find(source);

	if (it != windowForSource.end()) {
		windowForSource.erase(it);
	}

	// Subscribe to Events
	XSelectInput(disp(), win,
		     StructureNotifyMask | ExposureMask | VisibilityChangeMask);
	XCompositeRedirectWindow(disp(), win, CompositeRedirectAutomatic);
	XSync(disp(), 0);

	windowForSource.insert(std::make_pair(source, win));
}

void unregisterSource(XCompcapMain *source)
{
	PLock lock(&changeLock);

	blog(LOG_DEBUG, "unregisterSource(source=%p)", source);
	{
		auto it = windowForSource.find(source);
		Window win = it->second;

		if (it != windowForSource.end()) {
			windowForSource.erase(it);
		}

		// check if there are still sources listening for the same window
		it = windowForSource.begin();
		bool windowInUse = false;
		while (it != windowForSource.end()) {
			if (it->second == win) {
				windowInUse = true;
				break;
			}
			it++;
		}

		if (!windowInUse) {
			// Last source released, stop listening for events.
			XSelectInput(disp(), win, 0);
			XCompositeUnredirectWindow(disp(), win,
						   CompositeRedirectAutomatic);
			XSync(disp(), 0);
		}
	}

	{
		auto it = changedSources.find(source);

		if (it != changedSources.end()) {
			changedSources.erase(it);
		}
	}
}

void processEvents()
{
	PLock lock(&changeLock);

	XLockDisplay(disp());

	while (XEventsQueued(disp(), QueuedAfterReading) > 0) {
		XEvent ev;
		Window win = 0;

		XNextEvent(disp(), &ev);

		if (ev.type == ConfigureNotify)
			win = ev.xconfigure.event;

		else if (ev.type == MapNotify)
			win = ev.xmap.event;

		else if (ev.type == Expose)
			win = ev.xexpose.window;

		else if (ev.type == VisibilityNotify)
			win = ev.xvisibility.window;

		else if (ev.type == DestroyNotify)
			win = ev.xdestroywindow.event;

		if (win != 0) {
			blog(LOG_DEBUG, "processEvents(): windowChanged=%ld",
			     win);

			auto it = windowForSource.begin();

			while (it != windowForSource.end()) {
				if (it->second == win) {
					blog(LOG_DEBUG,
					     "processEvents(): sourceChanged=%p",
					     it->first);
					changedSources.insert(it->first);
				}
				it++;
			}
		}
	}

	XUnlockDisplay(disp());
}

bool sourceWasReconfigured(XCompcapMain *source)
{
	PLock lock(&changeLock);

	auto it = changedSources.find(source);

	if (it != changedSources.end()) {
		changedSources.erase(it);
		blog(LOG_DEBUG, "sourceWasReconfigured(source=%p)=true",
		     source);
		return true;
	}

	return false;
}

}

PLock::PLock(pthread_mutex_t *mtx, bool trylock) : m(mtx)
{
	if (trylock)
		islock = mtx && pthread_mutex_trylock(mtx) == 0;
	else
		islock = mtx && pthread_mutex_lock(mtx) == 0;
}

PLock::~PLock()
{
	if (islock) {
		pthread_mutex_unlock(m);
	}
}

bool PLock::isLocked()
{
	return islock;
}

void PLock::unlock()
{
	if (islock) {
		pthread_mutex_unlock(m);
		islock = false;
	}
}

void PLock::lock()
{
	if (!islock) {
		pthread_mutex_lock(m);
		islock = true;
	}
}

static bool *curErrorTarget = 0;
static char curErrorText[200];
static int xerrorlock_handler(Display *disp, XErrorEvent *err)
{

	if (curErrorTarget)
		*curErrorTarget = true;

	XGetErrorText(disp, err->error_code, curErrorText, 200);

	return 0;
}

XErrorLock::XErrorLock()
{
	goterr = false;
	islock = false;
	prevhandler = 0;

	lock();
}

XErrorLock::~XErrorLock()
{
	unlock();
}

bool XErrorLock::isLocked()
{
	return islock;
}

void XErrorLock::lock()
{
	if (!islock) {
		XLockDisplay(XCompcap::disp());
		XSync(XCompcap::disp(), 0);

		curErrorTarget = &goterr;
		curErrorText[0] = 0;
		prevhandler = XSetErrorHandler(xerrorlock_handler);

		islock = true;
	}
}

void XErrorLock::unlock()
{
	if (islock) {
		XSync(XCompcap::disp(), 0);

		curErrorTarget = 0;
		XSetErrorHandler(prevhandler);
		prevhandler = 0;
		XUnlockDisplay(XCompcap::disp());
		islock = false;
	}
}

bool XErrorLock::gotError()
{
	if (!islock)
		return false;

	XSync(XCompcap::disp(), 0);

	bool res = goterr;
	goterr = false;
	return res;
}

std::string XErrorLock::getErrorText()
{
	return curErrorText;
}

void XErrorLock::resetError()
{
	if (islock)
		XSync(XCompcap::disp(), 0);

	goterr = false;
	curErrorText[0] = 0;
}

XDisplayLock::XDisplayLock()
{
	islock = false;
	lock();
}

XDisplayLock::~XDisplayLock()
{
	unlock();
}

bool XDisplayLock::isLocked()
{
	return islock;
}

void XDisplayLock::lock()
{
	if (!islock) {
		XLockDisplay(XCompcap::disp());
		islock = true;
	}
}

void XDisplayLock::unlock()
{
	if (islock) {
		XSync(XCompcap::disp(), 0);
		XUnlockDisplay(XCompcap::disp());
		islock = false;
	}
}

ObsGsContextHolder::ObsGsContextHolder()
{
	obs_enter_graphics();
}

ObsGsContextHolder::~ObsGsContextHolder()
{
	obs_leave_graphics();
}
