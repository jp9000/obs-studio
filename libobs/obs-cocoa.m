/******************************************************************************
    Copyright (C) 2013 by Ruwen Hahn <palana@stunned.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "util/platform.h"
#include "util/dstr.h"
#include "obs.h"
#include "obs-internal.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDManager.h>

#import <AppKit/AppKit.h>

bool is_in_bundle()
{
	NSRunningApplication *app = [NSRunningApplication currentApplication];
	return [app bundleIdentifier] != nil;
}

const char *get_module_extension(void)
{
	return ".so";
}

static const char *module_bin[] = {
	"../obs-plugins",
	OBS_INSTALL_PREFIX "obs-plugins",
};

static const char *module_data[] = {
	"../data/obs-plugins/%module%",
	OBS_INSTALL_DATA_PATH "obs-plugins/%module%",
};

static const int module_patterns_size =
	sizeof(module_bin) / sizeof(module_bin[0]);

void add_default_module_paths(void)
{
	for (int i = 0; i < module_patterns_size; i++)
		obs_add_module_path(module_bin[i], module_data[i]);

	if (is_in_bundle()) {
		NSRunningApplication *app =
			[NSRunningApplication currentApplication];
		NSURL *bundleURL = [app bundleURL];
		NSURL *pluginsURL = [bundleURL
			URLByAppendingPathComponent:@"Contents/PlugIns"];
		NSURL *dataURL = [bundleURL
			URLByAppendingPathComponent:
				@"Contents/Resources/data/obs-plugins/%module%"];

		const char *binPath = [[pluginsURL path]
			cStringUsingEncoding:NSUTF8StringEncoding];
		const char *dataPath = [[dataURL path]
			cStringUsingEncoding:NSUTF8StringEncoding];

		obs_add_module_path(binPath, dataPath);
	}
}

char *find_libobs_data_file(const char *file)
{
	struct dstr path;

	if (is_in_bundle()) {
		NSRunningApplication *app =
			[NSRunningApplication currentApplication];
		NSURL *bundleURL = [app bundleURL];
		NSURL *libobsDataURL =
			[bundleURL URLByAppendingPathComponent:
					   @"Contents/Resources/data/libobs/"];
		const char *libobsDataPath = [[libobsDataURL path]
			cStringUsingEncoding:NSUTF8StringEncoding];
		dstr_init_copy(&path, libobsDataPath);
		dstr_cat(&path, "/");
	} else {
		dstr_init_copy(&path, OBS_INSTALL_DATA_PATH "/libobs/");
	}

	dstr_cat(&path, file);
	return path.array;
}

static void log_processor_name(void)
{
	char *name = NULL;
	size_t size;
	int ret;

	ret = sysctlbyname("machdep.cpu.brand_string", NULL, &size, NULL, 0);
	if (ret != 0)
		return;

	name = malloc(size);

	ret = sysctlbyname("machdep.cpu.brand_string", name, &size, NULL, 0);
	if (ret == 0)
		blog(LOG_INFO, "CPU Name: %s", name);

	free(name);
}

static void log_processor_speed(void)
{
	size_t size;
	long long freq;
	int ret;

	size = sizeof(freq);
	ret = sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0);
	if (ret == 0)
		blog(LOG_INFO, "CPU Speed: %lldMHz", freq / 1000000);
}

static void log_processor_cores(void)
{
	blog(LOG_INFO, "Physical Cores: %d, Logical Cores: %d",
	     os_get_physical_cores(), os_get_logical_cores());
}

static void log_available_memory(void)
{
	size_t size;
	long long memory_available;
	int ret;

	size = sizeof(memory_available);
	ret = sysctlbyname("hw.memsize", &memory_available, &size, NULL, 0);
	if (ret == 0)
		blog(LOG_INFO, "Physical Memory: %lldMB Total",
		     memory_available / 1024 / 1024);
}

static bool using_10_15_or_above = true;

static void log_os(void)
{
	NSProcessInfo *pi = [NSProcessInfo processInfo];
	blog(LOG_INFO, "OS Name: Mac OS X");
	blog(LOG_INFO, "OS Version: %s",
	     [[pi operatingSystemVersionString] UTF8String]);

	NSOperatingSystemVersion catalina = {10, 15, 0};
	using_10_15_or_above = [pi isOperatingSystemAtLeastVersion:catalina];
}

static void log_kernel_version(void)
{
	char kernel_version[1024];
	size_t size = sizeof(kernel_version);
	int ret;

	ret = sysctlbyname("kern.osrelease", kernel_version, &size, NULL, 0);
	if (ret == 0)
		blog(LOG_INFO, "Kernel Version: %s", kernel_version);
}

void log_system_info(void)
{
	log_processor_name();
	log_processor_speed();
	log_processor_cores();
	log_available_memory();
	log_os();
	log_kernel_version();
}

static bool dstr_from_cfstring(struct dstr *str, CFStringRef ref)
{
	CFIndex length = CFStringGetLength(ref);
	CFIndex max_size = CFStringGetMaximumSizeForEncoding(
		length, kCFStringEncodingUTF8);
	dstr_reserve(str, max_size);

	if (!CFStringGetCString(ref, str->array, max_size,
				kCFStringEncodingUTF8))
		return false;

	str->len = strlen(str->array);
	return true;
}

struct obs_hotkeys_platform {
	volatile long refs;
	bool secure_input_activated;
	TISInputSourceRef tis;
	CFDataRef layout_data;
	UCKeyboardLayout *layout;
	IOHIDManagerRef manager;
	DARRAY(IOHIDElementRef) keys[OBS_KEY_LAST_VALUE];
};

static void hotkeys_retain(struct obs_hotkeys_platform *plat)
{
	os_atomic_inc_long(&plat->refs);
}

static inline void free_hotkeys_platform(obs_hotkeys_platform_t *plat);
static void hotkeys_release(struct obs_hotkeys_platform *plat)
{
	if (os_atomic_dec_long(&plat->refs) == -1)
		free_hotkeys_platform(plat);
}

#define INVALID_KEY 0xff

int obs_key_to_virtual_key(obs_key_t code)
{
	switch (code) {
	case OBS_KEY_A:
		return kVK_ANSI_A;
	case OBS_KEY_B:
		return kVK_ANSI_B;
	case OBS_KEY_C:
		return kVK_ANSI_C;
	case OBS_KEY_D:
		return kVK_ANSI_D;
	case OBS_KEY_E:
		return kVK_ANSI_E;
	case OBS_KEY_F:
		return kVK_ANSI_F;
	case OBS_KEY_G:
		return kVK_ANSI_G;
	case OBS_KEY_H:
		return kVK_ANSI_H;
	case OBS_KEY_I:
		return kVK_ANSI_I;
	case OBS_KEY_J:
		return kVK_ANSI_J;
	case OBS_KEY_K:
		return kVK_ANSI_K;
	case OBS_KEY_L:
		return kVK_ANSI_L;
	case OBS_KEY_M:
		return kVK_ANSI_M;
	case OBS_KEY_N:
		return kVK_ANSI_N;
	case OBS_KEY_O:
		return kVK_ANSI_O;
	case OBS_KEY_P:
		return kVK_ANSI_P;
	case OBS_KEY_Q:
		return kVK_ANSI_Q;
	case OBS_KEY_R:
		return kVK_ANSI_R;
	case OBS_KEY_S:
		return kVK_ANSI_S;
	case OBS_KEY_T:
		return kVK_ANSI_T;
	case OBS_KEY_U:
		return kVK_ANSI_U;
	case OBS_KEY_V:
		return kVK_ANSI_V;
	case OBS_KEY_W:
		return kVK_ANSI_W;
	case OBS_KEY_X:
		return kVK_ANSI_X;
	case OBS_KEY_Y:
		return kVK_ANSI_Y;
	case OBS_KEY_Z:
		return kVK_ANSI_Z;

	case OBS_KEY_1:
		return kVK_ANSI_1;
	case OBS_KEY_2:
		return kVK_ANSI_2;
	case OBS_KEY_3:
		return kVK_ANSI_3;
	case OBS_KEY_4:
		return kVK_ANSI_4;
	case OBS_KEY_5:
		return kVK_ANSI_5;
	case OBS_KEY_6:
		return kVK_ANSI_6;
	case OBS_KEY_7:
		return kVK_ANSI_7;
	case OBS_KEY_8:
		return kVK_ANSI_8;
	case OBS_KEY_9:
		return kVK_ANSI_9;
	case OBS_KEY_0:
		return kVK_ANSI_0;

	case OBS_KEY_RETURN:
		return kVK_Return;
	case OBS_KEY_ESCAPE:
		return kVK_Escape;
	case OBS_KEY_BACKSPACE:
		return kVK_Delete;
	case OBS_KEY_TAB:
		return kVK_Tab;
	case OBS_KEY_SPACE:
		return kVK_Space;
	case OBS_KEY_MINUS:
		return kVK_ANSI_Minus;
	case OBS_KEY_EQUAL:
		return kVK_ANSI_Equal;
	case OBS_KEY_BRACKETLEFT:
		return kVK_ANSI_LeftBracket;
	case OBS_KEY_BRACKETRIGHT:
		return kVK_ANSI_RightBracket;
	case OBS_KEY_BACKSLASH:
		return kVK_ANSI_Backslash;
	case OBS_KEY_SEMICOLON:
		return kVK_ANSI_Semicolon;
	case OBS_KEY_QUOTE:
		return kVK_ANSI_Quote;
	case OBS_KEY_DEAD_GRAVE:
		return kVK_ANSI_Grave;
	case OBS_KEY_COMMA:
		return kVK_ANSI_Comma;
	case OBS_KEY_PERIOD:
		return kVK_ANSI_Period;
	case OBS_KEY_SLASH:
		return kVK_ANSI_Slash;
	case OBS_KEY_CAPSLOCK:
		return kVK_CapsLock;
	case OBS_KEY_SECTION:
		return kVK_ISO_Section;

	case OBS_KEY_F1:
		return kVK_F1;
	case OBS_KEY_F2:
		return kVK_F2;
	case OBS_KEY_F3:
		return kVK_F3;
	case OBS_KEY_F4:
		return kVK_F4;
	case OBS_KEY_F5:
		return kVK_F5;
	case OBS_KEY_F6:
		return kVK_F6;
	case OBS_KEY_F7:
		return kVK_F7;
	case OBS_KEY_F8:
		return kVK_F8;
	case OBS_KEY_F9:
		return kVK_F9;
	case OBS_KEY_F10:
		return kVK_F10;
	case OBS_KEY_F11:
		return kVK_F11;
	case OBS_KEY_F12:
		return kVK_F12;

	case OBS_KEY_HELP:
		return kVK_Help;
	case OBS_KEY_HOME:
		return kVK_Home;
	case OBS_KEY_PAGEUP:
		return kVK_PageUp;
	case OBS_KEY_DELETE:
		return kVK_ForwardDelete;
	case OBS_KEY_END:
		return kVK_End;
	case OBS_KEY_PAGEDOWN:
		return kVK_PageDown;

	case OBS_KEY_RIGHT:
		return kVK_RightArrow;
	case OBS_KEY_LEFT:
		return kVK_LeftArrow;
	case OBS_KEY_DOWN:
		return kVK_DownArrow;
	case OBS_KEY_UP:
		return kVK_UpArrow;

	case OBS_KEY_CLEAR:
		return kVK_ANSI_KeypadClear;
	case OBS_KEY_NUMSLASH:
		return kVK_ANSI_KeypadDivide;
	case OBS_KEY_NUMASTERISK:
		return kVK_ANSI_KeypadMultiply;
	case OBS_KEY_NUMMINUS:
		return kVK_ANSI_KeypadMinus;
	case OBS_KEY_NUMPLUS:
		return kVK_ANSI_KeypadPlus;
	case OBS_KEY_ENTER:
		return kVK_ANSI_KeypadEnter;

	case OBS_KEY_NUM1:
		return kVK_ANSI_Keypad1;
	case OBS_KEY_NUM2:
		return kVK_ANSI_Keypad2;
	case OBS_KEY_NUM3:
		return kVK_ANSI_Keypad3;
	case OBS_KEY_NUM4:
		return kVK_ANSI_Keypad4;
	case OBS_KEY_NUM5:
		return kVK_ANSI_Keypad5;
	case OBS_KEY_NUM6:
		return kVK_ANSI_Keypad6;
	case OBS_KEY_NUM7:
		return kVK_ANSI_Keypad7;
	case OBS_KEY_NUM8:
		return kVK_ANSI_Keypad8;
	case OBS_KEY_NUM9:
		return kVK_ANSI_Keypad9;
	case OBS_KEY_NUM0:
		return kVK_ANSI_Keypad0;

	case OBS_KEY_NUMPERIOD:
		return kVK_ANSI_KeypadDecimal;
	case OBS_KEY_NUMEQUAL:
		return kVK_ANSI_KeypadEquals;

	case OBS_KEY_F13:
		return kVK_F13;
	case OBS_KEY_F14:
		return kVK_F14;
	case OBS_KEY_F15:
		return kVK_F15;
	case OBS_KEY_F16:
		return kVK_F16;
	case OBS_KEY_F17:
		return kVK_F17;
	case OBS_KEY_F18:
		return kVK_F18;
	case OBS_KEY_F19:
		return kVK_F19;
	case OBS_KEY_F20:
		return kVK_F20;

	case OBS_KEY_CONTROL:
		return kVK_Control;
	case OBS_KEY_SHIFT:
		return kVK_Shift;
	case OBS_KEY_ALT:
		return kVK_Option;
	case OBS_KEY_META:
		return kVK_Command;
		//case OBS_KEY_CONTROL: return kVK_RightControl;
		//case OBS_KEY_SHIFT: return kVK_RightShift;
		//case OBS_KEY_ALT: return kVK_RightOption;
		//case OBS_KEY_META: return 0x36;

	case OBS_KEY_NONE:
	case OBS_KEY_LAST_VALUE:
	default:
		break;
	}
	return INVALID_KEY;
}

static bool localized_key_to_str(obs_key_t key, struct dstr *str)
{
#define MAP_KEY(k, s)                                             \
	case k:                                                   \
		dstr_copy(str, obs_get_hotkey_translation(k, s)); \
		return true

#define MAP_BUTTON(i)                                                         \
	case OBS_KEY_MOUSE##i:                                                \
		dstr_copy(str, obs_get_hotkey_translation(key, "Mouse " #i)); \
		return true

	switch (key) {
		MAP_KEY(OBS_KEY_SPACE, "Space");
		MAP_KEY(OBS_KEY_NUMEQUAL, "= (Keypad)");
		MAP_KEY(OBS_KEY_NUMASTERISK, "* (Keypad)");
		MAP_KEY(OBS_KEY_NUMPLUS, "+ (Keypad)");
		MAP_KEY(OBS_KEY_NUMMINUS, "- (Keypad)");
		MAP_KEY(OBS_KEY_NUMPERIOD, ". (Keypad)");
		MAP_KEY(OBS_KEY_NUMSLASH, "/ (Keypad)");
		MAP_KEY(OBS_KEY_NUM0, "0 (Keypad)");
		MAP_KEY(OBS_KEY_NUM1, "1 (Keypad)");
		MAP_KEY(OBS_KEY_NUM2, "2 (Keypad)");
		MAP_KEY(OBS_KEY_NUM3, "3 (Keypad)");
		MAP_KEY(OBS_KEY_NUM4, "4 (Keypad)");
		MAP_KEY(OBS_KEY_NUM5, "5 (Keypad)");
		MAP_KEY(OBS_KEY_NUM6, "6 (Keypad)");
		MAP_KEY(OBS_KEY_NUM7, "7 (Keypad)");
		MAP_KEY(OBS_KEY_NUM8, "8 (Keypad)");
		MAP_KEY(OBS_KEY_NUM9, "9 (Keypad)");

		MAP_BUTTON(1);
		MAP_BUTTON(2);
		MAP_BUTTON(3);
		MAP_BUTTON(4);
		MAP_BUTTON(5);
		MAP_BUTTON(6);
		MAP_BUTTON(7);
		MAP_BUTTON(8);
		MAP_BUTTON(9);
		MAP_BUTTON(10);
		MAP_BUTTON(11);
		MAP_BUTTON(12);
		MAP_BUTTON(13);
		MAP_BUTTON(14);
		MAP_BUTTON(15);
		MAP_BUTTON(16);
		MAP_BUTTON(17);
		MAP_BUTTON(18);
		MAP_BUTTON(19);
		MAP_BUTTON(20);
		MAP_BUTTON(21);
		MAP_BUTTON(22);
		MAP_BUTTON(23);
		MAP_BUTTON(24);
		MAP_BUTTON(25);
		MAP_BUTTON(26);
		MAP_BUTTON(27);
		MAP_BUTTON(28);
		MAP_BUTTON(29);
	default:
		break;
	}
#undef MAP_BUTTON
#undef MAP_KEY

	return false;
}

static bool code_to_str(int code, struct dstr *str)
{
#define MAP_GLYPH(c, g)                                \
	case c:                                        \
		dstr_from_wcs(str, (wchar_t[]){g, 0}); \
		return true
#define MAP_STR(c, s)              \
	case c:                    \
		dstr_copy(str, s); \
		return true
	switch (code) {
		MAP_GLYPH(kVK_Return, 0x21A9);
		MAP_GLYPH(kVK_Escape, 0x238B);
		MAP_GLYPH(kVK_Delete, 0x232B);
		MAP_GLYPH(kVK_Tab, 0x21e5);
		MAP_GLYPH(kVK_CapsLock, 0x21EA);
		MAP_GLYPH(kVK_ANSI_KeypadClear, 0x2327);
		MAP_GLYPH(kVK_ANSI_KeypadEnter, 0x2305);
		MAP_GLYPH(kVK_Help, 0x003F);
		MAP_GLYPH(kVK_Home, 0x2196);
		MAP_GLYPH(kVK_PageUp, 0x21de);
		MAP_GLYPH(kVK_ForwardDelete, 0x2326);
		MAP_GLYPH(kVK_End, 0x2198);
		MAP_GLYPH(kVK_PageDown, 0x21df);

		MAP_GLYPH(kVK_RightArrow, 0x2192);
		MAP_GLYPH(kVK_LeftArrow, 0x2190);
		MAP_GLYPH(kVK_DownArrow, 0x2193);
		MAP_GLYPH(kVK_UpArrow, 0x2191);

		MAP_STR(kVK_F1, "F1");
		MAP_STR(kVK_F2, "F2");
		MAP_STR(kVK_F3, "F3");
		MAP_STR(kVK_F4, "F4");
		MAP_STR(kVK_F5, "F5");
		MAP_STR(kVK_F6, "F6");
		MAP_STR(kVK_F7, "F7");
		MAP_STR(kVK_F8, "F8");
		MAP_STR(kVK_F9, "F9");
		MAP_STR(kVK_F10, "F10");
		MAP_STR(kVK_F11, "F11");
		MAP_STR(kVK_F12, "F12");
		MAP_STR(kVK_F13, "F13");
		MAP_STR(kVK_F14, "F14");
		MAP_STR(kVK_F15, "F15");
		MAP_STR(kVK_F16, "F16");
		MAP_STR(kVK_F17, "F17");
		MAP_STR(kVK_F18, "F18");
		MAP_STR(kVK_F19, "F19");
		MAP_STR(kVK_F20, "F20");
		MAP_GLYPH(kVK_Control, kControlUnicode);
		MAP_GLYPH(kVK_Shift, kShiftUnicode);
		MAP_GLYPH(kVK_Option, kOptionUnicode);
		MAP_GLYPH(kVK_Command, kCommandUnicode);
		MAP_GLYPH(kVK_RightControl, kControlUnicode);
		MAP_GLYPH(kVK_RightShift, kShiftUnicode);
		MAP_GLYPH(kVK_RightOption, kOptionUnicode);
	}
#undef MAP_STR
#undef MAP_GLYPH

	return false;
}

void obs_key_to_str(obs_key_t key, struct dstr *str)
{
	if (localized_key_to_str(key, str))
		return;

	int code = obs_key_to_virtual_key(key);
	if (code_to_str(code, str))
		return;

	if (code == INVALID_KEY) {
		blog(LOG_ERROR,
		     "hotkey-cocoa: Got invalid key while "
		     "translating key '%d' (%s)",
		     key, obs_key_to_name(key));
		goto err;
	}

	struct obs_hotkeys_platform *plat = NULL;

	if (obs) {
		pthread_mutex_lock(&obs->hotkeys.mutex);
		plat = obs->hotkeys.platform_context;
		hotkeys_retain(plat);
		pthread_mutex_unlock(&obs->hotkeys.mutex);
	}

	if (!plat) {
		blog(LOG_ERROR,
		     "hotkey-cocoa: Could not get hotkey platform "
		     "while translating key '%d' (%s)",
		     key, obs_key_to_name(key));
		goto err;
	}

	const UniCharCount max_length = 16;
	UInt32 dead_key_state = 0;
	UniChar buffer[max_length];
	UniCharCount len = 0;

	OSStatus err =
		UCKeyTranslate(plat->layout, code, kUCKeyActionDown,
			       0x104, //caps lock for upper case letters
			       LMGetKbdType(), kUCKeyTranslateNoDeadKeysBit,
			       &dead_key_state, max_length, &len, buffer);

	if (err == noErr && len <= 0 && dead_key_state) {
		err = UCKeyTranslate(plat->layout, kVK_Space, kUCKeyActionDown,
				     0x104, LMGetKbdType(),
				     kUCKeyTranslateNoDeadKeysBit,
				     &dead_key_state, max_length, &len, buffer);
	}

	hotkeys_release(plat);

	if (err != noErr) {
		blog(LOG_ERROR,
		     "hotkey-cocoa: Error while translating key '%d'"
		     " (0x%x, %s) to string: %d",
		     key, code, obs_key_to_name(key), err);
		goto err;
	}

	if (len == 0) {
		blog(LOG_ERROR,
		     "hotkey-cocoa: Got 0 length string while "
		     "translating '%d' (0x%x, %s) to string",
		     key, code, obs_key_to_name(key));
		goto err;
	}

	CFStringRef string = CFStringCreateWithCharactersNoCopy(
		NULL, buffer, len, kCFAllocatorNull);
	if (!string) {
		blog(LOG_ERROR,
		     "hotkey-cocoa: Could not create CFStringRef "
		     "while translating '%d' (0x%x, %s) to string",
		     key, code, obs_key_to_name(key));
		goto err;
	}

	if (!dstr_from_cfstring(str, string)) {
		blog(LOG_ERROR,
		     "hotkey-cocoa: Could not translate CFStringRef "
		     "to CString while translating '%d' (0x%x, %s)",
		     key, code, obs_key_to_name(key));

		goto release;
	}

	CFRelease(string);
	return;

release:
	CFRelease(string);
err:
	dstr_copy(str, obs_key_to_name(key));
}

#define OBS_COCOA_MODIFIER_SIZE 7
static void unichar_to_utf8(const UniChar *c, char *buff)
{
	CFStringRef string = CFStringCreateWithCharactersNoCopy(
		NULL, c, 2, kCFAllocatorNull);
	if (!string) {
		blog(LOG_ERROR, "hotkey-cocoa: Could not create CFStringRef "
				"while populating modifier strings");
		return;
	}

	if (!CFStringGetCString(string, buff, OBS_COCOA_MODIFIER_SIZE,
				kCFStringEncodingUTF8))
		blog(LOG_ERROR,
		     "hotkey-cocoa: Error while populating "
		     " modifier string with glyph %d (0x%x)",
		     c[0], c[0]);

	CFRelease(string);
}

static char ctrl_str[OBS_COCOA_MODIFIER_SIZE];
static char opt_str[OBS_COCOA_MODIFIER_SIZE];
static char shift_str[OBS_COCOA_MODIFIER_SIZE];
static char cmd_str[OBS_COCOA_MODIFIER_SIZE];
static void init_utf_8_strings(void)
{
	const UniChar ctrl_uni[] = {kControlUnicode, 0};
	const UniChar opt_uni[] = {kOptionUnicode, 0};
	const UniChar shift_uni[] = {kShiftUnicode, 0};
	const UniChar cmd_uni[] = {kCommandUnicode, 0};

	unichar_to_utf8(ctrl_uni, ctrl_str);
	unichar_to_utf8(opt_uni, opt_str);
	unichar_to_utf8(shift_uni, shift_str);
	unichar_to_utf8(cmd_uni, cmd_str);
}

static pthread_once_t strings_token = PTHREAD_ONCE_INIT;
void obs_key_combination_to_str(obs_key_combination_t key, struct dstr *str)
{
	struct dstr key_str = {0};
	if (key.key != OBS_KEY_NONE)
		obs_key_to_str(key.key, &key_str);

	int res = pthread_once(&strings_token, init_utf_8_strings);
	if (res) {
		blog(LOG_ERROR,
		     "hotkeys-cocoa: Error while translating "
		     "modifiers %d (0x%x)",
		     res, res);
		dstr_move(str, &key_str);
		return;
	}

#define CHECK_MODIFIER(mod, str) ((key.modifiers & mod) ? str : "")
	dstr_printf(str, "%s%s%s%s%s",
		    CHECK_MODIFIER(INTERACT_CONTROL_KEY, ctrl_str),
		    CHECK_MODIFIER(INTERACT_ALT_KEY, opt_str),
		    CHECK_MODIFIER(INTERACT_SHIFT_KEY, shift_str),
		    CHECK_MODIFIER(INTERACT_COMMAND_KEY, cmd_str),
		    key_str.len ? key_str.array : "");
#undef CHECK_MODIFIER

	dstr_free(&key_str);
}

static inline CFDictionaryRef copy_device_mask(UInt32 page, UInt32 usage)
{
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
		kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);

	CFNumberRef value;
	// Add the page value.
	value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
	CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsagePageKey), value);
	CFRelease(value);

	// Add the usage value (which is only valid if page value exists).
	value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
	CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsageKey), value);
	CFRelease(value);

	return dict;
}

static CFSetRef copy_devices(obs_hotkeys_platform_t *plat, UInt32 page,
			     UInt32 usage)
{
	CFDictionaryRef mask = copy_device_mask(page, usage);
	IOHIDManagerSetDeviceMatching(plat->manager, mask);
	CFRelease(mask);

	CFSetRef devices = IOHIDManagerCopyDevices(plat->manager);
	if (!devices)
		return NULL;

	if (CFSetGetCount(devices) < 1) {
		CFRelease(devices);
		return NULL;
	}

	return devices;
}

static UInt16 usage_to_carbon(UInt32 usage)
{
	switch (usage) {
	case kHIDUsage_KeyboardErrorRollOver:
		return INVALID_KEY;
	case kHIDUsage_KeyboardPOSTFail:
		return INVALID_KEY;
	case kHIDUsage_KeyboardErrorUndefined:
		return INVALID_KEY;

	case kHIDUsage_KeyboardA:
		return kVK_ANSI_A;
	case kHIDUsage_KeyboardB:
		return kVK_ANSI_B;
	case kHIDUsage_KeyboardC:
		return kVK_ANSI_C;
	case kHIDUsage_KeyboardD:
		return kVK_ANSI_D;
	case kHIDUsage_KeyboardE:
		return kVK_ANSI_E;
	case kHIDUsage_KeyboardF:
		return kVK_ANSI_F;
	case kHIDUsage_KeyboardG:
		return kVK_ANSI_G;
	case kHIDUsage_KeyboardH:
		return kVK_ANSI_H;
	case kHIDUsage_KeyboardI:
		return kVK_ANSI_I;
	case kHIDUsage_KeyboardJ:
		return kVK_ANSI_J;
	case kHIDUsage_KeyboardK:
		return kVK_ANSI_K;
	case kHIDUsage_KeyboardL:
		return kVK_ANSI_L;
	case kHIDUsage_KeyboardM:
		return kVK_ANSI_M;
	case kHIDUsage_KeyboardN:
		return kVK_ANSI_N;
	case kHIDUsage_KeyboardO:
		return kVK_ANSI_O;
	case kHIDUsage_KeyboardP:
		return kVK_ANSI_P;
	case kHIDUsage_KeyboardQ:
		return kVK_ANSI_Q;
	case kHIDUsage_KeyboardR:
		return kVK_ANSI_R;
	case kHIDUsage_KeyboardS:
		return kVK_ANSI_S;
	case kHIDUsage_KeyboardT:
		return kVK_ANSI_T;
	case kHIDUsage_KeyboardU:
		return kVK_ANSI_U;
	case kHIDUsage_KeyboardV:
		return kVK_ANSI_V;
	case kHIDUsage_KeyboardW:
		return kVK_ANSI_W;
	case kHIDUsage_KeyboardX:
		return kVK_ANSI_X;
	case kHIDUsage_KeyboardY:
		return kVK_ANSI_Y;
	case kHIDUsage_KeyboardZ:
		return kVK_ANSI_Z;

	case kHIDUsage_Keyboard1:
		return kVK_ANSI_1;
	case kHIDUsage_Keyboard2:
		return kVK_ANSI_2;
	case kHIDUsage_Keyboard3:
		return kVK_ANSI_3;
	case kHIDUsage_Keyboard4:
		return kVK_ANSI_4;
	case kHIDUsage_Keyboard5:
		return kVK_ANSI_5;
	case kHIDUsage_Keyboard6:
		return kVK_ANSI_6;
	case kHIDUsage_Keyboard7:
		return kVK_ANSI_7;
	case kHIDUsage_Keyboard8:
		return kVK_ANSI_8;
	case kHIDUsage_Keyboard9:
		return kVK_ANSI_9;
	case kHIDUsage_Keyboard0:
		return kVK_ANSI_0;

	case kHIDUsage_KeyboardReturnOrEnter:
		return kVK_Return;
	case kHIDUsage_KeyboardEscape:
		return kVK_Escape;
	case kHIDUsage_KeyboardDeleteOrBackspace:
		return kVK_Delete;
	case kHIDUsage_KeyboardTab:
		return kVK_Tab;
	case kHIDUsage_KeyboardSpacebar:
		return kVK_Space;
	case kHIDUsage_KeyboardHyphen:
		return kVK_ANSI_Minus;
	case kHIDUsage_KeyboardEqualSign:
		return kVK_ANSI_Equal;
	case kHIDUsage_KeyboardOpenBracket:
		return kVK_ANSI_LeftBracket;
	case kHIDUsage_KeyboardCloseBracket:
		return kVK_ANSI_RightBracket;
	case kHIDUsage_KeyboardBackslash:
		return kVK_ANSI_Backslash;
	case kHIDUsage_KeyboardNonUSPound:
		return INVALID_KEY;
	case kHIDUsage_KeyboardSemicolon:
		return kVK_ANSI_Semicolon;
	case kHIDUsage_KeyboardQuote:
		return kVK_ANSI_Quote;
	case kHIDUsage_KeyboardGraveAccentAndTilde:
		return kVK_ANSI_Grave;
	case kHIDUsage_KeyboardComma:
		return kVK_ANSI_Comma;
	case kHIDUsage_KeyboardPeriod:
		return kVK_ANSI_Period;
	case kHIDUsage_KeyboardSlash:
		return kVK_ANSI_Slash;
	case kHIDUsage_KeyboardCapsLock:
		return kVK_CapsLock;

	case kHIDUsage_KeyboardF1:
		return kVK_F1;
	case kHIDUsage_KeyboardF2:
		return kVK_F2;
	case kHIDUsage_KeyboardF3:
		return kVK_F3;
	case kHIDUsage_KeyboardF4:
		return kVK_F4;
	case kHIDUsage_KeyboardF5:
		return kVK_F5;
	case kHIDUsage_KeyboardF6:
		return kVK_F6;
	case kHIDUsage_KeyboardF7:
		return kVK_F7;
	case kHIDUsage_KeyboardF8:
		return kVK_F8;
	case kHIDUsage_KeyboardF9:
		return kVK_F9;
	case kHIDUsage_KeyboardF10:
		return kVK_F10;
	case kHIDUsage_KeyboardF11:
		return kVK_F11;
	case kHIDUsage_KeyboardF12:
		return kVK_F12;

	case kHIDUsage_KeyboardPrintScreen:
		return INVALID_KEY;
	case kHIDUsage_KeyboardScrollLock:
		return INVALID_KEY;
	case kHIDUsage_KeyboardPause:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInsert:
		return kVK_Help;
	case kHIDUsage_KeyboardHome:
		return kVK_Home;
	case kHIDUsage_KeyboardPageUp:
		return kVK_PageUp;
	case kHIDUsage_KeyboardDeleteForward:
		return kVK_ForwardDelete;
	case kHIDUsage_KeyboardEnd:
		return kVK_End;
	case kHIDUsage_KeyboardPageDown:
		return kVK_PageDown;

	case kHIDUsage_KeyboardRightArrow:
		return kVK_RightArrow;
	case kHIDUsage_KeyboardLeftArrow:
		return kVK_LeftArrow;
	case kHIDUsage_KeyboardDownArrow:
		return kVK_DownArrow;
	case kHIDUsage_KeyboardUpArrow:
		return kVK_UpArrow;

	case kHIDUsage_KeypadNumLock:
		return kVK_ANSI_KeypadClear;
	case kHIDUsage_KeypadSlash:
		return kVK_ANSI_KeypadDivide;
	case kHIDUsage_KeypadAsterisk:
		return kVK_ANSI_KeypadMultiply;
	case kHIDUsage_KeypadHyphen:
		return kVK_ANSI_KeypadMinus;
	case kHIDUsage_KeypadPlus:
		return kVK_ANSI_KeypadPlus;
	case kHIDUsage_KeypadEnter:
		return kVK_ANSI_KeypadEnter;

	case kHIDUsage_Keypad1:
		return kVK_ANSI_Keypad1;
	case kHIDUsage_Keypad2:
		return kVK_ANSI_Keypad2;
	case kHIDUsage_Keypad3:
		return kVK_ANSI_Keypad3;
	case kHIDUsage_Keypad4:
		return kVK_ANSI_Keypad4;
	case kHIDUsage_Keypad5:
		return kVK_ANSI_Keypad5;
	case kHIDUsage_Keypad6:
		return kVK_ANSI_Keypad6;
	case kHIDUsage_Keypad7:
		return kVK_ANSI_Keypad7;
	case kHIDUsage_Keypad8:
		return kVK_ANSI_Keypad8;
	case kHIDUsage_Keypad9:
		return kVK_ANSI_Keypad9;
	case kHIDUsage_Keypad0:
		return kVK_ANSI_Keypad0;

	case kHIDUsage_KeypadPeriod:
		return kVK_ANSI_KeypadDecimal;
	case kHIDUsage_KeyboardNonUSBackslash:
		return INVALID_KEY;
	case kHIDUsage_KeyboardApplication:
		return kVK_F13;
	case kHIDUsage_KeyboardPower:
		return INVALID_KEY;
	case kHIDUsage_KeypadEqualSign:
		return kVK_ANSI_KeypadEquals;

	case kHIDUsage_KeyboardF13:
		return kVK_F13;
	case kHIDUsage_KeyboardF14:
		return kVK_F14;
	case kHIDUsage_KeyboardF15:
		return kVK_F15;
	case kHIDUsage_KeyboardF16:
		return kVK_F16;
	case kHIDUsage_KeyboardF17:
		return kVK_F17;
	case kHIDUsage_KeyboardF18:
		return kVK_F18;
	case kHIDUsage_KeyboardF19:
		return kVK_F19;
	case kHIDUsage_KeyboardF20:
		return kVK_F20;
	case kHIDUsage_KeyboardF21:
		return INVALID_KEY;
	case kHIDUsage_KeyboardF22:
		return INVALID_KEY;
	case kHIDUsage_KeyboardF23:
		return INVALID_KEY;
	case kHIDUsage_KeyboardF24:
		return INVALID_KEY;

	case kHIDUsage_KeyboardExecute:
		return INVALID_KEY;
	case kHIDUsage_KeyboardHelp:
		return INVALID_KEY;
	case kHIDUsage_KeyboardMenu:
		return 0x7F;
	case kHIDUsage_KeyboardSelect:
		return kVK_ANSI_KeypadEnter;
	case kHIDUsage_KeyboardStop:
		return INVALID_KEY;
	case kHIDUsage_KeyboardAgain:
		return INVALID_KEY;
	case kHIDUsage_KeyboardUndo:
		return INVALID_KEY;
	case kHIDUsage_KeyboardCut:
		return INVALID_KEY;
	case kHIDUsage_KeyboardCopy:
		return INVALID_KEY;
	case kHIDUsage_KeyboardPaste:
		return INVALID_KEY;
	case kHIDUsage_KeyboardFind:
		return INVALID_KEY;

	case kHIDUsage_KeyboardMute:
		return kVK_Mute;
	case kHIDUsage_KeyboardVolumeUp:
		return kVK_VolumeUp;
	case kHIDUsage_KeyboardVolumeDown:
		return kVK_VolumeDown;

	case kHIDUsage_KeyboardLockingCapsLock:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLockingNumLock:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLockingScrollLock:
		return INVALID_KEY;

	case kHIDUsage_KeypadComma:
		return INVALID_KEY;
	case kHIDUsage_KeypadEqualSignAS400:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational1:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational2:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational3:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational4:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational5:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational6:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational7:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational8:
		return INVALID_KEY;
	case kHIDUsage_KeyboardInternational9:
		return INVALID_KEY;

	case kHIDUsage_KeyboardLANG1:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLANG2:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLANG3:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLANG4:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLANG5:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLANG6:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLANG7:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLANG8:
		return INVALID_KEY;
	case kHIDUsage_KeyboardLANG9:
		return INVALID_KEY;

	case kHIDUsage_KeyboardAlternateErase:
		return INVALID_KEY;
	case kHIDUsage_KeyboardSysReqOrAttention:
		return INVALID_KEY;
	case kHIDUsage_KeyboardCancel:
		return INVALID_KEY;
	case kHIDUsage_KeyboardClear:
		return INVALID_KEY;
	case kHIDUsage_KeyboardPrior:
		return INVALID_KEY;
	case kHIDUsage_KeyboardReturn:
		return INVALID_KEY;
	case kHIDUsage_KeyboardSeparator:
		return INVALID_KEY;
	case kHIDUsage_KeyboardOut:
		return INVALID_KEY;
	case kHIDUsage_KeyboardOper:
		return INVALID_KEY;
	case kHIDUsage_KeyboardClearOrAgain:
		return INVALID_KEY;
	case kHIDUsage_KeyboardCrSelOrProps:
		return INVALID_KEY;
	case kHIDUsage_KeyboardExSel:
		return INVALID_KEY;

		/* 0xa5-0xdf Reserved */

	case kHIDUsage_KeyboardLeftControl:
		return kVK_Control;
	case kHIDUsage_KeyboardLeftShift:
		return kVK_Shift;
	case kHIDUsage_KeyboardLeftAlt:
		return kVK_Option;
	case kHIDUsage_KeyboardLeftGUI:
		return kVK_Command;
	case kHIDUsage_KeyboardRightControl:
		return kVK_RightControl;
	case kHIDUsage_KeyboardRightShift:
		return kVK_RightShift;
	case kHIDUsage_KeyboardRightAlt:
		return kVK_RightOption;
	case kHIDUsage_KeyboardRightGUI:
		return 0x36; //??

		/* 0xe8-0xffff Reserved */

	case kHIDUsage_Keyboard_Reserved:
		return INVALID_KEY;
	default:
		return INVALID_KEY;
	}
	return INVALID_KEY;
}

obs_key_t obs_key_from_virtual_key(int code)
{
	switch (code) {
	case kVK_ANSI_A:
		return OBS_KEY_A;
	case kVK_ANSI_B:
		return OBS_KEY_B;
	case kVK_ANSI_C:
		return OBS_KEY_C;
	case kVK_ANSI_D:
		return OBS_KEY_D;
	case kVK_ANSI_E:
		return OBS_KEY_E;
	case kVK_ANSI_F:
		return OBS_KEY_F;
	case kVK_ANSI_G:
		return OBS_KEY_G;
	case kVK_ANSI_H:
		return OBS_KEY_H;
	case kVK_ANSI_I:
		return OBS_KEY_I;
	case kVK_ANSI_J:
		return OBS_KEY_J;
	case kVK_ANSI_K:
		return OBS_KEY_K;
	case kVK_ANSI_L:
		return OBS_KEY_L;
	case kVK_ANSI_M:
		return OBS_KEY_M;
	case kVK_ANSI_N:
		return OBS_KEY_N;
	case kVK_ANSI_O:
		return OBS_KEY_O;
	case kVK_ANSI_P:
		return OBS_KEY_P;
	case kVK_ANSI_Q:
		return OBS_KEY_Q;
	case kVK_ANSI_R:
		return OBS_KEY_R;
	case kVK_ANSI_S:
		return OBS_KEY_S;
	case kVK_ANSI_T:
		return OBS_KEY_T;
	case kVK_ANSI_U:
		return OBS_KEY_U;
	case kVK_ANSI_V:
		return OBS_KEY_V;
	case kVK_ANSI_W:
		return OBS_KEY_W;
	case kVK_ANSI_X:
		return OBS_KEY_X;
	case kVK_ANSI_Y:
		return OBS_KEY_Y;
	case kVK_ANSI_Z:
		return OBS_KEY_Z;

	case kVK_ANSI_1:
		return OBS_KEY_1;
	case kVK_ANSI_2:
		return OBS_KEY_2;
	case kVK_ANSI_3:
		return OBS_KEY_3;
	case kVK_ANSI_4:
		return OBS_KEY_4;
	case kVK_ANSI_5:
		return OBS_KEY_5;
	case kVK_ANSI_6:
		return OBS_KEY_6;
	case kVK_ANSI_7:
		return OBS_KEY_7;
	case kVK_ANSI_8:
		return OBS_KEY_8;
	case kVK_ANSI_9:
		return OBS_KEY_9;
	case kVK_ANSI_0:
		return OBS_KEY_0;

	case kVK_Return:
		return OBS_KEY_RETURN;
	case kVK_Escape:
		return OBS_KEY_ESCAPE;
	case kVK_Delete:
		return OBS_KEY_BACKSPACE;
	case kVK_Tab:
		return OBS_KEY_TAB;
	case kVK_Space:
		return OBS_KEY_SPACE;
	case kVK_ANSI_Minus:
		return OBS_KEY_MINUS;
	case kVK_ANSI_Equal:
		return OBS_KEY_EQUAL;
	case kVK_ANSI_LeftBracket:
		return OBS_KEY_BRACKETLEFT;
	case kVK_ANSI_RightBracket:
		return OBS_KEY_BRACKETRIGHT;
	case kVK_ANSI_Backslash:
		return OBS_KEY_BACKSLASH;
	case kVK_ANSI_Semicolon:
		return OBS_KEY_SEMICOLON;
	case kVK_ANSI_Quote:
		return OBS_KEY_QUOTE;
	case kVK_ANSI_Grave:
		return OBS_KEY_DEAD_GRAVE;
	case kVK_ANSI_Comma:
		return OBS_KEY_COMMA;
	case kVK_ANSI_Period:
		return OBS_KEY_PERIOD;
	case kVK_ANSI_Slash:
		return OBS_KEY_SLASH;
	case kVK_CapsLock:
		return OBS_KEY_CAPSLOCK;
	case kVK_ISO_Section:
		return OBS_KEY_SECTION;

	case kVK_F1:
		return OBS_KEY_F1;
	case kVK_F2:
		return OBS_KEY_F2;
	case kVK_F3:
		return OBS_KEY_F3;
	case kVK_F4:
		return OBS_KEY_F4;
	case kVK_F5:
		return OBS_KEY_F5;
	case kVK_F6:
		return OBS_KEY_F6;
	case kVK_F7:
		return OBS_KEY_F7;
	case kVK_F8:
		return OBS_KEY_F8;
	case kVK_F9:
		return OBS_KEY_F9;
	case kVK_F10:
		return OBS_KEY_F10;
	case kVK_F11:
		return OBS_KEY_F11;
	case kVK_F12:
		return OBS_KEY_F12;

	case kVK_Help:
		return OBS_KEY_HELP;
	case kVK_Home:
		return OBS_KEY_HOME;
	case kVK_PageUp:
		return OBS_KEY_PAGEUP;
	case kVK_ForwardDelete:
		return OBS_KEY_DELETE;
	case kVK_End:
		return OBS_KEY_END;
	case kVK_PageDown:
		return OBS_KEY_PAGEDOWN;

	case kVK_RightArrow:
		return OBS_KEY_RIGHT;
	case kVK_LeftArrow:
		return OBS_KEY_LEFT;
	case kVK_DownArrow:
		return OBS_KEY_DOWN;
	case kVK_UpArrow:
		return OBS_KEY_UP;

	case kVK_ANSI_KeypadClear:
		return OBS_KEY_CLEAR;
	case kVK_ANSI_KeypadDivide:
		return OBS_KEY_NUMSLASH;
	case kVK_ANSI_KeypadMultiply:
		return OBS_KEY_NUMASTERISK;
	case kVK_ANSI_KeypadMinus:
		return OBS_KEY_NUMMINUS;
	case kVK_ANSI_KeypadPlus:
		return OBS_KEY_NUMPLUS;
	case kVK_ANSI_KeypadEnter:
		return OBS_KEY_ENTER;

	case kVK_ANSI_Keypad1:
		return OBS_KEY_NUM1;
	case kVK_ANSI_Keypad2:
		return OBS_KEY_NUM2;
	case kVK_ANSI_Keypad3:
		return OBS_KEY_NUM3;
	case kVK_ANSI_Keypad4:
		return OBS_KEY_NUM4;
	case kVK_ANSI_Keypad5:
		return OBS_KEY_NUM5;
	case kVK_ANSI_Keypad6:
		return OBS_KEY_NUM6;
	case kVK_ANSI_Keypad7:
		return OBS_KEY_NUM7;
	case kVK_ANSI_Keypad8:
		return OBS_KEY_NUM8;
	case kVK_ANSI_Keypad9:
		return OBS_KEY_NUM9;
	case kVK_ANSI_Keypad0:
		return OBS_KEY_NUM0;

	case kVK_ANSI_KeypadDecimal:
		return OBS_KEY_NUMPERIOD;
	case kVK_ANSI_KeypadEquals:
		return OBS_KEY_NUMEQUAL;

	case kVK_F13:
		return OBS_KEY_F13;
	case kVK_F14:
		return OBS_KEY_F14;
	case kVK_F15:
		return OBS_KEY_F15;
	case kVK_F16:
		return OBS_KEY_F16;
	case kVK_F17:
		return OBS_KEY_F17;
	case kVK_F18:
		return OBS_KEY_F18;
	case kVK_F19:
		return OBS_KEY_F19;
	case kVK_F20:
		return OBS_KEY_F20;

	case kVK_Control:
		return OBS_KEY_CONTROL;
	case kVK_Shift:
		return OBS_KEY_SHIFT;
	case kVK_Option:
		return OBS_KEY_ALT;
	case kVK_Command:
		return OBS_KEY_META;
	case kVK_RightControl:
		return OBS_KEY_CONTROL;
	case kVK_RightShift:
		return OBS_KEY_SHIFT;
	case kVK_RightOption:
		return OBS_KEY_ALT;
	case 0x36:
		return OBS_KEY_META;

	case kVK_Function:
	case kVK_Mute:
	case kVK_VolumeDown:
	case kVK_VolumeUp:
		break;
	}
	return OBS_KEY_NONE;
}

static inline void load_key(obs_hotkeys_platform_t *plat, IOHIDElementRef key)
{
	UInt32 usage_code = IOHIDElementGetUsage(key);
	UInt16 carbon_code = usage_to_carbon(usage_code);

	if (carbon_code == INVALID_KEY)
		return;

	obs_key_t obs_key = obs_key_from_virtual_key(carbon_code);
	if (obs_key == OBS_KEY_NONE)
		return;

	da_push_back(plat->keys[obs_key], &key);
	CFRetain(*(IOHIDElementRef *)da_end(plat->keys[obs_key]));
}

static inline void load_keyboard(obs_hotkeys_platform_t *plat,
				 IOHIDDeviceRef keyboard)
{
	CFArrayRef keys = IOHIDDeviceCopyMatchingElements(
		keyboard, NULL, kIOHIDOptionsTypeNone);

	if (!keys) {
		blog(LOG_ERROR, "hotkeys-cocoa: Getting keyboard keys failed");
		return;
	}

	CFIndex count = CFArrayGetCount(keys);
	if (!count) {
		blog(LOG_ERROR, "hotkeys-cocoa: Keyboard has no keys");
		CFRelease(keys);
		return;
	}

	for (CFIndex i = 0; i < count; i++) {
		IOHIDElementRef key =
			(IOHIDElementRef)CFArrayGetValueAtIndex(keys, i);

		// Skip non-matching keys elements
		if (IOHIDElementGetUsagePage(key) != kHIDPage_KeyboardOrKeypad)
			continue;

		load_key(plat, key);
	}

	CFRelease(keys);
}

static bool init_keyboard(obs_hotkeys_platform_t *plat)
{
	CFSetRef keyboards = copy_devices(plat, kHIDPage_GenericDesktop,
					  kHIDUsage_GD_Keyboard);
	if (!keyboards)
		return false;

	CFIndex count = CFSetGetCount(keyboards);

	CFTypeRef devices[count];
	CFSetGetValues(keyboards, devices);

	for (CFIndex i = 0; i < count; i++)
		load_keyboard(plat, (IOHIDDeviceRef)devices[i]);

	CFRelease(keyboards);
	return true;
}

static inline void free_hotkeys_platform(obs_hotkeys_platform_t *plat)
{
	if (!plat)
		return;

	if (plat->tis) {
		CFRelease(plat->tis);
		plat->tis = NULL;
	}

	if (plat->layout_data) {
		CFRelease(plat->layout_data);
		plat->layout_data = NULL;
	}

	if (plat->manager) {
		CFRelease(plat->manager);
		plat->manager = NULL;
	}

	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++) {
		for (size_t j = 0; j < plat->keys[i].num; j++)
			CFRelease(plat->keys[i].array[j]);

		da_free(plat->keys[i]);
	}

	bfree(plat);
}

static bool log_layout_name(TISInputSourceRef tis)
{
	struct dstr layout_name = {0};
	CFStringRef sid = (CFStringRef)TISGetInputSourceProperty(
		tis, kTISPropertyInputSourceID);
	if (!sid) {
		blog(LOG_ERROR, "hotkeys-cocoa: Failed getting InputSourceID");
		return false;
	}

	if (!dstr_from_cfstring(&layout_name, sid)) {
		blog(LOG_ERROR, "hotkeys-cocoa: Could not convert InputSourceID"
				" to CString");
		goto fail;
	}

	blog(LOG_INFO, "hotkeys-cocoa: Using layout '%s'", layout_name.array);

	dstr_free(&layout_name);
	return true;

fail:
	dstr_free(&layout_name);
	return false;
}

static bool init_hotkeys_platform(obs_hotkeys_platform_t **plat_)
{
	if (!plat_)
		return false;

	*plat_ = bzalloc(sizeof(obs_hotkeys_platform_t));
	obs_hotkeys_platform_t *plat = *plat_;
	if (!plat) {
		*plat_ = NULL;
		return false;
	}

	plat->tis = TISCopyCurrentKeyboardLayoutInputSource();
	plat->layout_data = (CFDataRef)TISGetInputSourceProperty(
		plat->tis, kTISPropertyUnicodeKeyLayoutData);

	if (!plat->layout_data) {
		blog(LOG_ERROR, "hotkeys-cocoa: Failed getting LayoutData");
		goto fail;
	}

	CFRetain(plat->layout_data);
	plat->layout = (UCKeyboardLayout *)CFDataGetBytePtr(plat->layout_data);

	plat->manager =
		IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

	IOReturn openStatus =
		IOHIDManagerOpen(plat->manager, kIOHIDOptionsTypeNone);
	if (openStatus != kIOReturnSuccess) {
		blog(LOG_ERROR, "hotkeys-cocoa: Failed opening HIDManager");
		goto fail;
	}

	init_keyboard(plat);

	return true;

fail:
	hotkeys_release(plat);
	*plat_ = NULL;
	return false;
}

static void input_method_changed(CFNotificationCenterRef nc, void *observer,
				 CFStringRef name, const void *object,
				 CFDictionaryRef user_info)
{
	UNUSED_PARAMETER(nc);
	UNUSED_PARAMETER(name);
	UNUSED_PARAMETER(object);
	UNUSED_PARAMETER(user_info);

	struct obs_core_hotkeys *hotkeys = observer;
	obs_hotkeys_platform_t *new_plat;

	if (init_hotkeys_platform(&new_plat)) {
		obs_hotkeys_platform_t *plat;

		pthread_mutex_lock(&hotkeys->mutex);
		plat = hotkeys->platform_context;

		if (new_plat && plat &&
		    new_plat->layout_data == plat->layout_data) {
			pthread_mutex_unlock(&hotkeys->mutex);
			hotkeys_release(new_plat);
			return;
		}

		hotkeys->platform_context = new_plat;
		if (new_plat)
			log_layout_name(new_plat->tis);
		pthread_mutex_unlock(&hotkeys->mutex);

		calldata_t params = {0};
		signal_handler_signal(hotkeys->signals, "hotkey_layout_change",
				      &params);
		if (plat)
			hotkeys_release(plat);
	}
}

bool obs_hotkeys_platform_init(struct obs_core_hotkeys *hotkeys)
{
	CFNotificationCenterAddObserver(
		CFNotificationCenterGetDistributedCenter(), hotkeys,
		input_method_changed,
		kTISNotifySelectedKeyboardInputSourceChanged, NULL,
		CFNotificationSuspensionBehaviorDeliverImmediately);

	input_method_changed(NULL, hotkeys, NULL, NULL, NULL);
	return hotkeys->platform_context != NULL;
}

void obs_hotkeys_platform_free(struct obs_core_hotkeys *hotkeys)
{
	CFNotificationCenterRemoveEveryObserver(
		CFNotificationCenterGetDistributedCenter(), hotkeys);

	hotkeys_release(hotkeys->platform_context);
}

typedef unsigned long NSUInteger;
static bool mouse_button_pressed(obs_key_t key, bool *pressed)
{
	int button = 0;
	switch (key) {
#define MAP_BUTTON(n)           \
	case OBS_KEY_MOUSE##n:  \
		button = n - 1; \
		break
		MAP_BUTTON(1);
		MAP_BUTTON(2);
		MAP_BUTTON(3);
		MAP_BUTTON(4);
		MAP_BUTTON(5);
		MAP_BUTTON(6);
		MAP_BUTTON(7);
		MAP_BUTTON(8);
		MAP_BUTTON(9);
		MAP_BUTTON(10);
		MAP_BUTTON(11);
		MAP_BUTTON(12);
		MAP_BUTTON(13);
		MAP_BUTTON(14);
		MAP_BUTTON(15);
		MAP_BUTTON(16);
		MAP_BUTTON(17);
		MAP_BUTTON(18);
		MAP_BUTTON(19);
		MAP_BUTTON(20);
		MAP_BUTTON(21);
		MAP_BUTTON(22);
		MAP_BUTTON(23);
		MAP_BUTTON(24);
		MAP_BUTTON(25);
		MAP_BUTTON(26);
		MAP_BUTTON(27);
		MAP_BUTTON(28);
		MAP_BUTTON(29);
		break;
#undef MAP_BUTTON

	default:
		return false;
	}

	NSUInteger buttons = [NSEvent pressedMouseButtons];
	*pressed = (buttons & (1 << button)) != 0;
	return true;
}

bool obs_hotkeys_platform_is_pressed(obs_hotkeys_platform_t *plat,
				     obs_key_t key)
{
	bool mouse_pressed = false;
	if (mouse_button_pressed(key, &mouse_pressed))
		return mouse_pressed;

	if (!plat)
		return false;

	if (key >= OBS_KEY_LAST_VALUE)
		return false;

	/* if secure input is activated, kill hotkeys.
	 *
	 * TODO: rewrite all mac hotkey code, suspect there's a bug in 10.15
	 * causing the crash internally.  */
	if (plat->secure_input_activated) {
		return false;
	}

	for (size_t i = 0; i < plat->keys[key].num;) {
		IOHIDElementRef element = plat->keys[key].array[i];
		IOHIDValueRef value = 0;
		IOHIDDeviceRef device = IOHIDElementGetDevice(element);

		if (device == NULL) {
			continue;
		}

		if (using_10_15_or_above && IsSecureEventInputEnabled()) {
			plat->secure_input_activated = true;
			return false;
		}

		if (IOHIDDeviceGetValue(device, element, &value) !=
		    kIOReturnSuccess) {
			i += 1;
			continue;
		}

		if (!value) {
			CFRelease(element);
			da_erase(plat->keys[key], i);
			continue;
		}

		if (IOHIDValueGetIntegerValue(value) == 1)
			return true;

		i += 1;
	}

	return false;
}

void *obs_graphics_thread_autorelease(void *param)
{
	@autoreleasepool {
		return obs_graphics_thread(param);
	}
}

bool obs_graphics_thread_loop_autorelease(struct obs_graphics_context *context)
{
	@autoreleasepool {
		return obs_graphics_thread_loop(context);
	}
}
