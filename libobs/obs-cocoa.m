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

#include <objc/objc.h>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>
#import <CoreGraphics/CoreGraphics.h>

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

static void log_os_name(id pi, SEL UTF8StringSel)
{
	typedef int (*os_func)(id, SEL);
	os_func operatingSystem = (os_func)objc_msgSend;
	unsigned long os_id = (unsigned long)operatingSystem(
		pi, sel_registerName("operatingSystem"));

	typedef id (*os_name_func)(id, SEL);
	os_name_func operatingSystemName = (os_name_func)objc_msgSend;
	id os = operatingSystemName(pi,
				    sel_registerName("operatingSystemName"));
	typedef const char *(*utf8_func)(id, SEL);
	utf8_func UTF8String = (utf8_func)objc_msgSend;
	const char *name = UTF8String(os, UTF8StringSel);

	if (os_id == 5 /*NSMACHOperatingSystem*/) {
		blog(LOG_INFO, "OS Name: Mac OS X (%s)", name);
		return;
	}

	blog(LOG_INFO, "OS Name: %s", name ? name : "Unknown");
}

static void log_os_version(id pi, SEL UTF8StringSel)
{
	typedef id (*version_func)(id, SEL);
	version_func operatingSystemVersionString = (version_func)objc_msgSend;
	id vs = operatingSystemVersionString(
		pi, sel_registerName("operatingSystemVersionString"));
	typedef const char *(*utf8_func)(id, SEL);
	utf8_func UTF8String = (utf8_func)objc_msgSend;
	const char *version = UTF8String(vs, UTF8StringSel);

	blog(LOG_INFO, "OS Version: %s", version ? version : "Unknown");
}

static void log_os(void)
{
	Class NSProcessInfo = objc_getClass("NSProcessInfo");
	typedef id (*func)(id, SEL);
	func processInfo = (func)objc_msgSend;
	id pi = processInfo((id)NSProcessInfo, sel_registerName("processInfo"));

	SEL UTF8String = sel_registerName("UTF8String");

	log_os_name(pi, UTF8String);
	log_os_version(pi, UTF8String);
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
	CFTypeRef monitor;
	bool is_key_down[OBS_KEY_LAST_VALUE];
	TISInputSourceRef tis;
	CFDataRef layout_data;
	UCKeyboardLayout *layout;
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

#pragma GCC diagnostic ignored "-Winitializer-overrides"
static const int virtual_keys[] = {
	[0 ... OBS_KEY_LAST_VALUE] = INVALID_KEY,

	[OBS_KEY_A] = kVK_ANSI_A,
	[OBS_KEY_B] = kVK_ANSI_B,
	[OBS_KEY_C] = kVK_ANSI_C,
	[OBS_KEY_D] = kVK_ANSI_D,
	[OBS_KEY_E] = kVK_ANSI_E,
	[OBS_KEY_F] = kVK_ANSI_F,
	[OBS_KEY_G] = kVK_ANSI_G,
	[OBS_KEY_H] = kVK_ANSI_H,
	[OBS_KEY_I] = kVK_ANSI_I,
	[OBS_KEY_J] = kVK_ANSI_J,
	[OBS_KEY_K] = kVK_ANSI_K,
	[OBS_KEY_L] = kVK_ANSI_L,
	[OBS_KEY_M] = kVK_ANSI_M,
	[OBS_KEY_N] = kVK_ANSI_N,
	[OBS_KEY_O] = kVK_ANSI_O,
	[OBS_KEY_P] = kVK_ANSI_P,
	[OBS_KEY_Q] = kVK_ANSI_Q,
	[OBS_KEY_R] = kVK_ANSI_R,
	[OBS_KEY_S] = kVK_ANSI_S,
	[OBS_KEY_T] = kVK_ANSI_T,
	[OBS_KEY_U] = kVK_ANSI_U,
	[OBS_KEY_V] = kVK_ANSI_V,
	[OBS_KEY_W] = kVK_ANSI_W,
	[OBS_KEY_X] = kVK_ANSI_X,
	[OBS_KEY_Y] = kVK_ANSI_Y,
	[OBS_KEY_Z] = kVK_ANSI_Z,

	[OBS_KEY_1] = kVK_ANSI_1,
	[OBS_KEY_2] = kVK_ANSI_2,
	[OBS_KEY_3] = kVK_ANSI_3,
	[OBS_KEY_4] = kVK_ANSI_4,
	[OBS_KEY_5] = kVK_ANSI_5,
	[OBS_KEY_6] = kVK_ANSI_6,
	[OBS_KEY_7] = kVK_ANSI_7,
	[OBS_KEY_8] = kVK_ANSI_8,
	[OBS_KEY_9] = kVK_ANSI_9,
	[OBS_KEY_0] = kVK_ANSI_0,

	[OBS_KEY_RETURN] = kVK_Return,
	[OBS_KEY_ESCAPE] = kVK_Escape,
	[OBS_KEY_BACKSPACE] = kVK_Delete,
	[OBS_KEY_TAB] = kVK_Tab,
	[OBS_KEY_SPACE] = kVK_Space,
	[OBS_KEY_MINUS] = kVK_ANSI_Minus,
	[OBS_KEY_EQUAL] = kVK_ANSI_Equal,
	[OBS_KEY_BRACKETLEFT] = kVK_ANSI_LeftBracket,
	[OBS_KEY_BRACKETRIGHT] = kVK_ANSI_RightBracket,
	[OBS_KEY_BACKSLASH] = kVK_ANSI_Backslash,
	[OBS_KEY_SEMICOLON] = kVK_ANSI_Semicolon,
	[OBS_KEY_QUOTE] = kVK_ANSI_Quote,
	[OBS_KEY_DEAD_GRAVE] = kVK_ANSI_Grave,
	[OBS_KEY_COMMA] = kVK_ANSI_Comma,
	[OBS_KEY_PERIOD] = kVK_ANSI_Period,
	[OBS_KEY_SLASH] = kVK_ANSI_Slash,
	[OBS_KEY_CAPSLOCK] = kVK_CapsLock,
	[OBS_KEY_SECTION] = kVK_ISO_Section,

	[OBS_KEY_F1] = kVK_F1,
	[OBS_KEY_F2] = kVK_F2,
	[OBS_KEY_F3] = kVK_F3,
	[OBS_KEY_F4] = kVK_F4,
	[OBS_KEY_F5] = kVK_F5,
	[OBS_KEY_F6] = kVK_F6,
	[OBS_KEY_F7] = kVK_F7,
	[OBS_KEY_F8] = kVK_F8,
	[OBS_KEY_F9] = kVK_F9,
	[OBS_KEY_F10] = kVK_F10,
	[OBS_KEY_F11] = kVK_F11,
	[OBS_KEY_F12] = kVK_F12,

	[OBS_KEY_HELP] = kVK_Help,
	[OBS_KEY_HOME] = kVK_Home,
	[OBS_KEY_PAGEUP] = kVK_PageUp,
	[OBS_KEY_DELETE] = kVK_ForwardDelete,
	[OBS_KEY_END] = kVK_End,
	[OBS_KEY_PAGEDOWN] = kVK_PageDown,

	[OBS_KEY_RIGHT] = kVK_RightArrow,
	[OBS_KEY_LEFT] = kVK_LeftArrow,
	[OBS_KEY_DOWN] = kVK_DownArrow,
	[OBS_KEY_UP] = kVK_UpArrow,

	[OBS_KEY_CLEAR] = kVK_ANSI_KeypadClear,
	[OBS_KEY_NUMSLASH] = kVK_ANSI_KeypadDivide,
	[OBS_KEY_NUMASTERISK] = kVK_ANSI_KeypadMultiply,
	[OBS_KEY_NUMMINUS] = kVK_ANSI_KeypadMinus,
	[OBS_KEY_NUMPLUS] = kVK_ANSI_KeypadPlus,
	[OBS_KEY_ENTER] = kVK_ANSI_KeypadEnter,

	[OBS_KEY_NUM1] = kVK_ANSI_Keypad1,
	[OBS_KEY_NUM2] = kVK_ANSI_Keypad2,
	[OBS_KEY_NUM3] = kVK_ANSI_Keypad3,
	[OBS_KEY_NUM4] = kVK_ANSI_Keypad4,
	[OBS_KEY_NUM5] = kVK_ANSI_Keypad5,
	[OBS_KEY_NUM6] = kVK_ANSI_Keypad6,
	[OBS_KEY_NUM7] = kVK_ANSI_Keypad7,
	[OBS_KEY_NUM8] = kVK_ANSI_Keypad8,
	[OBS_KEY_NUM9] = kVK_ANSI_Keypad9,
	[OBS_KEY_NUM0] = kVK_ANSI_Keypad0,

	[OBS_KEY_NUMPERIOD] = kVK_ANSI_KeypadDecimal,
	[OBS_KEY_NUMEQUAL] = kVK_ANSI_KeypadEquals,

	[OBS_KEY_F13] = kVK_F13,
	[OBS_KEY_F14] = kVK_F14,
	[OBS_KEY_F15] = kVK_F15,
	[OBS_KEY_F16] = kVK_F16,
	[OBS_KEY_F17] = kVK_F17,
	[OBS_KEY_F18] = kVK_F18,
	[OBS_KEY_F19] = kVK_F19,
	[OBS_KEY_F20] = kVK_F20,

	[OBS_KEY_CONTROL] = kVK_Control,
	[OBS_KEY_SHIFT] = kVK_Shift,
	[OBS_KEY_ALT] = kVK_Option,
	[OBS_KEY_META] = kVK_Command,
	[OBS_KEY_CONTROL] = kVK_RightControl,
};

int obs_key_to_virtual_key(obs_key_t code)
{
	return virtual_keys[code];
}

obs_key_t obs_key_from_virtual_key(int code)
{
	if (code == kVK_RightShift)
		return OBS_KEY_SHIFT;
	if (code == kVK_RightOption)
		return OBS_KEY_ALT;
	if (code == kVK_RightCommand)
		return OBS_KEY_META;
	if (code == kVK_RightControl)
		return OBS_KEY_META;
	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++) {
		if (virtual_keys[i] == code) {
			return i;
		}
	}
	return OBS_KEY_NONE;
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

static void handle_monitor_event(obs_hotkeys_platform_t *plat, NSEvent *event)
{
	if (event.type == NSEventTypeFlagsChanged) {
		NSEventModifierFlags flags = event.modifierFlags;
		plat->is_key_down[OBS_KEY_CAPSLOCK] =
			!!(flags & NSEventModifierFlagCapsLock);
		plat->is_key_down[OBS_KEY_SHIFT] =
			!!(flags & NSEventModifierFlagShift);
		plat->is_key_down[OBS_KEY_ALT] =
			!!(flags & NSEventModifierFlagOption);
		plat->is_key_down[OBS_KEY_META] =
			!!(flags & NSEventModifierFlagCommand);
		plat->is_key_down[OBS_KEY_CONTROL] =
			!!(flags & NSEventModifierFlagControl);
	} else if (event.type == NSEventTypeKeyDown ||
		   event.type == NSEventTypeKeyUp) {
		plat->is_key_down[obs_key_from_virtual_key(event.keyCode)] =
			(event.type == NSEventTypeKeyDown);
	}
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

	void (^handler)(NSEvent *) = ^(NSEvent *event) {
		handle_monitor_event(plat, event);
	};
	plat->monitor = (__bridge CFTypeRef)[NSEvent
		addGlobalMonitorForEventsMatchingMask:NSEventMaskKeyDown |
						      NSEventMaskKeyUp |
						      NSEventMaskFlagsChanged
					      handler:handler];

	plat->tis = TISCopyCurrentKeyboardLayoutInputSource();
	plat->layout_data = (CFDataRef)TISGetInputSourceProperty(
		plat->tis, kTISPropertyUnicodeKeyLayoutData);

	if (!plat->layout_data) {
		blog(LOG_ERROR, "hotkeys-cocoa: Failed getting LayoutData");
		goto fail;
	}

	CFRetain(plat->layout_data);
	plat->layout = (UCKeyboardLayout *)CFDataGetBytePtr(plat->layout_data);

	return true;

fail:
	hotkeys_release(plat);
	*plat_ = NULL;
	return false;
}

static inline void free_hotkeys_platform(obs_hotkeys_platform_t *plat)
{
	if (!plat)
		return;

	if (plat->monitor) {
		CFRelease(plat->monitor);
		plat->monitor = NULL;
	}

	if (plat->tis) {
		CFRelease(plat->tis);
		plat->tis = NULL;
	}

	if (plat->layout_data) {
		CFRelease(plat->layout_data);
		plat->layout_data = NULL;
	}

	bfree(plat);
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

	Class NSEvent = objc_getClass("NSEvent");
	SEL pressedMouseButtonsSel = sel_registerName("pressedMouseButtons");
	typedef int (*func)(id, SEL);
	func pressedMouseButtons = (func)objc_msgSend;
	NSUInteger buttons = (NSUInteger)pressedMouseButtons(
		(id)NSEvent, pressedMouseButtonsSel);

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

	return plat->is_key_down[key];
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
