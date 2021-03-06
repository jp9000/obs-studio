/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

#include <time.h>
#include <stdio.h>
#include <wchar.h>
#include <chrono>
#include <ratio>
#include <string>
#include <sstream>
#include <mutex>
#include <util/bmem.h>
#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <util/cf-parser.h>
#include <obs-config.h>
#include <obs.hpp>

#include <QGuiApplication>
#include <QProxyStyle>
#include <QScreen>
#include <QProcess>
#include <QAccessible>

#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include "log-viewer.hpp"
#include "slider-ignorewheel.hpp"
#include "window-basic-main.hpp"
#include "window-basic-settings.hpp"
#include "crash-report.hpp"
#include "platform.hpp"

#include <fstream>

#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#else
#include <signal.h>
#include <pthread.h>
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <obs-nix-platform.h>
#include <QX11Info>

#ifdef ENABLE_WAYLAND
#include <qpa/qplatformnativeinterface.h>
#endif

#endif

#include <iostream>

#include "ui-config.h"

using namespace std;

static log_handler_t def_log_handler;

static string currentLogFile;
static string lastLogFile;
static string lastCrashLogFile;

bool portable_mode = false;
static bool multi = false;
static bool log_verbose = false;
static bool unfiltered_log = false;
bool opt_start_streaming = false;
bool opt_start_recording = false;
bool opt_studio_mode = false;
bool opt_start_replaybuffer = false;
bool opt_start_virtualcam = false;
bool opt_minimize_tray = false;
bool opt_allow_opengl = false;
bool opt_always_on_top = false;
bool opt_disable_high_dpi_scaling = false;
bool opt_disable_updater = false;
string opt_starting_collection;
string opt_starting_profile;
string opt_starting_scene;

bool restart = false;

QPointer<OBSLogViewer> obsLogViewer;

// GPU hint exports for AMD/NVIDIA laptops
#ifdef _MSC_VER
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

QObject *CreateShortcutFilter()
{
	return new OBSEventFilter([](QObject *obj, QEvent *event) {
		auto mouse_event = [](QMouseEvent &event) {
			if (!App()->HotkeysEnabledInFocus() &&
			    event.button() != Qt::LeftButton)
				return true;

			obs_key_combination_t hotkey = {0, OBS_KEY_NONE};
			bool pressed = event.type() == QEvent::MouseButtonPress;

			switch (event.button()) {
			case Qt::NoButton:
			case Qt::LeftButton:
			case Qt::RightButton:
			case Qt::AllButtons:
			case Qt::MouseButtonMask:
				return false;

			case Qt::MiddleButton:
				hotkey.key = OBS_KEY_MOUSE3;
				break;

#define MAP_BUTTON(i, j)                       \
	case Qt::ExtraButton##i:               \
		hotkey.key = OBS_KEY_MOUSE##j; \
		break;
				MAP_BUTTON(1, 4);
				MAP_BUTTON(2, 5);
				MAP_BUTTON(3, 6);
				MAP_BUTTON(4, 7);
				MAP_BUTTON(5, 8);
				MAP_BUTTON(6, 9);
				MAP_BUTTON(7, 10);
				MAP_BUTTON(8, 11);
				MAP_BUTTON(9, 12);
				MAP_BUTTON(10, 13);
				MAP_BUTTON(11, 14);
				MAP_BUTTON(12, 15);
				MAP_BUTTON(13, 16);
				MAP_BUTTON(14, 17);
				MAP_BUTTON(15, 18);
				MAP_BUTTON(16, 19);
				MAP_BUTTON(17, 20);
				MAP_BUTTON(18, 21);
				MAP_BUTTON(19, 22);
				MAP_BUTTON(20, 23);
				MAP_BUTTON(21, 24);
				MAP_BUTTON(22, 25);
				MAP_BUTTON(23, 26);
				MAP_BUTTON(24, 27);
#undef MAP_BUTTON
			}

			hotkey.modifiers = TranslateQtKeyboardEventModifiers(
				event.modifiers());

			obs_hotkey_inject_event(hotkey, pressed);
			return true;
		};

		auto key_event = [&](QKeyEvent *event) {
			if (!App()->HotkeysEnabledInFocus())
				return true;

			QDialog *dialog = qobject_cast<QDialog *>(obj);

			obs_key_combination_t hotkey = {0, OBS_KEY_NONE};
			bool pressed = event->type() == QEvent::KeyPress;

			switch (event->key()) {
			case Qt::Key_Shift:
			case Qt::Key_Control:
			case Qt::Key_Alt:
			case Qt::Key_Meta:
				break;

#ifdef __APPLE__
			case Qt::Key_CapsLock:
				// kVK_CapsLock == 57
				hotkey.key = obs_key_from_virtual_key(57);
				pressed = true;
				break;
#endif

			case Qt::Key_Enter:
			case Qt::Key_Escape:
			case Qt::Key_Return:
				if (dialog && pressed)
					return false;
				/* Falls through. */
			default:
				hotkey.key = obs_key_from_virtual_key(
					event->nativeVirtualKey());
			}

			if (event->isAutoRepeat())
				return true;

			hotkey.modifiers = TranslateQtKeyboardEventModifiers(
				event->modifiers());

			obs_hotkey_inject_event(hotkey, pressed);
			return true;
		};

		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
			return mouse_event(*static_cast<QMouseEvent *>(event));

		/*case QEvent::MouseButtonDblClick:
		case QEvent::Wheel:*/
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return key_event(static_cast<QKeyEvent *>(event));

		default:
			return false;
		}
	});
}

string CurrentTimeString()
{
	using namespace std::chrono;

	struct tm tstruct;
	char buf[80];

	auto tp = system_clock::now();
	auto now = system_clock::to_time_t(tp);
	tstruct = *localtime(&now);

	size_t written = strftime(buf, sizeof(buf), "%X", &tstruct);
	if (ratio_less<system_clock::period, seconds::period>::value &&
	    written && (sizeof(buf) - written) > 5) {
		auto tp_secs = time_point_cast<seconds>(tp);
		auto millis = duration_cast<milliseconds>(tp - tp_secs).count();

		snprintf(buf + written, sizeof(buf) - written, ".%03u",
			 static_cast<unsigned>(millis));
	}

	return buf;
}

string CurrentDateTimeString()
{
	time_t now = time(0);
	struct tm tstruct;
	char buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d, %X", &tstruct);
	return buf;
}

static inline void LogString(fstream &logFile, const char *timeString,
			     char *str, int log_level)
{
	string msg;
	msg += timeString;
	msg += str;

	logFile << msg << endl;

	if (!!obsLogViewer)
		QMetaObject::invokeMethod(obsLogViewer.data(), "AddLine",
					  Qt::QueuedConnection,
					  Q_ARG(int, log_level),
					  Q_ARG(QString, QString(msg.c_str())));
}

static inline void LogStringChunk(fstream &logFile, char *str, int log_level)
{
	char *nextLine = str;
	string timeString = CurrentTimeString();
	timeString += ": ";

	while (*nextLine) {
		char *nextLine = strchr(str, '\n');
		if (!nextLine)
			break;

		if (nextLine != str && nextLine[-1] == '\r') {
			nextLine[-1] = 0;
		} else {
			nextLine[0] = 0;
		}

		LogString(logFile, timeString.c_str(), str, log_level);
		nextLine++;
		str = nextLine;
	}

	LogString(logFile, timeString.c_str(), str, log_level);
}

#define MAX_REPEATED_LINES 30
#define MAX_CHAR_VARIATION (255 * 3)

static inline int sum_chars(const char *str)
{
	int val = 0;
	for (; *str != 0; str++)
		val += *str;

	return val;
}

static inline bool too_many_repeated_entries(fstream &logFile, const char *msg,
					     const char *output_str)
{
	static mutex log_mutex;
	static const char *last_msg_ptr = nullptr;
	static int last_char_sum = 0;
	static char cmp_str[4096];
	static int rep_count = 0;

	int new_sum = sum_chars(output_str);

	lock_guard<mutex> guard(log_mutex);

	if (unfiltered_log) {
		return false;
	}

	if (last_msg_ptr == msg) {
		int diff = std::abs(new_sum - last_char_sum);
		if (diff < MAX_CHAR_VARIATION) {
			return (rep_count++ >= MAX_REPEATED_LINES);
		}
	}

	if (rep_count > MAX_REPEATED_LINES) {
		logFile << CurrentTimeString()
			<< ": Last log entry repeated for "
			<< to_string(rep_count - MAX_REPEATED_LINES)
			<< " more lines" << endl;
	}

	last_msg_ptr = msg;
	strcpy(cmp_str, output_str);
	last_char_sum = new_sum;
	rep_count = 0;

	return false;
}

static void do_log(int log_level, const char *msg, va_list args, void *param)
{
	fstream &logFile = *static_cast<fstream *>(param);
	char str[4096];

#ifndef _WIN32
	va_list args2;
	va_copy(args2, args);
#endif

	vsnprintf(str, 4095, msg, args);

#ifdef _WIN32
	if (IsDebuggerPresent()) {
		int wNum = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
		if (wNum > 1) {
			static wstring wide_buf;
			static mutex wide_mutex;

			lock_guard<mutex> lock(wide_mutex);
			wide_buf.reserve(wNum + 1);
			wide_buf.resize(wNum - 1);
			MultiByteToWideChar(CP_UTF8, 0, str, -1, &wide_buf[0],
					    wNum);
			wide_buf.push_back('\n');

			OutputDebugStringW(wide_buf.c_str());
		}
	}
#else
	def_log_handler(log_level, msg, args2, nullptr);
	va_end(args2);
#endif

	if (log_level <= LOG_INFO || log_verbose) {
		if (too_many_repeated_entries(logFile, msg, str))
			return;
		LogStringChunk(logFile, str, log_level);
	}

#if defined(_WIN32) && defined(OBS_DEBUGBREAK_ON_ERROR)
	if (log_level <= LOG_ERROR && IsDebuggerPresent())
		__debugbreak();
#endif
}

#define DEFAULT_LANG "en-US"

bool OBSApp::InitGlobalConfigDefaults()
{
	config_set_default_string(globalConfig, "General", "Language",
				  DEFAULT_LANG);
	config_set_default_uint(globalConfig, "General", "MaxLogs", 10);
	config_set_default_int(globalConfig, "General", "InfoIncrement", -1);
	config_set_default_string(globalConfig, "General", "ProcessPriority",
				  "Normal");
	config_set_default_bool(globalConfig, "General", "EnableAutoUpdates",
				true);

#if _WIN32
	config_set_default_string(globalConfig, "Video", "Renderer",
				  "Direct3D 11");
#else
	config_set_default_string(globalConfig, "Video", "Renderer", "OpenGL");
#endif

	config_set_default_bool(globalConfig, "BasicWindow", "PreviewEnabled",
				true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"PreviewProgramMode", false);
	config_set_default_bool(globalConfig, "BasicWindow",
				"SceneDuplicationMode", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SwapScenesMode",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "SnappingEnabled",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "ScreenSnapping",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "SourceSnapping",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "CenterSnapping",
				false);
	config_set_default_double(globalConfig, "BasicWindow", "SnapDistance",
				  10.0);
	config_set_default_bool(globalConfig, "BasicWindow",
				"RecordWhenStreaming", false);
	config_set_default_bool(globalConfig, "BasicWindow",
				"KeepRecordingWhenStreamStops", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SysTrayEnabled",
				true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"SysTrayWhenStarted", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SaveProjectors",
				false);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowTransitions",
				true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"ShowListboxToolbars", true);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowStatusBar",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowSourceIcons",
				true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"ShowContextToolbars", true);
	config_set_default_bool(globalConfig, "BasicWindow", "StudioModeLabels",
				true);

	if (!config_get_bool(globalConfig, "General", "Pre21Defaults")) {
		config_set_default_string(globalConfig, "General",
					  "CurrentTheme", DEFAULT_THEME);
	}

	config_set_default_string(globalConfig, "General", "HotkeyFocusType",
				  "NeverDisableHotkeys");

	config_set_default_bool(globalConfig, "BasicWindow",
				"VerticalVolControl", false);

	config_set_default_bool(globalConfig, "BasicWindow",
				"MultiviewMouseSwitch", true);

	config_set_default_bool(globalConfig, "BasicWindow",
				"MultiviewDrawNames", true);

	config_set_default_bool(globalConfig, "BasicWindow",
				"MultiviewDrawAreas", true);

#ifdef _WIN32
	uint32_t winver = GetWindowsVersion();

	config_set_default_bool(globalConfig, "Audio", "DisableAudioDucking",
				true);
	config_set_default_bool(globalConfig, "General", "BrowserHWAccel",
				winver > 0x601);
#endif

#ifdef __APPLE__
	config_set_default_bool(globalConfig, "General", "BrowserHWAccel",
				true);
	config_set_default_bool(globalConfig, "Video", "DisableOSXVSync", true);
	config_set_default_bool(globalConfig, "Video", "ResetOSXVSyncOnExit",
				true);
#endif

	config_set_default_bool(globalConfig, "BasicWindow",
				"MediaControlsCountdownTimer", true);

	return true;
}

static bool do_mkdir(const char *path)
{
	if (os_mkdirs(path) == MKDIR_ERROR) {
		OBSErrorBox(NULL, "Failed to create directory %s", path);
		return false;
	}

	return true;
}

static bool MakeUserDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-studio/logs") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-studio/profiler_data") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

#ifdef _WIN32
	if (GetConfigPath(path, sizeof(path), "obs-studio/crashes") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-studio/updates") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
#endif

	if (GetConfigPath(path, sizeof(path), "obs-studio/plugin_config") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static bool MakeUserProfileDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic/profiles") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic/scenes") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static string GetProfileDirFromName(const char *name)
{
	string outputPath;
	os_glob_t *glob;
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic/profiles") <= 0)
		return outputPath;

	strcat(path, "/*");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (!ent.directory)
			continue;

		strcpy(path, ent.path);
		strcat(path, "/basic.ini");

		ConfigFile config;
		if (config.Open(path, CONFIG_OPEN_EXISTING) != 0)
			continue;

		const char *curName =
			config_get_string(config, "General", "Name");
		if (astrcmpi(curName, name) == 0) {
			outputPath = ent.path;
			break;
		}
	}

	os_globfree(glob);

	if (!outputPath.empty()) {
		replace(outputPath.begin(), outputPath.end(), '\\', '/');
		const char *start = strrchr(outputPath.c_str(), '/');
		if (start)
			outputPath.erase(0, start - outputPath.c_str() + 1);
	}

	return outputPath;
}

static string GetSceneCollectionFileFromName(const char *name)
{
	string outputPath;
	os_glob_t *glob;
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic/scenes") <= 0)
		return outputPath;

	strcat(path, "/*.json");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (ent.directory)
			continue;

		obs_data_t *data =
			obs_data_create_from_json_file_safe(ent.path, "bak");
		const char *curName = obs_data_get_string(data, "name");

		if (astrcmpi(name, curName) == 0) {
			outputPath = ent.path;
			obs_data_release(data);
			break;
		}

		obs_data_release(data);
	}

	os_globfree(glob);

	if (!outputPath.empty()) {
		outputPath.resize(outputPath.size() - 5);
		replace(outputPath.begin(), outputPath.end(), '\\', '/');
		const char *start = strrchr(outputPath.c_str(), '/');
		if (start)
			outputPath.erase(0, start - outputPath.c_str() + 1);
	}

	return outputPath;
}

bool OBSApp::UpdatePre22MultiviewLayout(const char *layout)
{
	if (!layout)
		return false;

	if (astrcmpi(layout, "horizontaltop") == 0) {
		config_set_int(
			globalConfig, "BasicWindow", "MultiviewLayout",
			static_cast<int>(
				MultiviewLayout::HORIZONTAL_TOP_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "horizontalbottom") == 0) {
		config_set_int(
			globalConfig, "BasicWindow", "MultiviewLayout",
			static_cast<int>(
				MultiviewLayout::HORIZONTAL_BOTTOM_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "verticalleft") == 0) {
		config_set_int(
			globalConfig, "BasicWindow", "MultiviewLayout",
			static_cast<int>(
				MultiviewLayout::VERTICAL_LEFT_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "verticalright") == 0) {
		config_set_int(
			globalConfig, "BasicWindow", "MultiviewLayout",
			static_cast<int>(
				MultiviewLayout::VERTICAL_RIGHT_8_SCENES));
		return true;
	}

	return false;
}

bool OBSApp::InitGlobalConfig()
{
	char path[512];
	bool changed = false;

	int len = GetConfigPath(path, sizeof(path), "obs-studio/global.ini");
	if (len <= 0) {
		return false;
	}

	int errorcode = globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		OBSErrorBox(NULL, "Failed to open global.ini: %d", errorcode);
		return false;
	}

	if (!opt_starting_collection.empty()) {
		string path = GetSceneCollectionFileFromName(
			opt_starting_collection.c_str());
		if (!path.empty()) {
			config_set_string(globalConfig, "Basic",
					  "SceneCollection",
					  opt_starting_collection.c_str());
			config_set_string(globalConfig, "Basic",
					  "SceneCollectionFile", path.c_str());
			changed = true;
		}
	}

	if (!opt_starting_profile.empty()) {
		string path =
			GetProfileDirFromName(opt_starting_profile.c_str());
		if (!path.empty()) {
			config_set_string(globalConfig, "Basic", "Profile",
					  opt_starting_profile.c_str());
			config_set_string(globalConfig, "Basic", "ProfileDir",
					  path.c_str());
			changed = true;
		}
	}

	uint32_t lastVersion =
		config_get_int(globalConfig, "General", "LastVersion");

	if (!config_has_user_value(globalConfig, "General", "Pre19Defaults")) {
		bool useOldDefaults = lastVersion &&
				      lastVersion <
					      MAKE_SEMANTIC_VERSION(19, 0, 0);

		config_set_bool(globalConfig, "General", "Pre19Defaults",
				useOldDefaults);
		changed = true;
	}

	if (!config_has_user_value(globalConfig, "General", "Pre21Defaults")) {
		bool useOldDefaults = lastVersion &&
				      lastVersion <
					      MAKE_SEMANTIC_VERSION(21, 0, 0);

		config_set_bool(globalConfig, "General", "Pre21Defaults",
				useOldDefaults);
		changed = true;
	}

	if (!config_has_user_value(globalConfig, "General", "Pre23Defaults")) {
		bool useOldDefaults = lastVersion &&
				      lastVersion <
					      MAKE_SEMANTIC_VERSION(23, 0, 0);

		config_set_bool(globalConfig, "General", "Pre23Defaults",
				useOldDefaults);
		changed = true;
	}

#define PRE_24_1_DEFS "Pre24.1Defaults"
	if (!config_has_user_value(globalConfig, "General", PRE_24_1_DEFS)) {
		bool useOldDefaults = lastVersion &&
				      lastVersion <
					      MAKE_SEMANTIC_VERSION(24, 1, 0);

		config_set_bool(globalConfig, "General", PRE_24_1_DEFS,
				useOldDefaults);
		changed = true;
	}
#undef PRE_24_1_DEFS

	if (config_has_user_value(globalConfig, "BasicWindow",
				  "MultiviewLayout")) {
		const char *layout = config_get_string(
			globalConfig, "BasicWindow", "MultiviewLayout");
		changed |= UpdatePre22MultiviewLayout(layout);
	}

	if (lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(24, 0, 0)) {
		bool disableHotkeysInFocus = config_get_bool(
			globalConfig, "General", "DisableHotkeysInFocus");
		if (disableHotkeysInFocus)
			config_set_string(globalConfig, "General",
					  "HotkeyFocusType",
					  "DisableHotkeysInFocus");
		changed = true;
	}

	if (changed)
		config_save_safe(globalConfig, "tmp", nullptr);

	return InitGlobalConfigDefaults();
}

bool OBSApp::InitLocale()
{
	ProfileScope("OBSApp::InitLocale");
	const char *lang =
		config_get_string(globalConfig, "General", "Language");

	locale = lang;

	string englishPath;
	if (!GetDataFilePath("locale/" DEFAULT_LANG ".ini", englishPath)) {
		OBSErrorBox(NULL, "Failed to find locale/" DEFAULT_LANG ".ini");
		return false;
	}

	textLookup = text_lookup_create(englishPath.c_str());
	if (!textLookup) {
		OBSErrorBox(NULL, "Failed to create locale from file '%s'",
			    englishPath.c_str());
		return false;
	}

	bool userLocale =
		config_has_user_value(globalConfig, "General", "Language");
	bool defaultLang = astrcmpi(lang, DEFAULT_LANG) == 0;

	if (userLocale && defaultLang)
		return true;

	if (!userLocale && defaultLang) {
		for (auto &locale_ : GetPreferredLocales()) {
			if (locale_ == lang)
				return true;

			stringstream file;
			file << "locale/" << locale_ << ".ini";

			string path;
			if (!GetDataFilePath(file.str().c_str(), path))
				continue;

			if (!text_lookup_add(textLookup, path.c_str()))
				continue;

			blog(LOG_INFO, "Using preferred locale '%s'",
			     locale_.c_str());
			locale = locale_;
			return true;
		}

		return true;
	}

	stringstream file;
	file << "locale/" << lang << ".ini";

	string path;
	if (GetDataFilePath(file.str().c_str(), path)) {
		if (!text_lookup_add(textLookup, path.c_str()))
			blog(LOG_ERROR, "Failed to add locale file '%s'",
			     path.c_str());
	} else {
		blog(LOG_ERROR, "Could not find locale file '%s'",
		     file.str().c_str());
	}

	return true;
}

void OBSApp::AddExtraThemeColor(QPalette &pal, int group, const char *name,
				uint32_t color)
{
	std::function<void(QPalette::ColorGroup)> func;

#define DEF_PALETTE_ASSIGN(name)                              \
	do {                                                  \
		func = [&](QPalette::ColorGroup group) {      \
			pal.setColor(group, QPalette::name,   \
				     QColor::fromRgb(color)); \
		};                                            \
	} while (false)

	if (astrcmpi(name, "alternateBase") == 0) {
		DEF_PALETTE_ASSIGN(AlternateBase);
	} else if (astrcmpi(name, "base") == 0) {
		DEF_PALETTE_ASSIGN(Base);
	} else if (astrcmpi(name, "brightText") == 0) {
		DEF_PALETTE_ASSIGN(BrightText);
	} else if (astrcmpi(name, "button") == 0) {
		DEF_PALETTE_ASSIGN(Button);
	} else if (astrcmpi(name, "buttonText") == 0) {
		DEF_PALETTE_ASSIGN(ButtonText);
	} else if (astrcmpi(name, "brightText") == 0) {
		DEF_PALETTE_ASSIGN(BrightText);
	} else if (astrcmpi(name, "dark") == 0) {
		DEF_PALETTE_ASSIGN(Dark);
	} else if (astrcmpi(name, "highlight") == 0) {
		DEF_PALETTE_ASSIGN(Highlight);
	} else if (astrcmpi(name, "highlightedText") == 0) {
		DEF_PALETTE_ASSIGN(HighlightedText);
	} else if (astrcmpi(name, "light") == 0) {
		DEF_PALETTE_ASSIGN(Light);
	} else if (astrcmpi(name, "link") == 0) {
		DEF_PALETTE_ASSIGN(Link);
	} else if (astrcmpi(name, "linkVisited") == 0) {
		DEF_PALETTE_ASSIGN(LinkVisited);
	} else if (astrcmpi(name, "mid") == 0) {
		DEF_PALETTE_ASSIGN(Mid);
	} else if (astrcmpi(name, "midlight") == 0) {
		DEF_PALETTE_ASSIGN(Midlight);
	} else if (astrcmpi(name, "shadow") == 0) {
		DEF_PALETTE_ASSIGN(Shadow);
	} else if (astrcmpi(name, "text") == 0 ||
		   astrcmpi(name, "foreground") == 0) {
		DEF_PALETTE_ASSIGN(Text);
	} else if (astrcmpi(name, "toolTipBase") == 0) {
		DEF_PALETTE_ASSIGN(ToolTipBase);
	} else if (astrcmpi(name, "toolTipText") == 0) {
		DEF_PALETTE_ASSIGN(ToolTipText);
	} else if (astrcmpi(name, "windowText") == 0) {
		DEF_PALETTE_ASSIGN(WindowText);
	} else if (astrcmpi(name, "window") == 0 ||
		   astrcmpi(name, "background") == 0) {
		DEF_PALETTE_ASSIGN(Window);
	} else {
		return;
	}

#undef DEF_PALETTE_ASSIGN

	switch (group) {
	case QPalette::Disabled:
	case QPalette::Active:
	case QPalette::Inactive:
		func((QPalette::ColorGroup)group);
		break;
	default:
		func((QPalette::ColorGroup)QPalette::Disabled);
		func((QPalette::ColorGroup)QPalette::Active);
		func((QPalette::ColorGroup)QPalette::Inactive);
	}
}

struct CFParser {
	cf_parser cfp = {};
	inline ~CFParser() { cf_parser_free(&cfp); }
	inline operator cf_parser *() { return &cfp; }
	inline cf_parser *operator->() { return &cfp; }
};

void OBSApp::ParseExtraThemeData(const char *path)
{
	BPtr<char> data = os_quick_read_utf8_file(path);
	QPalette pal = palette();
	CFParser cfp;
	int ret;

	cf_parser_parse(cfp, data, path);

	while (cf_go_to_token(cfp, "OBSTheme", nullptr)) {
		if (!cf_next_token(cfp))
			return;

		int group = -1;

		if (cf_token_is(cfp, ":")) {
			ret = cf_next_token_should_be(cfp, ":", nullptr,
						      nullptr);
			if (ret != PARSE_SUCCESS)
				continue;

			if (!cf_next_token(cfp))
				return;

			if (cf_token_is(cfp, "disabled")) {
				group = QPalette::Disabled;
			} else if (cf_token_is(cfp, "active")) {
				group = QPalette::Active;
			} else if (cf_token_is(cfp, "inactive")) {
				group = QPalette::Inactive;
			} else {
				continue;
			}

			if (!cf_next_token(cfp))
				return;
		}

		if (!cf_token_is(cfp, "{"))
			continue;

		for (;;) {
			if (!cf_next_token(cfp))
				return;

			ret = cf_token_is_type(cfp, CFTOKEN_NAME, "name",
					       nullptr);
			if (ret != PARSE_SUCCESS)
				break;

			DStr name;
			dstr_copy_strref(name, &cfp->cur_token->str);

			ret = cf_next_token_should_be(cfp, ":", ";", nullptr);
			if (ret != PARSE_SUCCESS)
				continue;

			if (!cf_next_token(cfp))
				return;

			const char *array;
			uint32_t color = 0;

			if (cf_token_is(cfp, "#")) {
				array = cfp->cur_token->str.array;
				color = strtol(array + 1, nullptr, 16);

			} else if (cf_token_is(cfp, "rgb")) {
				ret = cf_next_token_should_be(cfp, "(", ";",
							      nullptr);
				if (ret != PARSE_SUCCESS)
					continue;
				if (!cf_next_token(cfp))
					return;

				array = cfp->cur_token->str.array;
				color |= strtol(array, nullptr, 10) << 16;

				ret = cf_next_token_should_be(cfp, ",", ";",
							      nullptr);
				if (ret != PARSE_SUCCESS)
					continue;
				if (!cf_next_token(cfp))
					return;

				array = cfp->cur_token->str.array;
				color |= strtol(array, nullptr, 10) << 8;

				ret = cf_next_token_should_be(cfp, ",", ";",
							      nullptr);
				if (ret != PARSE_SUCCESS)
					continue;
				if (!cf_next_token(cfp))
					return;

				array = cfp->cur_token->str.array;
				color |= strtol(array, nullptr, 10);

			} else if (cf_token_is(cfp, "white")) {
				color = 0xFFFFFF;

			} else if (cf_token_is(cfp, "black")) {
				color = 0;
			}

			if (!cf_go_to_token(cfp, ";", nullptr))
				return;

			AddExtraThemeColor(pal, group, name->array, color);
		}

		ret = cf_token_should_be(cfp, "}", "}", nullptr);
		if (ret != PARSE_SUCCESS)
			continue;
	}

	setPalette(pal);
}

bool OBSApp::SetTheme(std::string name, std::string path)
{
	theme = name;

	/* Check user dir first, then preinstalled themes. */
	if (path == "") {
		char userDir[512];
		name = "themes/" + name + ".qss";
		string temp = "obs-studio/" + name;
		int ret = GetConfigPath(userDir, sizeof(userDir), temp.c_str());

		if (ret > 0 && QFile::exists(userDir)) {
			path = string(userDir);
		} else if (!GetDataFilePath(name.c_str(), path)) {
			OBSErrorBox(NULL, "Failed to find %s.", name.c_str());
			return false;
		}
	}

	QString mpath = QString("file:///") + path.c_str();
	setPalette(defaultPalette);
	setStyleSheet(mpath);
	ParseExtraThemeData(path.c_str());

	emit StyleChanged();
	return true;
}

bool OBSApp::InitTheme()
{
	defaultPalette = palette();

	const char *themeName =
		config_get_string(globalConfig, "General", "CurrentTheme2");

	if (!themeName)
		/* Use deprecated "CurrentTheme" value if available */
		themeName = config_get_string(globalConfig, "General",
					      "CurrentTheme");
	if (!themeName)
		/* Use deprecated "Theme" value if available */
		themeName = config_get_string(globalConfig, "General", "Theme");
	if (!themeName)
		themeName = DEFAULT_THEME;
	if (!themeName)
		themeName = "Dark";

	if (strcmp(themeName, "Default") == 0)
		themeName = "System";

	if (strcmp(themeName, "System") != 0 && SetTheme(themeName))
		return true;

	return SetTheme("System");
}

OBSApp::OBSApp(int &argc, char **argv, profiler_name_store_t *store)
	: QApplication(argc, argv), profilerNameStore(store)
{
	sleepInhibitor = os_inhibit_sleep_create("OBS Video/audio");

#ifdef __APPLE__
	setWindowIcon(
		QIcon::fromTheme("obs", QIcon(":/res/images/obs_256x256.png")));
#else
	setWindowIcon(QIcon::fromTheme("obs", QIcon(":/res/images/obs.png")));
#endif
}

OBSApp::~OBSApp()
{
#ifdef _WIN32
	bool disableAudioDucking =
		config_get_bool(globalConfig, "Audio", "DisableAudioDucking");
	if (disableAudioDucking)
		DisableAudioDucking(false);
#endif

#ifdef __APPLE__
	bool vsyncDiabled =
		config_get_bool(globalConfig, "Video", "DisableOSXVSync");
	bool resetVSync =
		config_get_bool(globalConfig, "Video", "ResetOSXVSyncOnExit");
	if (vsyncDiabled && resetVSync)
		EnableOSXVSync(true);
#endif

	os_inhibit_sleep_set_active(sleepInhibitor, false);
	os_inhibit_sleep_destroy(sleepInhibitor);

	if (libobs_initialized)
		obs_shutdown();
}

static void move_basic_to_profiles(void)
{
	char path[512];
	char new_path[512];
	os_glob_t *glob;

	/* if not first time use */
	if (GetConfigPath(path, 512, "obs-studio/basic") <= 0)
		return;
	if (!os_file_exists(path))
		return;

	/* if the profiles directory doesn't already exist */
	if (GetConfigPath(new_path, 512, "obs-studio/basic/profiles") <= 0)
		return;
	if (os_file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/*.*");
	if (os_glob(path, 0, &glob) != 0)
		return;

	strcpy(path, new_path);

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		char *file;

		if (ent.directory)
			continue;

		file = strrchr(ent.path, '/');
		if (!file++)
			continue;

		if (astrcmpi(file, "scenes.json") == 0)
			continue;

		strcpy(new_path, path);
		strcat(new_path, "/");
		strcat(new_path, file);
		os_rename(ent.path, new_path);
	}

	os_globfree(glob);
}

static void move_basic_to_scene_collections(void)
{
	char path[512];
	char new_path[512];

	if (GetConfigPath(path, 512, "obs-studio/basic") <= 0)
		return;
	if (!os_file_exists(path))
		return;

	if (GetConfigPath(new_path, 512, "obs-studio/basic/scenes") <= 0)
		return;
	if (os_file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/scenes.json");
	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	strcat(new_path, ".json");

	os_rename(path, new_path);
}

void OBSApp::AppInit()
{
	ProfileScope("OBSApp::AppInit");

	if (!InitApplicationBundle())
		throw "Failed to initialize application bundle";
	if (!MakeUserDirs())
		throw "Failed to create required user directories";
	if (!InitGlobalConfig())
		throw "Failed to initialize global config";
	if (!InitLocale())
		throw "Failed to load locale";
	if (!InitTheme())
		throw "Failed to load theme";

	config_set_default_string(globalConfig, "Basic", "Profile",
				  Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "ProfileDir",
				  Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollection",
				  Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollectionFile",
				  Str("Untitled"));
	config_set_default_bool(globalConfig, "Basic", "ConfigOnNewProfile",
				true);

	if (!config_has_user_value(globalConfig, "Basic", "Profile")) {
		config_set_string(globalConfig, "Basic", "Profile",
				  Str("Untitled"));
		config_set_string(globalConfig, "Basic", "ProfileDir",
				  Str("Untitled"));
	}

	if (!config_has_user_value(globalConfig, "Basic", "SceneCollection")) {
		config_set_string(globalConfig, "Basic", "SceneCollection",
				  Str("Untitled"));
		config_set_string(globalConfig, "Basic", "SceneCollectionFile",
				  Str("Untitled"));
	}

#ifdef _WIN32
	bool disableAudioDucking =
		config_get_bool(globalConfig, "Audio", "DisableAudioDucking");
	if (disableAudioDucking)
		DisableAudioDucking(true);
#endif

#ifdef __APPLE__
	if (config_get_bool(globalConfig, "Video", "DisableOSXVSync"))
		EnableOSXVSync(false);
#endif

	UpdateHotkeyFocusSetting(false);

	move_basic_to_profiles();
	move_basic_to_scene_collections();

	if (!MakeUserProfileDirs())
		throw "Failed to create profile directories";
}

const char *OBSApp::GetRenderModule() const
{
	const char *renderer =
		config_get_string(globalConfig, "Video", "Renderer");

	return (astrcmpi(renderer, "Direct3D 11") == 0) ? DL_D3D11 : DL_OPENGL;
}

static bool StartupOBS(const char *locale, profiler_name_store_t *store)
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/plugin_config") <= 0)
		return false;

	return obs_startup(locale, path, store);
}

inline void OBSApp::ResetHotkeyState(bool inFocus)
{
	obs_hotkey_enable_background_press(
		(inFocus && enableHotkeysInFocus) ||
		(!inFocus && enableHotkeysOutOfFocus));
}

void OBSApp::UpdateHotkeyFocusSetting(bool resetState)
{
	enableHotkeysInFocus = true;
	enableHotkeysOutOfFocus = true;

	const char *hotkeyFocusType =
		config_get_string(globalConfig, "General", "HotkeyFocusType");

	if (astrcmpi(hotkeyFocusType, "DisableHotkeysInFocus") == 0) {
		enableHotkeysInFocus = false;
	} else if (astrcmpi(hotkeyFocusType, "DisableHotkeysOutOfFocus") == 0) {
		enableHotkeysOutOfFocus = false;
	}

	if (resetState)
		ResetHotkeyState(applicationState() == Qt::ApplicationActive);
}

void OBSApp::DisableHotkeys()
{
	enableHotkeysInFocus = false;
	enableHotkeysOutOfFocus = false;
	ResetHotkeyState(applicationState() == Qt::ApplicationActive);
}

Q_DECLARE_METATYPE(VoidFunc)

void OBSApp::Exec(VoidFunc func)
{
	func();
}

static void ui_task_handler(obs_task_t task, void *param, bool wait)
{
	auto doTask = [=]() {
		/* to get clang-format to behave */
		task(param);
	};
	QMetaObject::invokeMethod(App(), "Exec",
				  wait ? WaitConnection() : Qt::AutoConnection,
				  Q_ARG(VoidFunc, doTask));
}

bool OBSApp::OBSInit()
{
	ProfileScope("OBSApp::OBSInit");

	setAttribute(Qt::AA_UseHighDpiPixmaps);
	setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

	qRegisterMetaType<VoidFunc>();

#if !defined(_WIN32) && !defined(__APPLE__)
	obs_set_nix_platform(OBS_NIX_PLATFORM_X11_GLX);
	if (QApplication::platformName() == "xcb") {
		if (getenv("OBS_USE_EGL")) {
			blog(LOG_INFO, "Using EGL/X11");
			obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL);
		}
		obs_set_nix_platform_display(QX11Info::display());
	}

#ifdef ENABLE_WAYLAND
	if (QApplication::platformName().contains("wayland")) {
		obs_set_nix_platform(OBS_NIX_PLATFORM_WAYLAND);
		QPlatformNativeInterface *native =
			QGuiApplication::platformNativeInterface();
		obs_set_nix_platform_display(
			native->nativeResourceForIntegration("display"));

		blog(LOG_INFO, "Platform: Wayland");
	}
#endif
#endif

	if (!StartupOBS(locale.c_str(), GetProfilerNameStore()))
		return false;

	libobs_initialized = true;

	obs_set_ui_task_handler(ui_task_handler);

#if defined(_WIN32) || defined(__APPLE__)
	bool browserHWAccel =
		config_get_bool(globalConfig, "General", "BrowserHWAccel");

	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "BrowserHWAccel", browserHWAccel);
	obs_apply_private_data(settings);
	obs_data_release(settings);

	blog(LOG_INFO, "Current Date/Time: %s",
	     CurrentDateTimeString().c_str());

	blog(LOG_INFO, "Browser Hardware Acceleration: %s",
	     browserHWAccel ? "true" : "false");
#endif

	blog(LOG_INFO, "Portable mode: %s", portable_mode ? "true" : "false");

	setQuitOnLastWindowClosed(false);

	mainWindow = new OBSBasic();

	mainWindow->setAttribute(Qt::WA_DeleteOnClose, true);
	connect(mainWindow, SIGNAL(destroyed()), this, SLOT(quit()));

	mainWindow->OBSInit();

	connect(this, &QGuiApplication::applicationStateChanged,
		[this](Qt::ApplicationState state) {
			ResetHotkeyState(state == Qt::ApplicationActive);
		});
	ResetHotkeyState(applicationState() == Qt::ApplicationActive);
	return true;
}

string OBSApp::GetVersionString() const
{
	stringstream ver;

#ifdef HAVE_OBSCONFIG_H
	ver << OBS_VERSION;
#else
	ver << LIBOBS_API_MAJOR_VER << "." << LIBOBS_API_MINOR_VER << "."
	    << LIBOBS_API_PATCH_VER;

#endif
	ver << " (";

#ifdef _WIN32
	if (sizeof(void *) == 8)
		ver << "64-bit, ";
	else
		ver << "32-bit, ";

	ver << "windows)";
#elif __APPLE__
	ver << "mac)";
#elif __OpenBSD__
	ver << "openbsd)";
#elif __FreeBSD__
	ver << "freebsd)";
#else /* assume linux for the time being */
	ver << "linux)";
#endif

	return ver.str();
}

bool OBSApp::IsPortableMode()
{
	return portable_mode;
}

bool OBSApp::IsUpdaterDisabled()
{
	return opt_disable_updater;
}

#ifdef __APPLE__
#define INPUT_AUDIO_SOURCE "coreaudio_input_capture"
#define OUTPUT_AUDIO_SOURCE "coreaudio_output_capture"
#elif _WIN32
#define INPUT_AUDIO_SOURCE "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"
#else
#define INPUT_AUDIO_SOURCE "pulse_input_capture"
#define OUTPUT_AUDIO_SOURCE "pulse_output_capture"
#endif

const char *OBSApp::InputAudioSource() const
{
	return INPUT_AUDIO_SOURCE;
}

const char *OBSApp::OutputAudioSource() const
{
	return OUTPUT_AUDIO_SOURCE;
}

const char *OBSApp::GetLastLog() const
{
	return lastLogFile.c_str();
}

const char *OBSApp::GetCurrentLog() const
{
	return currentLogFile.c_str();
}

const char *OBSApp::GetLastCrashLog() const
{
	return lastCrashLogFile.c_str();
}

bool OBSApp::TranslateString(const char *lookupVal, const char **out) const
{
	for (obs_frontend_translate_ui_cb cb : translatorHooks) {
		if (cb(lookupVal, out))
			return true;
	}

	return text_lookup_getstr(App()->GetTextLookup(), lookupVal, out);
}

QString OBSTranslator::translate(const char *context, const char *sourceText,
				 const char *disambiguation, int n) const
{
	const char *out = nullptr;
	if (!App()->TranslateString(sourceText, &out))
		return QString(sourceText);

	UNUSED_PARAMETER(context);
	UNUSED_PARAMETER(disambiguation);
	UNUSED_PARAMETER(n);
	return QT_UTF8(out);
}

static bool get_token(lexer *lex, string &str, base_token_type type)
{
	base_token token;
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != type)
		return false;

	str.assign(token.text.array, token.text.len);
	return true;
}

static bool expect_token(lexer *lex, const char *str, base_token_type type)
{
	base_token token;
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != type)
		return false;

	return strref_cmp(&token.text, str) == 0;
}

static uint64_t convert_log_name(bool has_prefix, const char *name)
{
	BaseLexer lex;
	string year, month, day, hour, minute, second;

	lexer_start(lex, name);

	if (has_prefix) {
		string temp;
		if (!get_token(lex, temp, BASETOKEN_ALPHA))
			return 0;
	}

	if (!get_token(lex, year, BASETOKEN_DIGIT))
		return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!get_token(lex, month, BASETOKEN_DIGIT))
		return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!get_token(lex, day, BASETOKEN_DIGIT))
		return 0;
	if (!get_token(lex, hour, BASETOKEN_DIGIT))
		return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!get_token(lex, minute, BASETOKEN_DIGIT))
		return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!get_token(lex, second, BASETOKEN_DIGIT))
		return 0;

	stringstream timestring;
	timestring << year << month << day << hour << minute << second;
	return std::stoull(timestring.str());
}

static void delete_oldest_file(bool has_prefix, const char *location)
{
	BPtr<char> logDir(GetConfigPathPtr(location));
	string oldestLog;
	uint64_t oldest_ts = (uint64_t)-1;
	struct os_dirent *entry;

	unsigned int maxLogs = (unsigned int)config_get_uint(
		App()->GlobalConfig(), "General", "MaxLogs");

	os_dir_t *dir = os_opendir(logDir);
	if (dir) {
		unsigned int count = 0;

		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			uint64_t ts =
				convert_log_name(has_prefix, entry->d_name);

			if (ts) {
				if (ts < oldest_ts) {
					oldestLog = entry->d_name;
					oldest_ts = ts;
				}

				count++;
			}
		}

		os_closedir(dir);

		if (count > maxLogs) {
			stringstream delPath;

			delPath << logDir << "/" << oldestLog;
			os_unlink(delPath.str().c_str());
		}
	}
}

static void get_last_log(bool has_prefix, const char *subdir_to_use,
			 std::string &last)
{
	BPtr<char> logDir(GetConfigPathPtr(subdir_to_use));
	struct os_dirent *entry;
	os_dir_t *dir = os_opendir(logDir);
	uint64_t highest_ts = 0;

	if (dir) {
		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			uint64_t ts =
				convert_log_name(has_prefix, entry->d_name);

			if (ts > highest_ts) {
				last = entry->d_name;
				highest_ts = ts;
			}
		}

		os_closedir(dir);
	}
}

string GenerateTimeDateFilename(const char *extension, bool noSpace)
{
	time_t now = time(0);
	char file[256] = {};
	struct tm *cur_time;

	cur_time = localtime(&now);
	snprintf(file, sizeof(file), "%d-%02d-%02d%c%02d-%02d-%02d.%s",
		 cur_time->tm_year + 1900, cur_time->tm_mon + 1,
		 cur_time->tm_mday, noSpace ? '_' : ' ', cur_time->tm_hour,
		 cur_time->tm_min, cur_time->tm_sec, extension);

	return string(file);
}

string GenerateSpecifiedFilename(const char *extension, bool noSpace,
				 const char *format)
{
	BPtr<char> filename =
		os_generate_formatted_filename(extension, !noSpace, format);
	return string(filename);
}

static void FindBestFilename(string &strPath, bool noSpace)
{
	int num = 2;

	if (!os_file_exists(strPath.c_str()))
		return;

	const char *ext = strrchr(strPath.c_str(), '.');
	if (!ext)
		return;

	int extStart = int(ext - strPath.c_str());
	for (;;) {
		string testPath = strPath;
		string numStr;

		numStr = noSpace ? "_" : " (";
		numStr += to_string(num++);
		if (!noSpace)
			numStr += ")";

		testPath.insert(extStart, numStr);

		if (!os_file_exists(testPath.c_str())) {
			strPath = testPath;
			break;
		}
	}
}

static void ensure_directory_exists(string &path)
{
	replace(path.begin(), path.end(), '\\', '/');

	size_t last = path.rfind('/');
	if (last == string::npos)
		return;

	string directory = path.substr(0, last);
	os_mkdirs(directory.c_str());
}

static void remove_reserved_file_characters(string &s)
{
	replace(s.begin(), s.end(), '\\', '/');
	replace(s.begin(), s.end(), '*', '_');
	replace(s.begin(), s.end(), '?', '_');
	replace(s.begin(), s.end(), '"', '_');
	replace(s.begin(), s.end(), '|', '_');
	replace(s.begin(), s.end(), ':', '_');
	replace(s.begin(), s.end(), '>', '_');
	replace(s.begin(), s.end(), '<', '_');
}

string GetFormatString(const char *format, const char *prefix,
		       const char *suffix)
{
	string f;

	if (prefix && *prefix) {
		f += prefix;
		if (f.back() != ' ')
			f += " ";
	}

	f += format;

	if (suffix && *suffix) {
		if (*suffix != ' ')
			f += " ";
		f += suffix;
	}

	remove_reserved_file_characters(f);

	return f;
}

string GetOutputFilename(const char *path, const char *ext, bool noSpace,
			 bool overwrite, const char *format)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	os_dir_t *dir = path && path[0] ? os_opendir(path) : nullptr;

	if (!dir) {
		if (main->isVisible())
			OBSMessageBox::warning(main,
					       QTStr("Output.BadPath.Title"),
					       QTStr("Output.BadPath.Text"));
		else
			main->SysTrayNotify(QTStr("Output.BadPath.Text"),
					    QSystemTrayIcon::Warning);
		return "";
	}

	os_closedir(dir);

	string strPath;
	strPath += path;

	char lastChar = strPath.back();
	if (lastChar != '/' && lastChar != '\\')
		strPath += "/";

	strPath += GenerateSpecifiedFilename(ext, noSpace, format);
	ensure_directory_exists(strPath);
	if (!overwrite)
		FindBestFilename(strPath, noSpace);

	return strPath;
}

vector<pair<string, string>> GetLocaleNames()
{
	string path;
	if (!GetDataFilePath("locale.ini", path))
		throw "Could not find locale.ini path";

	ConfigFile ini;
	if (ini.Open(path.c_str(), CONFIG_OPEN_EXISTING) != 0)
		throw "Could not open locale.ini";

	size_t sections = config_num_sections(ini);

	vector<pair<string, string>> names;
	names.reserve(sections);
	for (size_t i = 0; i < sections; i++) {
		const char *tag = config_get_section(ini, i);
		const char *name = config_get_string(ini, tag, "Name");
		names.emplace_back(tag, name);
	}

	return names;
}

static void create_log_file(fstream &logFile)
{
	stringstream dst;

	get_last_log(false, "obs-studio/logs", lastLogFile);
#ifdef _WIN32
	get_last_log(true, "obs-studio/crashes", lastCrashLogFile);
#endif

	currentLogFile = GenerateTimeDateFilename("txt");
	dst << "obs-studio/logs/" << currentLogFile.c_str();

	BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	logFile.open(wpath, ios_base::in | ios_base::out | ios_base::trunc);
#else
	logFile.open(path, ios_base::in | ios_base::out | ios_base::trunc);
#endif

	if (logFile.is_open()) {
		delete_oldest_file(false, "obs-studio/logs");
		base_set_log_handler(do_log, &logFile);
	} else {
		blog(LOG_ERROR, "Failed to open log file");
	}
}

static auto ProfilerNameStoreRelease = [](profiler_name_store_t *store) {
	profiler_name_store_free(store);
};

using ProfilerNameStore = std::unique_ptr<profiler_name_store_t,
					  decltype(ProfilerNameStoreRelease)>;

ProfilerNameStore CreateNameStore()
{
	return ProfilerNameStore{profiler_name_store_create(),
				 ProfilerNameStoreRelease};
}

static auto SnapshotRelease = [](profiler_snapshot_t *snap) {
	profile_snapshot_free(snap);
};

using ProfilerSnapshot =
	std::unique_ptr<profiler_snapshot_t, decltype(SnapshotRelease)>;

ProfilerSnapshot GetSnapshot()
{
	return ProfilerSnapshot{profile_snapshot_create(), SnapshotRelease};
}

static void SaveProfilerData(const ProfilerSnapshot &snap)
{
	if (currentLogFile.empty())
		return;

	auto pos = currentLogFile.rfind('.');
	if (pos == currentLogFile.npos)
		return;

#define LITERAL_SIZE(x) x, (sizeof(x) - 1)
	ostringstream dst;
	dst.write(LITERAL_SIZE("obs-studio/profiler_data/"));
	dst.write(currentLogFile.c_str(), pos);
	dst.write(LITERAL_SIZE(".csv.gz"));
#undef LITERAL_SIZE

	BPtr<char> path = GetConfigPathPtr(dst.str().c_str());
	if (!profiler_snapshot_dump_csv_gz(snap.get(), path))
		blog(LOG_WARNING, "Could not save profiler data to '%s'",
		     static_cast<const char *>(path));
}

static auto ProfilerFree = [](void *) {
	profiler_stop();

	auto snap = GetSnapshot();

	profiler_print(snap.get());
	profiler_print_time_between_calls(snap.get());

	SaveProfilerData(snap);

	profiler_free();
};

QAccessibleInterface *accessibleFactory(const QString &classname,
					QObject *object)
{
	if (classname == QLatin1String("VolumeSlider") && object &&
	    object->isWidgetType())
		return new VolumeAccessibleInterface(
			static_cast<QWidget *>(object));

	return nullptr;
}

static const char *run_program_init = "run_program_init";
static int run_program(fstream &logFile, int argc, char *argv[])
{
	int ret = -1;

	auto profilerNameStore = CreateNameStore();

	std::unique_ptr<void, decltype(ProfilerFree)> prof_release(
		static_cast<void *>(&ProfilerFree), ProfilerFree);

	profiler_start();
	profile_register_root(run_program_init, 0);

	ScopeProfiler prof{run_program_init};

#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
	QGuiApplication::setAttribute(opt_disable_high_dpi_scaling
					      ? Qt::AA_DisableHighDpiScaling
					      : Qt::AA_EnableHighDpiScaling);
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)) && defined(_WIN32)
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
		Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

	QCoreApplication::addLibraryPath(".");

#if __APPLE__
	InstallNSApplicationSubclass();
#endif

	OBSApp program(argc, argv, profilerNameStore.get());
	try {
		QAccessible::installFactory(accessibleFactory);

		bool created_log = false;

		program.AppInit();
		delete_oldest_file(false, "obs-studio/profiler_data");

		OBSTranslator translator;
		program.installTranslator(&translator);

		/* --------------------------------------- */
		/* check and warn if already running       */

		bool cancel_launch = false;
		bool already_running = false;

#if defined(_WIN32)
		RunOnceMutex rom = GetRunOnceMutex(already_running);
#elif defined(__APPLE__)
		CheckAppWithSameBundleID(already_running);
#elif defined(__linux__)
		RunningInstanceCheck(already_running);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
		PIDFileCheck(already_running);
#endif

		if (!already_running) {
			goto run;
		}

		if (!multi) {
			QMessageBox::StandardButtons buttons(
				QMessageBox::Yes | QMessageBox::Cancel);
			QMessageBox mb(QMessageBox::Question,
				       QTStr("AlreadyRunning.Title"),
				       QTStr("AlreadyRunning.Text"), buttons,
				       nullptr);
			mb.setButtonText(QMessageBox::Yes,
					 QTStr("AlreadyRunning.LaunchAnyway"));
			mb.setButtonText(QMessageBox::Cancel, QTStr("Cancel"));
			mb.setDefaultButton(QMessageBox::Cancel);

			QMessageBox::StandardButton button;
			button = (QMessageBox::StandardButton)mb.exec();
			cancel_launch = button == QMessageBox::Cancel;
		}

		if (cancel_launch)
			return 0;

		if (!created_log) {
			create_log_file(logFile);
			created_log = true;
		}

		if (multi) {
			blog(LOG_INFO, "User enabled --multi flag and is now "
				       "running multiple instances of OBS.");
		} else {
			blog(LOG_WARNING, "================================");
			blog(LOG_WARNING, "Warning: OBS is already running!");
			blog(LOG_WARNING, "================================");
			blog(LOG_WARNING, "User is now running multiple "
					  "instances of OBS!");
		}

		/* --------------------------------------- */
	run:

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__FreeBSD__)
		// Mounted by termina during chromeOS linux container startup
		// https://chromium.googlesource.com/chromiumos/overlays/board-overlays/+/master/project-termina/chromeos-base/termina-lxd-scripts/files/lxd_setup.sh
		os_dir_t *crosDir = os_opendir("/opt/google/cros-containers");
		if (crosDir) {
			QMessageBox::StandardButtons buttons(QMessageBox::Ok);
			QMessageBox mb(QMessageBox::Critical,
				       QTStr("ChromeOS.Title"),
				       QTStr("ChromeOS.Text"), buttons,
				       nullptr);

			mb.exec();
			return 0;
		}
#endif

		if (!created_log) {
			create_log_file(logFile);
			created_log = true;
		}

		if (argc > 1) {
			stringstream stor;
			stor << argv[1];
			for (int i = 2; i < argc; ++i) {
				stor << " " << argv[i];
			}
			blog(LOG_INFO, "Command Line Arguments: %s",
			     stor.str().c_str());
		}

		if (!program.OBSInit())
			return 0;

		prof.Stop();

		ret = program.exec();

	} catch (const char *error) {
		blog(LOG_ERROR, "%s", error);
		OBSErrorBox(nullptr, "%s", error);
	}

	if (restart)
		QProcess::startDetached(qApp->arguments()[0],
					qApp->arguments());

	return ret;
}

#define MAX_CRASH_REPORT_SIZE (150 * 1024)

#ifdef _WIN32

#define CRASH_MESSAGE                                                      \
	"Woops, OBS has crashed!\n\nWould you like to copy the crash log " \
	"to the clipboard? The crash log will still be saved to:\n\n%s"

static void main_crash_handler(const char *format, va_list args, void *param)
{
	char *text = new char[MAX_CRASH_REPORT_SIZE];

	vsnprintf(text, MAX_CRASH_REPORT_SIZE, format, args);
	text[MAX_CRASH_REPORT_SIZE - 1] = 0;

	string crashFilePath = "obs-studio/crashes";

	delete_oldest_file(true, crashFilePath.c_str());

	string name = crashFilePath + "/";
	name += "Crash " + GenerateTimeDateFilename("txt");

	BPtr<char> path(GetConfigPathPtr(name.c_str()));

	fstream file;

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	file.open(wpath, ios_base::in | ios_base::out | ios_base::trunc |
				 ios_base::binary);
#else
	file.open(path, ios_base::in | ios_base::out | ios_base::trunc |
				ios_base::binary);
#endif
	file << text;
	file.close();

	string pathString(path.Get());

#ifdef _WIN32
	std::replace(pathString.begin(), pathString.end(), '/', '\\');
#endif

	string absolutePath =
		canonical(filesystem::path(pathString)).u8string();

	size_t size = snprintf(nullptr, 0, CRASH_MESSAGE, absolutePath.c_str());

	unique_ptr<char[]> message_buffer(new char[size + 1]);

	snprintf(message_buffer.get(), size + 1, CRASH_MESSAGE,
		 absolutePath.c_str());

	string finalMessage =
		string(message_buffer.get(), message_buffer.get() + size);

	int ret = MessageBoxA(NULL, finalMessage.c_str(), "OBS has crashed!",
			      MB_YESNO | MB_ICONERROR | MB_TASKMODAL);

	if (ret == IDYES) {
		size_t len = strlen(text);

		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len);
		memcpy(GlobalLock(mem), text, len);
		GlobalUnlock(mem);

		OpenClipboard(0);
		EmptyClipboard();
		SetClipboardData(CF_TEXT, mem);
		CloseClipboard();
	}

	exit(-1);

	UNUSED_PARAMETER(param);
}

static void load_debug_privilege(void)
{
	const DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
	TOKEN_PRIVILEGES tp;
	HANDLE token;
	LUID val;

	if (!OpenProcessToken(GetCurrentProcess(), flags, &token)) {
		return;
	}

	if (!!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &val)) {
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = val;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL,
				      NULL);
	}

	if (!!LookupPrivilegeValue(NULL, SE_INC_BASE_PRIORITY_NAME, &val)) {
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = val;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if (!AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL,
					   NULL)) {
			blog(LOG_INFO, "Could not set privilege to "
				       "increase GPU priority");
		}
	}

	CloseHandle(token);
}
#endif

#ifdef __APPLE__
#define BASE_PATH ".."
#else
#define BASE_PATH "../.."
#endif

#define CONFIG_PATH BASE_PATH "/config"

#ifndef OBS_UNIX_STRUCTURE
#define OBS_UNIX_STRUCTURE 0
#endif

int GetConfigPath(char *path, size_t size, const char *name)
{
	if (!OBS_UNIX_STRUCTURE && portable_mode) {
		if (name && *name) {
			return snprintf(path, size, CONFIG_PATH "/%s", name);
		} else {
			return snprintf(path, size, CONFIG_PATH);
		}
	} else {
		return os_get_config_path(path, size, name);
	}
}

char *GetConfigPathPtr(const char *name)
{
	if (!OBS_UNIX_STRUCTURE && portable_mode) {
		char path[512];

		if (snprintf(path, sizeof(path), CONFIG_PATH "/%s", name) > 0) {
			return bstrdup(path);
		} else {
			return NULL;
		}
	} else {
		return os_get_config_path_ptr(name);
	}
}

int GetProgramDataPath(char *path, size_t size, const char *name)
{
	return os_get_program_data_path(path, size, name);
}

char *GetProgramDataPathPtr(const char *name)
{
	return os_get_program_data_path_ptr(name);
}

bool GetFileSafeName(const char *name, std::string &file)
{
	size_t base_len = strlen(name);
	size_t len = os_utf8_to_wcs(name, base_len, nullptr, 0);
	std::wstring wfile;

	if (!len)
		return false;

	wfile.resize(len);
	os_utf8_to_wcs(name, base_len, &wfile[0], len + 1);

	for (size_t i = wfile.size(); i > 0; i--) {
		size_t im1 = i - 1;

		if (iswspace(wfile[im1])) {
			wfile[im1] = '_';
		} else if (wfile[im1] != '_' && !iswalnum(wfile[im1])) {
			wfile.erase(im1, 1);
		}
	}

	if (wfile.size() == 0)
		wfile = L"characters_only";

	len = os_wcs_to_utf8(wfile.c_str(), wfile.size(), nullptr, 0);
	if (!len)
		return false;

	file.resize(len);
	os_wcs_to_utf8(wfile.c_str(), wfile.size(), &file[0], len + 1);
	return true;
}

bool GetClosestUnusedFileName(std::string &path, const char *extension)
{
	size_t len = path.size();
	if (extension) {
		path += ".";
		path += extension;
	}

	if (!os_file_exists(path.c_str()))
		return true;

	int index = 1;

	do {
		path.resize(len);
		path += std::to_string(++index);
		if (extension) {
			path += ".";
			path += extension;
		}
	} while (os_file_exists(path.c_str()));

	return true;
}

bool GetUnusedSceneCollectionFile(std::string &name, std::string &file)
{
	char path[512];
	int ret;

	if (!GetFileSafeName(name.c_str(), file)) {
		blog(LOG_WARNING, "Failed to create safe file name for '%s'",
		     name.c_str());
		return false;
	}

	ret = GetConfigPath(path, sizeof(path), "obs-studio/basic/scenes/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get scene collection config path");
		return false;
	}

	file.insert(0, path);

	if (!GetClosestUnusedFileName(file, "json")) {
		blog(LOG_WARNING, "Failed to get closest file name for %s",
		     file.c_str());
		return false;
	}

	file.erase(file.size() - 5, 5);
	file.erase(0, strlen(path));
	return true;
}

bool WindowPositionValid(QRect rect)
{
	for (QScreen *screen : QGuiApplication::screens()) {
		if (screen->availableGeometry().intersects(rect))
			return true;
	}
	return false;
}

static inline bool arg_is(const char *arg, const char *long_form,
			  const char *short_form)
{
	return (long_form && strcmp(arg, long_form) == 0) ||
	       (short_form && strcmp(arg, short_form) == 0);
}

#if !defined(_WIN32) && !defined(__APPLE__)
#define IS_UNIX 1
#endif

/* if using XDG and was previously using an older build of OBS, move config
 * files to XDG directory */
#if defined(USE_XDG) && defined(IS_UNIX)
static void move_to_xdg(void)
{
	char old_path[512];
	char new_path[512];
	char *home = getenv("HOME");
	if (!home)
		return;

	if (snprintf(old_path, 512, "%s/.obs-studio", home) <= 0)
		return;

	/* make base xdg path if it doesn't already exist */
	if (GetConfigPath(new_path, 512, "") <= 0)
		return;
	if (os_mkdirs(new_path) == MKDIR_ERROR)
		return;

	if (GetConfigPath(new_path, 512, "obs-studio") <= 0)
		return;

	if (os_file_exists(old_path) && !os_file_exists(new_path)) {
		rename(old_path, new_path);
	}
}
#endif

static bool update_ffmpeg_output(ConfigFile &config)
{
	if (config_has_user_value(config, "AdvOut", "FFOutputToFile"))
		return false;

	const char *url = config_get_string(config, "AdvOut", "FFURL");
	if (!url)
		return false;

	bool isActualURL = strstr(url, "://") != nullptr;
	if (isActualURL)
		return false;

	string urlStr = url;
	string extension;

	for (size_t i = urlStr.length(); i > 0; i--) {
		size_t idx = i - 1;

		if (urlStr[idx] == '.') {
			extension = &urlStr[i];
		}

		if (urlStr[idx] == '\\' || urlStr[idx] == '/') {
			urlStr[idx] = 0;
			break;
		}
	}

	if (urlStr.empty() || extension.empty())
		return false;

	config_remove_value(config, "AdvOut", "FFURL");
	config_set_string(config, "AdvOut", "FFFilePath", urlStr.c_str());
	config_set_string(config, "AdvOut", "FFExtension", extension.c_str());
	config_set_bool(config, "AdvOut", "FFOutputToFile", true);
	return true;
}

static bool move_reconnect_settings(ConfigFile &config, const char *sec)
{
	bool changed = false;

	if (config_has_user_value(config, sec, "Reconnect")) {
		bool reconnect = config_get_bool(config, sec, "Reconnect");
		config_set_bool(config, "Output", "Reconnect", reconnect);
		changed = true;
	}
	if (config_has_user_value(config, sec, "RetryDelay")) {
		int delay = (int)config_get_uint(config, sec, "RetryDelay");
		config_set_uint(config, "Output", "RetryDelay", delay);
		changed = true;
	}
	if (config_has_user_value(config, sec, "MaxRetries")) {
		int retries = (int)config_get_uint(config, sec, "MaxRetries");
		config_set_uint(config, "Output", "MaxRetries", retries);
		changed = true;
	}

	return changed;
}

static bool update_reconnect(ConfigFile &config)
{
	if (!config_has_user_value(config, "Output", "Mode"))
		return false;

	const char *mode = config_get_string(config, "Output", "Mode");
	if (!mode)
		return false;

	const char *section = (strcmp(mode, "Advanced") == 0) ? "AdvOut"
							      : "SimpleOutput";

	if (move_reconnect_settings(config, section)) {
		config_remove_value(config, "SimpleOutput", "Reconnect");
		config_remove_value(config, "SimpleOutput", "RetryDelay");
		config_remove_value(config, "SimpleOutput", "MaxRetries");
		config_remove_value(config, "AdvOut", "Reconnect");
		config_remove_value(config, "AdvOut", "RetryDelay");
		config_remove_value(config, "AdvOut", "MaxRetries");
		return true;
	}

	return false;
}

static void convert_x264_settings(obs_data_t *data)
{
	bool use_bufsize = obs_data_get_bool(data, "use_bufsize");

	if (use_bufsize) {
		int buffer_size = (int)obs_data_get_int(data, "buffer_size");
		if (buffer_size == 0)
			obs_data_set_string(data, "rate_control", "CRF");
	}
}

static void convert_14_2_encoder_setting(const char *encoder, const char *file)
{
	obs_data_t *data = obs_data_create_from_json_file_safe(file, "bak");
	obs_data_item_t *cbr_item = obs_data_item_byname(data, "cbr");
	obs_data_item_t *rc_item = obs_data_item_byname(data, "rate_control");
	bool modified = false;
	bool cbr = true;

	if (cbr_item) {
		cbr = obs_data_item_get_bool(cbr_item);
		obs_data_item_unset_user_value(cbr_item);

		obs_data_set_string(data, "rate_control", cbr ? "CBR" : "VBR");

		modified = true;
	}

	if (!rc_item && astrcmpi(encoder, "obs_x264") == 0) {
		if (!cbr_item)
			obs_data_set_string(data, "rate_control", "CBR");
		else if (!cbr)
			convert_x264_settings(data);

		modified = true;
	}

	if (modified)
		obs_data_save_json_safe(data, file, "tmp", "bak");

	obs_data_item_release(&rc_item);
	obs_data_item_release(&cbr_item);
	obs_data_release(data);
}

static void upgrade_settings(void)
{
	char path[512];
	int pathlen = GetConfigPath(path, 512, "obs-studio/basic/profiles");

	if (pathlen <= 0)
		return;
	if (!os_file_exists(path))
		return;

	os_dir_t *dir = os_opendir(path);
	if (!dir)
		return;

	struct os_dirent *ent = os_readdir(dir);

	while (ent) {
		if (ent->directory && strcmp(ent->d_name, ".") != 0 &&
		    strcmp(ent->d_name, "..") != 0) {
			strcat(path, "/");
			strcat(path, ent->d_name);
			strcat(path, "/basic.ini");

			ConfigFile config;
			int ret;

			ret = config.Open(path, CONFIG_OPEN_EXISTING);
			if (ret == CONFIG_SUCCESS) {
				if (update_ffmpeg_output(config) ||
				    update_reconnect(config)) {
					config_save_safe(config, "tmp",
							 nullptr);
				}
			}

			if (config) {
				const char *sEnc = config_get_string(
					config, "AdvOut", "Encoder");
				const char *rEnc = config_get_string(
					config, "AdvOut", "RecEncoder");

				/* replace "cbr" option with "rate_control" for
				 * each profile's encoder data */
				path[pathlen] = 0;
				strcat(path, "/");
				strcat(path, ent->d_name);
				strcat(path, "/recordEncoder.json");
				convert_14_2_encoder_setting(rEnc, path);

				path[pathlen] = 0;
				strcat(path, "/");
				strcat(path, ent->d_name);
				strcat(path, "/streamEncoder.json");
				convert_14_2_encoder_setting(sEnc, path);
			}

			path[pathlen] = 0;
		}

		ent = os_readdir(dir);
	}

	os_closedir(dir);
}

void ctrlc_handler(int s)
{
	UNUSED_PARAMETER(s);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	main->close();
}

int main(int argc, char *argv[])
{
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);

	struct sigaction sig_handler;

	sig_handler.sa_handler = ctrlc_handler;
	sigemptyset(&sig_handler.sa_mask);
	sig_handler.sa_flags = 0;

	sigaction(SIGINT, &sig_handler, NULL);

	/* Block SIGPIPE in all threads, this can happen if a thread calls write on
	a closed pipe. */
	sigset_t sigpipe_mask;
	sigemptyset(&sigpipe_mask);
	sigaddset(&sigpipe_mask, SIGPIPE);
	sigset_t saved_mask;
	if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
		perror("pthread_sigmask");
		exit(1);
	}
#endif

#ifdef _WIN32
	obs_init_win32_crash_handler();
	SetErrorMode(SEM_FAILCRITICALERRORS);
	load_debug_privilege();
	base_set_crash_handler(main_crash_handler, nullptr);
#endif

	base_get_log_handler(&def_log_handler, nullptr);

#if defined(USE_XDG) && defined(IS_UNIX)
	move_to_xdg();
#endif

	obs_set_cmdline_args(argc, argv);

	for (int i = 1; i < argc; i++) {
		if (arg_is(argv[i], "--portable", "-p")) {
			portable_mode = true;

		} else if (arg_is(argv[i], "--multi", "-m")) {
			multi = true;

		} else if (arg_is(argv[i], "--verbose", nullptr)) {
			log_verbose = true;

		} else if (arg_is(argv[i], "--always-on-top", nullptr)) {
			opt_always_on_top = true;

		} else if (arg_is(argv[i], "--unfiltered_log", nullptr)) {
			unfiltered_log = true;

		} else if (arg_is(argv[i], "--startstreaming", nullptr)) {
			opt_start_streaming = true;

		} else if (arg_is(argv[i], "--startrecording", nullptr)) {
			opt_start_recording = true;

		} else if (arg_is(argv[i], "--startreplaybuffer", nullptr)) {
			opt_start_replaybuffer = true;

		} else if (arg_is(argv[i], "--startvirtualcam", nullptr)) {
			opt_start_virtualcam = true;

		} else if (arg_is(argv[i], "--collection", nullptr)) {
			if (++i < argc)
				opt_starting_collection = argv[i];

		} else if (arg_is(argv[i], "--profile", nullptr)) {
			if (++i < argc)
				opt_starting_profile = argv[i];

		} else if (arg_is(argv[i], "--scene", nullptr)) {
			if (++i < argc)
				opt_starting_scene = argv[i];

		} else if (arg_is(argv[i], "--minimize-to-tray", nullptr)) {
			opt_minimize_tray = true;

		} else if (arg_is(argv[i], "--studio-mode", nullptr)) {
			opt_studio_mode = true;

		} else if (arg_is(argv[i], "--allow-opengl", nullptr)) {
			opt_allow_opengl = true;

		} else if (arg_is(argv[i], "--disable-updater", nullptr)) {
			opt_disable_updater = true;

		} else if (arg_is(argv[i], "--disable-high-dpi-scaling",
				  nullptr)) {
			opt_disable_high_dpi_scaling = true;

		} else if (arg_is(argv[i], "--help", "-h")) {
			std::string help =
				"--help, -h: Get list of available commands.\n\n"
				"--startstreaming: Automatically start streaming.\n"
				"--startrecording: Automatically start recording.\n"
				"--startreplaybuffer: Start replay buffer.\n"
				"--startvirtualcam: Start virtual camera (if available).\n\n"
				"--collection <string>: Use specific scene collection."
				"\n"
				"--profile <string>: Use specific profile.\n"
				"--scene <string>: Start with specific scene.\n\n"
				"--studio-mode: Enable studio mode.\n"
				"--minimize-to-tray: Minimize to system tray.\n"
				"--portable, -p: Use portable mode.\n"
				"--multi, -m: Don't warn when launching multiple instances.\n\n"
				"--verbose: Make log more verbose.\n"
				"--always-on-top: Start in 'always on top' mode.\n\n"
				"--unfiltered_log: Make log unfiltered.\n\n"
				"--disable-updater: Disable built-in updater (Windows/Mac only)\n\n"
				"--disable-high-dpi-scaling: Disable automatic high-DPI scaling\n\n";

#ifdef _WIN32
			MessageBoxA(NULL, help.c_str(), "Help",
				    MB_OK | MB_ICONASTERISK);
#else
			std::cout << help
				  << "--version, -V: Get current version.\n";
#endif
			exit(0);

		} else if (arg_is(argv[i], "--version", "-V")) {
			std::cout << "OBS Studio - "
				  << App()->GetVersionString() << "\n";
			exit(0);
		}
	}

#if !OBS_UNIX_STRUCTURE
	if (!portable_mode) {
		portable_mode =
			os_file_exists(BASE_PATH "/portable_mode") ||
			os_file_exists(BASE_PATH "/obs_portable_mode") ||
			os_file_exists(BASE_PATH "/portable_mode.txt") ||
			os_file_exists(BASE_PATH "/obs_portable_mode.txt");
	}

	if (!opt_disable_updater) {
		opt_disable_updater =
			os_file_exists(BASE_PATH "/disable_updater") ||
			os_file_exists(BASE_PATH "/disable_updater.txt");
	}
#endif

	upgrade_settings();

	fstream logFile;

	curl_global_init(CURL_GLOBAL_ALL);
	int ret = run_program(logFile, argc, argv);

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());
	base_set_log_handler(nullptr, nullptr);
	return ret;
}
