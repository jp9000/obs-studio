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

#pragma once

#include <QApplication>
#include <QTranslator>
#include <QPointer>
#include <obs.hpp>
#include <util/lexer.h>
#include <util/profiler.h>
#include <util/util.hpp>
#include <util/platform.h>
#include <obs-frontend-api.h>
#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <deque>

#include "window-main.hpp"

std::string CurrentTimeString();
std::string CurrentDateTimeString();
std::string GenerateTimeDateFilename(const char *extension,
				     bool noSpace = false);
std::string GenerateSpecifiedFilename(const char *extension, bool noSpace,
				      const char *format);
std::string GetFormatString(const char *format, const char *prefix,
			    const char *suffix);
std::string GetOutputFilename(const char *path, const char *ext, bool noSpace,
			      bool overwrite, const char *format);
QObject *CreateShortcutFilter();

struct BaseLexer {
	lexer lex;

public:
	inline BaseLexer() { lexer_init(&lex); }
	inline ~BaseLexer() { lexer_free(&lex); }
	operator lexer *() { return &lex; }
};

class OBSTranslator : public QTranslator {
	Q_OBJECT

public:
	virtual bool isEmpty() const override { return false; }

	virtual QString translate(const char *context, const char *sourceText,
				  const char *disambiguation,
				  int n) const override;
};

typedef std::function<void()> VoidFunc;

class OBSApp : public QApplication {
	Q_OBJECT

private:
	std::string locale;
	std::string theme;
	ConfigFile globalConfig;
	TextLookup textLookup;
	QPointer<OBSMainWindow> mainWindow;
	profiler_name_store_t *profilerNameStore = nullptr;

	bool libobs_initialized = false;

	os_inhibit_t *sleepInhibitor = nullptr;
	int sleepInhibitRefs = 0;

	bool enableHotkeysInFocus = true;
	bool enableHotkeysOutOfFocus = true;

	std::deque<obs_frontend_translate_ui_cb> translatorHooks;

	bool UpdatePre22MultiviewLayout(const char *layout);

	bool InitGlobalConfig();
	bool InitGlobalConfigDefaults();
	bool InitLocale();
	bool InitTheme();

	inline void ResetHotkeyState(bool inFocus);

	QPalette defaultPalette;

	void ParseExtraThemeData(const char *path);
	void AddExtraThemeColor(QPalette &pal, int group, const char *name,
				uint32_t color);

public:
	OBSApp(int &argc, char **argv, profiler_name_store_t *store);
	~OBSApp();

	void AppInit();
	bool OBSInit();

	void UpdateHotkeyFocusSetting(bool reset = true);
	void DisableHotkeys();

	inline bool HotkeysEnabledInFocus() const
	{
		return enableHotkeysInFocus;
	}

	inline QMainWindow *GetMainWindow() const { return mainWindow.data(); }

	inline config_t *GlobalConfig() const { return globalConfig; }

	inline const char *GetLocale() const { return locale.c_str(); }

	inline const char *GetTheme() const { return theme.c_str(); }
	bool SetTheme(std::string name, std::string path = "");

	inline lookup_t *GetTextLookup() const { return textLookup; }

	inline const char *GetString(const char *lookupVal) const
	{
		return textLookup.GetString(lookupVal);
	}

	bool TranslateString(const char *lookupVal, const char **out) const;

	profiler_name_store_t *GetProfilerNameStore() const
	{
		return profilerNameStore;
	}

	const char *GetLastLog() const;
	const char *GetCurrentLog() const;

	const char *GetLastCrashLog() const;

	std::string GetVersionString() const;
	bool IsPortableMode();
	bool IsUpdaterDisabled();

	const char *InputAudioSource() const;
	const char *OutputAudioSource() const;

	const char *GetRenderModule() const;

	inline void IncrementSleepInhibition()
	{
		if (!sleepInhibitor)
			return;
		if (sleepInhibitRefs++ == 0)
			os_inhibit_sleep_set_active(sleepInhibitor, true);
	}

	inline void DecrementSleepInhibition()
	{
		if (!sleepInhibitor)
			return;
		if (sleepInhibitRefs == 0)
			return;
		if (--sleepInhibitRefs == 0)
			os_inhibit_sleep_set_active(sleepInhibitor, false);
	}

	inline void PushUITranslation(obs_frontend_translate_ui_cb cb)
	{
		translatorHooks.emplace_front(cb);
	}

	inline void PopUITranslation() { translatorHooks.pop_front(); }

public slots:
	void Exec(VoidFunc func);

signals:
	void StyleChanged();
};

int GetConfigPath(char *path, size_t size, const char *name);
char *GetConfigPathPtr(const char *name);

int GetProgramDataPath(char *path, size_t size, const char *name);
char *GetProgramDataPathPtr(const char *name);

inline OBSApp *App()
{
	return static_cast<OBSApp *>(qApp);
}

inline config_t *GetGlobalConfig()
{
	return App()->GlobalConfig();
}

std::vector<std::pair<std::string, std::string>> GetLocaleNames();
inline const char *Str(const char *lookup)
{
	return App()->GetString(lookup);
}
#define QTStr(lookupVal) QString::fromUtf8(Str(lookupVal))

bool GetFileSafeName(const char *name, std::string &file);
bool GetClosestUnusedFileName(std::string &path, const char *extension);
bool GetUnusedSceneCollectionFile(std::string &name, std::string &file);

bool WindowPositionValid(QRect rect);

static inline int GetProfilePath(char *path, size_t size, const char *file)
{
	OBSMainWindow *window =
		reinterpret_cast<OBSMainWindow *>(App()->GetMainWindow());
	return window->GetProfilePath(path, size, file);
}

extern bool portable_mode;

extern bool opt_start_streaming;
extern bool opt_start_recording;
extern bool opt_start_replaybuffer;
extern bool opt_start_virtualcam;
extern bool opt_minimize_tray;
extern bool opt_studio_mode;
extern bool opt_allow_opengl;
extern bool opt_always_on_top;
extern bool opt_disable_high_dpi_scaling;
extern std::string opt_starting_scene;
extern bool restart;
