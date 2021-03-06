#import <AppKit/AppKit.h>
#include <util/platform.h>
#include "auto-scene-switcher.hpp"

using namespace std;

void GetWindowList(vector<string> &windows)
{
	windows.resize(0);

	@autoreleasepool {
		NSWorkspace *ws = [NSWorkspace sharedWorkspace];
		NSArray *array = [ws runningApplications];
		for (NSRunningApplication *app in array) {
			NSString *name = app.localizedName;
			if (!name)
				continue;

			const char *str = name.UTF8String;
			if (str && *str)
				windows.emplace_back(str);
		}
	}
}

void GetCurrentWindowTitle(string &title)
{
	title.resize(0);

	@autoreleasepool {
		NSWorkspace *ws = [NSWorkspace sharedWorkspace];
		NSRunningApplication *app = [ws frontmostApplication];
		if (app) {
			NSString *name = app.localizedName;
			if (!name)
				return;

			const char *str = name.UTF8String;
			if (str && *str)
				title = str;
		}
	}
}

void CleanupSceneSwitcher() {}
