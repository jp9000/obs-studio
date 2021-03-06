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

#include "util/windows/win-registry.h"
#include "util/windows/win-version.h"
#include "util/platform.h"
#include "util/dstr.h"
#include "obs.h"
#include "obs-internal.h"

#include <windows.h>
#include <wscapi.h>
#include <iwscapi.h>

static uint32_t win_ver = 0;

const char *get_module_extension(void)
{
	return ".dll";
}

#ifdef _WIN64
#define BIT_STRING "64bit"
#else
#define BIT_STRING "32bit"
#endif

static const char *module_bin[] = {
	"../../obs-plugins/" BIT_STRING,
};

static const char *module_data[] = {"../../data/obs-plugins/%module%"};

static const int module_patterns_size =
	sizeof(module_bin) / sizeof(module_bin[0]);

void add_default_module_paths(void)
{
	for (int i = 0; i < module_patterns_size; i++)
		obs_add_module_path(module_bin[i], module_data[i]);
}

/* on windows, points to [base directory]/data/libobs */
char *find_libobs_data_file(const char *file)
{
	struct dstr path;
	dstr_init(&path);

	if (check_path(file, "../../data/libobs/", &path))
		return path.array;

	dstr_free(&path);
	return NULL;
}

static void log_processor_info(void)
{
	HKEY key;
	wchar_t data[1024];
	char *str = NULL;
	DWORD size, speed;
	LSTATUS status;

	memset(data, 0, sizeof(data));

	status = RegOpenKeyW(
		HKEY_LOCAL_MACHINE,
		L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", &key);
	if (status != ERROR_SUCCESS)
		return;

	size = sizeof(data);
	status = RegQueryValueExW(key, L"ProcessorNameString", NULL, NULL,
				  (LPBYTE)data, &size);
	if (status == ERROR_SUCCESS) {
		os_wcs_to_utf8_ptr(data, 0, &str);
		blog(LOG_INFO, "CPU Name: %s", str);
		bfree(str);
	}

	size = sizeof(speed);
	status = RegQueryValueExW(key, L"~MHz", NULL, NULL, (LPBYTE)&speed,
				  &size);
	if (status == ERROR_SUCCESS)
		blog(LOG_INFO, "CPU Speed: %ldMHz", speed);

	RegCloseKey(key);
}

static void log_processor_cores(void)
{
	blog(LOG_INFO, "Physical Cores: %d, Logical Cores: %d",
	     os_get_physical_cores(), os_get_logical_cores());
}

static void log_available_memory(void)
{
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);

	GlobalMemoryStatusEx(&ms);

#ifdef _WIN64
	const char *note = "";
#else
	const char *note = " (NOTE: 32bit programs cannot use more than 3gb)";
#endif

	blog(LOG_INFO, "Physical Memory: %luMB Total, %luMB Free%s",
	     (DWORD)(ms.ullTotalPhys / 1048576),
	     (DWORD)(ms.ullAvailPhys / 1048576), note);
}

extern const char *get_win_release_id();

static void log_windows_version(void)
{
	struct win_version_info ver;
	get_win_ver(&ver);

	const char *release_id = get_win_release_id();

	bool b64 = is_64_bit_windows();
	const char *windows_bitness = b64 ? "64" : "32";

	blog(LOG_INFO,
	     "Windows Version: %d.%d Build %d (release: %s; revision: %d; %s-bit)",
	     ver.major, ver.minor, ver.build, release_id, ver.revis,
	     windows_bitness);
}

static void log_admin_status(void)
{
	SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
	PSID admin_group;
	BOOL success;

	success = AllocateAndInitializeSid(&auth, 2,
					   SECURITY_BUILTIN_DOMAIN_RID,
					   DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0,
					   0, 0, &admin_group);
	if (success) {
		if (!CheckTokenMembership(NULL, admin_group, &success))
			success = false;
		FreeSid(admin_group);
	}

	blog(LOG_INFO, "Running as administrator: %s",
	     success ? "true" : "false");
}

typedef HRESULT(WINAPI *dwm_is_composition_enabled_t)(BOOL *);

static void log_aero(void)
{
	dwm_is_composition_enabled_t composition_enabled = NULL;

	const char *aeroMessage =
		win_ver >= 0x602
			? " (Aero is always on for windows 8 and above)"
			: "";

	HMODULE dwm = LoadLibraryW(L"dwmapi");
	BOOL bComposition = true;

	if (!dwm) {
		return;
	}

	composition_enabled = (dwm_is_composition_enabled_t)GetProcAddress(
		dwm, "DwmIsCompositionEnabled");
	if (!composition_enabled) {
		FreeLibrary(dwm);
		return;
	}

	composition_enabled(&bComposition);
	blog(LOG_INFO, "Aero is %s%s", bComposition ? "Enabled" : "Disabled",
	     aeroMessage);
}

#define WIN10_GAME_BAR_REG_KEY \
	L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR"
#define WIN10_GAME_DVR_POLICY_REG_KEY \
	L"SOFTWARE\\Policies\\Microsoft\\Windows\\GameDVR"
#define WIN10_GAME_DVR_REG_KEY L"System\\GameConfigStore"
#define WIN10_GAME_MODE_REG_KEY L"Software\\Microsoft\\GameBar"
#define WIN10_HAGS_REG_KEY \
	L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers"

static void log_gaming_features(void)
{
	if (win_ver < 0xA00)
		return;

	struct reg_dword game_bar_enabled;
	struct reg_dword game_dvr_allowed;
	struct reg_dword game_dvr_enabled;
	struct reg_dword game_dvr_bg_recording;
	struct reg_dword game_mode_enabled;
	struct reg_dword hags_enabled;

	get_reg_dword(HKEY_CURRENT_USER, WIN10_GAME_BAR_REG_KEY,
		      L"AppCaptureEnabled", &game_bar_enabled);
	get_reg_dword(HKEY_CURRENT_USER, WIN10_GAME_DVR_POLICY_REG_KEY,
		      L"AllowGameDVR", &game_dvr_allowed);
	get_reg_dword(HKEY_CURRENT_USER, WIN10_GAME_DVR_REG_KEY,
		      L"GameDVR_Enabled", &game_dvr_enabled);
	get_reg_dword(HKEY_CURRENT_USER, WIN10_GAME_BAR_REG_KEY,
		      L"HistoricalCaptureEnabled", &game_dvr_bg_recording);
	get_reg_dword(HKEY_CURRENT_USER, WIN10_GAME_MODE_REG_KEY,
		      L"AllowAutoGameMode", &game_mode_enabled);
	get_reg_dword(HKEY_LOCAL_MACHINE, WIN10_HAGS_REG_KEY, L"HwSchMode",
		      &hags_enabled);

	if (game_mode_enabled.status != ERROR_SUCCESS) {
		get_reg_dword(HKEY_CURRENT_USER, WIN10_GAME_MODE_REG_KEY,
			      L"AutoGameModeEnabled", &game_mode_enabled);
	}

	blog(LOG_INFO, "Windows 10 Gaming Features:");
	if (game_bar_enabled.status == ERROR_SUCCESS) {
		blog(LOG_INFO, "\tGame Bar: %s",
		     (bool)game_bar_enabled.return_value ? "On" : "Off");
	}

	if (game_dvr_allowed.status == ERROR_SUCCESS) {
		blog(LOG_INFO, "\tGame DVR Allowed: %s",
		     (bool)game_dvr_allowed.return_value ? "Yes" : "No");
	}

	if (game_dvr_enabled.status == ERROR_SUCCESS) {
		blog(LOG_INFO, "\tGame DVR: %s",
		     (bool)game_dvr_enabled.return_value ? "On" : "Off");
	}

	if (game_dvr_bg_recording.status == ERROR_SUCCESS) {
		blog(LOG_INFO, "\tGame DVR Background Recording: %s",
		     (bool)game_dvr_bg_recording.return_value ? "On" : "Off");
	}

	if (game_mode_enabled.status == ERROR_SUCCESS) {
		blog(LOG_INFO, "\tGame Mode: %s",
		     (bool)game_mode_enabled.return_value ? "On" : "Off");
	}

	if (hags_enabled.status == ERROR_SUCCESS) {
		blog(LOG_INFO, "\tHardware GPU Scheduler: %s",
		     (hags_enabled.return_value == 2) ? "On" : "Off");
	}
}

static const char *get_str_for_state(int state)
{
	switch (state) {
	case WSC_SECURITY_PRODUCT_STATE_ON:
		return "enabled";
	case WSC_SECURITY_PRODUCT_STATE_OFF:
		return "disabled";
	case WSC_SECURITY_PRODUCT_STATE_SNOOZED:
		return "temporarily disabled";
	case WSC_SECURITY_PRODUCT_STATE_EXPIRED:
		return "expired";
	default:
		return "unknown";
	}
}

static const char *get_str_for_type(int type)
{
	switch (type) {
	case WSC_SECURITY_PROVIDER_ANTIVIRUS:
		return "AV";
	case WSC_SECURITY_PROVIDER_FIREWALL:
		return "FW";
	case WSC_SECURITY_PROVIDER_ANTISPYWARE:
		return "ASW";
	default:
		return "unknown";
	}
}

static void log_security_products_by_type(IWSCProductList *prod_list, int type)
{
	HRESULT hr;
	LONG count = 0;
	IWscProduct *prod;
	BSTR name;
	WSC_SECURITY_PRODUCT_STATE prod_state;

	hr = prod_list->lpVtbl->Initialize(prod_list, type);

	if (FAILED(hr))
		return;

	hr = prod_list->lpVtbl->get_Count(prod_list, &count);
	if (FAILED(hr)) {
		prod_list->lpVtbl->Release(prod_list);
		return;
	}

	for (int i = 0; i < count; i++) {
		hr = prod_list->lpVtbl->get_Item(prod_list, i, &prod);
		if (FAILED(hr))
			continue;

		hr = prod->lpVtbl->get_ProductName(prod, &name);
		if (FAILED(hr))
			continue;

		hr = prod->lpVtbl->get_ProductState(prod, &prod_state);
		if (FAILED(hr)) {
			SysFreeString(name);
			continue;
		}

		blog(LOG_INFO, "\t%S: %s (%s)", name,
		     get_str_for_state(prod_state), get_str_for_type(type));

		SysFreeString(name);
		prod->lpVtbl->Release(prod);
	}

	prod_list->lpVtbl->Release(prod_list);
}

static void log_security_products(void)
{
	IWSCProductList *prod_list = NULL;
	HMODULE h_wsc;
	HRESULT hr;

	/* We load the DLL rather than import wcsapi.lib because the clsid /
	 * iid only exists on Windows 8 or higher. */

	h_wsc = LoadLibraryW(L"wscapi.dll");
	if (!h_wsc)
		return;

	const CLSID *prod_list_clsid =
		(const CLSID *)GetProcAddress(h_wsc, "CLSID_WSCProductList");
	const IID *prod_list_iid =
		(const IID *)GetProcAddress(h_wsc, "IID_IWSCProductList");

	if (prod_list_clsid && prod_list_iid) {
		blog(LOG_INFO, "Sec. Software Status:");

		hr = CoCreateInstance(prod_list_clsid, NULL,
				      CLSCTX_INPROC_SERVER, prod_list_iid,
				      &prod_list);
		if (!FAILED(hr)) {
			log_security_products_by_type(
				prod_list, WSC_SECURITY_PROVIDER_ANTIVIRUS);
		}

		hr = CoCreateInstance(prod_list_clsid, NULL,
				      CLSCTX_INPROC_SERVER, prod_list_iid,
				      &prod_list);
		if (!FAILED(hr)) {
			log_security_products_by_type(
				prod_list, WSC_SECURITY_PROVIDER_FIREWALL);
		}

		hr = CoCreateInstance(prod_list_clsid, NULL,
				      CLSCTX_INPROC_SERVER, prod_list_iid,
				      &prod_list);
		if (!FAILED(hr)) {
			log_security_products_by_type(
				prod_list, WSC_SECURITY_PROVIDER_ANTISPYWARE);
		}
	}

	FreeLibrary(h_wsc);
}

void log_system_info(void)
{
	struct win_version_info ver;
	get_win_ver(&ver);

	win_ver = (ver.major << 8) | ver.minor;

	log_processor_info();
	log_processor_cores();
	log_available_memory();
	log_windows_version();
	log_admin_status();
	log_aero();
	log_gaming_features();
	log_security_products();
}

struct obs_hotkeys_platform {
	int vk_codes[OBS_KEY_LAST_VALUE];
};

static int get_virtual_key(obs_key_t key)
{
	switch (key) {
	case OBS_KEY_RETURN:
		return VK_RETURN;
	case OBS_KEY_ESCAPE:
		return VK_ESCAPE;
	case OBS_KEY_TAB:
		return VK_TAB;
	case OBS_KEY_BACKTAB:
		return VK_OEM_BACKTAB;
	case OBS_KEY_BACKSPACE:
		return VK_BACK;
	case OBS_KEY_INSERT:
		return VK_INSERT;
	case OBS_KEY_DELETE:
		return VK_DELETE;
	case OBS_KEY_PAUSE:
		return VK_PAUSE;
	case OBS_KEY_PRINT:
		return VK_SNAPSHOT;
	case OBS_KEY_CLEAR:
		return VK_CLEAR;
	case OBS_KEY_HOME:
		return VK_HOME;
	case OBS_KEY_END:
		return VK_END;
	case OBS_KEY_LEFT:
		return VK_LEFT;
	case OBS_KEY_UP:
		return VK_UP;
	case OBS_KEY_RIGHT:
		return VK_RIGHT;
	case OBS_KEY_DOWN:
		return VK_DOWN;
	case OBS_KEY_PAGEUP:
		return VK_PRIOR;
	case OBS_KEY_PAGEDOWN:
		return VK_NEXT;

	case OBS_KEY_SHIFT:
		return VK_SHIFT;
	case OBS_KEY_CONTROL:
		return VK_CONTROL;
	case OBS_KEY_ALT:
		return VK_MENU;
	case OBS_KEY_CAPSLOCK:
		return VK_CAPITAL;
	case OBS_KEY_NUMLOCK:
		return VK_NUMLOCK;
	case OBS_KEY_SCROLLLOCK:
		return VK_SCROLL;

	case OBS_KEY_F1:
		return VK_F1;
	case OBS_KEY_F2:
		return VK_F2;
	case OBS_KEY_F3:
		return VK_F3;
	case OBS_KEY_F4:
		return VK_F4;
	case OBS_KEY_F5:
		return VK_F5;
	case OBS_KEY_F6:
		return VK_F6;
	case OBS_KEY_F7:
		return VK_F7;
	case OBS_KEY_F8:
		return VK_F8;
	case OBS_KEY_F9:
		return VK_F9;
	case OBS_KEY_F10:
		return VK_F10;
	case OBS_KEY_F11:
		return VK_F11;
	case OBS_KEY_F12:
		return VK_F12;
	case OBS_KEY_F13:
		return VK_F13;
	case OBS_KEY_F14:
		return VK_F14;
	case OBS_KEY_F15:
		return VK_F15;
	case OBS_KEY_F16:
		return VK_F16;
	case OBS_KEY_F17:
		return VK_F17;
	case OBS_KEY_F18:
		return VK_F18;
	case OBS_KEY_F19:
		return VK_F19;
	case OBS_KEY_F20:
		return VK_F20;
	case OBS_KEY_F21:
		return VK_F21;
	case OBS_KEY_F22:
		return VK_F22;
	case OBS_KEY_F23:
		return VK_F23;
	case OBS_KEY_F24:
		return VK_F24;

	case OBS_KEY_SPACE:
		return VK_SPACE;

	case OBS_KEY_APOSTROPHE:
		return VK_OEM_7;
	case OBS_KEY_PLUS:
		return VK_OEM_PLUS;
	case OBS_KEY_COMMA:
		return VK_OEM_COMMA;
	case OBS_KEY_MINUS:
		return VK_OEM_MINUS;
	case OBS_KEY_PERIOD:
		return VK_OEM_PERIOD;
	case OBS_KEY_SLASH:
		return VK_OEM_2;
	case OBS_KEY_0:
		return '0';
	case OBS_KEY_1:
		return '1';
	case OBS_KEY_2:
		return '2';
	case OBS_KEY_3:
		return '3';
	case OBS_KEY_4:
		return '4';
	case OBS_KEY_5:
		return '5';
	case OBS_KEY_6:
		return '6';
	case OBS_KEY_7:
		return '7';
	case OBS_KEY_8:
		return '8';
	case OBS_KEY_9:
		return '9';
	case OBS_KEY_NUMASTERISK:
		return VK_MULTIPLY;
	case OBS_KEY_NUMPLUS:
		return VK_ADD;
	case OBS_KEY_NUMMINUS:
		return VK_SUBTRACT;
	case OBS_KEY_NUMPERIOD:
		return VK_DECIMAL;
	case OBS_KEY_NUMSLASH:
		return VK_DIVIDE;
	case OBS_KEY_NUM0:
		return VK_NUMPAD0;
	case OBS_KEY_NUM1:
		return VK_NUMPAD1;
	case OBS_KEY_NUM2:
		return VK_NUMPAD2;
	case OBS_KEY_NUM3:
		return VK_NUMPAD3;
	case OBS_KEY_NUM4:
		return VK_NUMPAD4;
	case OBS_KEY_NUM5:
		return VK_NUMPAD5;
	case OBS_KEY_NUM6:
		return VK_NUMPAD6;
	case OBS_KEY_NUM7:
		return VK_NUMPAD7;
	case OBS_KEY_NUM8:
		return VK_NUMPAD8;
	case OBS_KEY_NUM9:
		return VK_NUMPAD9;
	case OBS_KEY_SEMICOLON:
		return VK_OEM_1;
	case OBS_KEY_A:
		return 'A';
	case OBS_KEY_B:
		return 'B';
	case OBS_KEY_C:
		return 'C';
	case OBS_KEY_D:
		return 'D';
	case OBS_KEY_E:
		return 'E';
	case OBS_KEY_F:
		return 'F';
	case OBS_KEY_G:
		return 'G';
	case OBS_KEY_H:
		return 'H';
	case OBS_KEY_I:
		return 'I';
	case OBS_KEY_J:
		return 'J';
	case OBS_KEY_K:
		return 'K';
	case OBS_KEY_L:
		return 'L';
	case OBS_KEY_M:
		return 'M';
	case OBS_KEY_N:
		return 'N';
	case OBS_KEY_O:
		return 'O';
	case OBS_KEY_P:
		return 'P';
	case OBS_KEY_Q:
		return 'Q';
	case OBS_KEY_R:
		return 'R';
	case OBS_KEY_S:
		return 'S';
	case OBS_KEY_T:
		return 'T';
	case OBS_KEY_U:
		return 'U';
	case OBS_KEY_V:
		return 'V';
	case OBS_KEY_W:
		return 'W';
	case OBS_KEY_X:
		return 'X';
	case OBS_KEY_Y:
		return 'Y';
	case OBS_KEY_Z:
		return 'Z';
	case OBS_KEY_BRACKETLEFT:
		return VK_OEM_4;
	case OBS_KEY_BACKSLASH:
		return VK_OEM_5;
	case OBS_KEY_BRACKETRIGHT:
		return VK_OEM_6;
	case OBS_KEY_ASCIITILDE:
		return VK_OEM_3;

	case OBS_KEY_HENKAN:
		return VK_CONVERT;
	case OBS_KEY_MUHENKAN:
		return VK_NONCONVERT;
	case OBS_KEY_KANJI:
		return VK_KANJI;
	case OBS_KEY_TOUROKU:
		return VK_OEM_FJ_TOUROKU;
	case OBS_KEY_MASSYO:
		return VK_OEM_FJ_MASSHOU;

	case OBS_KEY_HANGUL:
		return VK_HANGUL;

	case OBS_KEY_BACKSLASH_RT102:
		return VK_OEM_102;

	case OBS_KEY_MOUSE1:
		return VK_LBUTTON;
	case OBS_KEY_MOUSE2:
		return VK_RBUTTON;
	case OBS_KEY_MOUSE3:
		return VK_MBUTTON;
	case OBS_KEY_MOUSE4:
		return VK_XBUTTON1;
	case OBS_KEY_MOUSE5:
		return VK_XBUTTON2;

	case OBS_KEY_VK_CANCEL:
		return VK_CANCEL;
	case OBS_KEY_0x07:
		return 0x07;
	case OBS_KEY_0x0A:
		return 0x0A;
	case OBS_KEY_0x0B:
		return 0x0B;
	case OBS_KEY_0x0E:
		return 0x0E;
	case OBS_KEY_0x0F:
		return 0x0F;
	case OBS_KEY_0x16:
		return 0x16;
	case OBS_KEY_VK_JUNJA:
		return VK_JUNJA;
	case OBS_KEY_VK_FINAL:
		return VK_FINAL;
	case OBS_KEY_0x1A:
		return 0x1A;
	case OBS_KEY_VK_ACCEPT:
		return VK_ACCEPT;
	case OBS_KEY_VK_MODECHANGE:
		return VK_MODECHANGE;
	case OBS_KEY_VK_SELECT:
		return VK_SELECT;
	case OBS_KEY_VK_PRINT:
		return VK_PRINT;
	case OBS_KEY_VK_EXECUTE:
		return VK_EXECUTE;
	case OBS_KEY_VK_HELP:
		return VK_HELP;
	case OBS_KEY_0x30:
		return 0x30;
	case OBS_KEY_0x31:
		return 0x31;
	case OBS_KEY_0x32:
		return 0x32;
	case OBS_KEY_0x33:
		return 0x33;
	case OBS_KEY_0x34:
		return 0x34;
	case OBS_KEY_0x35:
		return 0x35;
	case OBS_KEY_0x36:
		return 0x36;
	case OBS_KEY_0x37:
		return 0x37;
	case OBS_KEY_0x38:
		return 0x38;
	case OBS_KEY_0x39:
		return 0x39;
	case OBS_KEY_0x3A:
		return 0x3A;
	case OBS_KEY_0x3B:
		return 0x3B;
	case OBS_KEY_0x3C:
		return 0x3C;
	case OBS_KEY_0x3D:
		return 0x3D;
	case OBS_KEY_0x3E:
		return 0x3E;
	case OBS_KEY_0x3F:
		return 0x3F;
	case OBS_KEY_0x40:
		return 0x40;
	case OBS_KEY_0x41:
		return 0x41;
	case OBS_KEY_0x42:
		return 0x42;
	case OBS_KEY_0x43:
		return 0x43;
	case OBS_KEY_0x44:
		return 0x44;
	case OBS_KEY_0x45:
		return 0x45;
	case OBS_KEY_0x46:
		return 0x46;
	case OBS_KEY_0x47:
		return 0x47;
	case OBS_KEY_0x48:
		return 0x48;
	case OBS_KEY_0x49:
		return 0x49;
	case OBS_KEY_0x4A:
		return 0x4A;
	case OBS_KEY_0x4B:
		return 0x4B;
	case OBS_KEY_0x4C:
		return 0x4C;
	case OBS_KEY_0x4D:
		return 0x4D;
	case OBS_KEY_0x4E:
		return 0x4E;
	case OBS_KEY_0x4F:
		return 0x4F;
	case OBS_KEY_0x50:
		return 0x50;
	case OBS_KEY_0x51:
		return 0x51;
	case OBS_KEY_0x52:
		return 0x52;
	case OBS_KEY_0x53:
		return 0x53;
	case OBS_KEY_0x54:
		return 0x54;
	case OBS_KEY_0x55:
		return 0x55;
	case OBS_KEY_0x56:
		return 0x56;
	case OBS_KEY_0x57:
		return 0x57;
	case OBS_KEY_0x58:
		return 0x58;
	case OBS_KEY_0x59:
		return 0x59;
	case OBS_KEY_0x5A:
		return 0x5A;
	case OBS_KEY_VK_LWIN:
		return VK_LWIN;
	case OBS_KEY_VK_RWIN:
		return VK_RWIN;
	case OBS_KEY_VK_APPS:
		return VK_APPS;
	case OBS_KEY_0x5E:
		return 0x5E;
	case OBS_KEY_VK_SLEEP:
		return VK_SLEEP;
	case OBS_KEY_VK_SEPARATOR:
		return VK_SEPARATOR;
	case OBS_KEY_0x88:
		return 0x88;
	case OBS_KEY_0x89:
		return 0x89;
	case OBS_KEY_0x8A:
		return 0x8A;
	case OBS_KEY_0x8B:
		return 0x8B;
	case OBS_KEY_0x8C:
		return 0x8C;
	case OBS_KEY_0x8D:
		return 0x8D;
	case OBS_KEY_0x8E:
		return 0x8E;
	case OBS_KEY_0x8F:
		return 0x8F;
	case OBS_KEY_VK_OEM_FJ_JISHO:
		return VK_OEM_FJ_JISHO;
	case OBS_KEY_VK_OEM_FJ_LOYA:
		return VK_OEM_FJ_LOYA;
	case OBS_KEY_VK_OEM_FJ_ROYA:
		return VK_OEM_FJ_ROYA;
	case OBS_KEY_0x97:
		return 0x97;
	case OBS_KEY_0x98:
		return 0x98;
	case OBS_KEY_0x99:
		return 0x99;
	case OBS_KEY_0x9A:
		return 0x9A;
	case OBS_KEY_0x9B:
		return 0x9B;
	case OBS_KEY_0x9C:
		return 0x9C;
	case OBS_KEY_0x9D:
		return 0x9D;
	case OBS_KEY_0x9E:
		return 0x9E;
	case OBS_KEY_0x9F:
		return 0x9F;
	case OBS_KEY_VK_LSHIFT:
		return VK_LSHIFT;
	case OBS_KEY_VK_RSHIFT:
		return VK_RSHIFT;
	case OBS_KEY_VK_LCONTROL:
		return VK_LCONTROL;
	case OBS_KEY_VK_RCONTROL:
		return VK_RCONTROL;
	case OBS_KEY_VK_LMENU:
		return VK_LMENU;
	case OBS_KEY_VK_RMENU:
		return VK_RMENU;
	case OBS_KEY_VK_BROWSER_BACK:
		return VK_BROWSER_BACK;
	case OBS_KEY_VK_BROWSER_FORWARD:
		return VK_BROWSER_FORWARD;
	case OBS_KEY_VK_BROWSER_REFRESH:
		return VK_BROWSER_REFRESH;
	case OBS_KEY_VK_BROWSER_STOP:
		return VK_BROWSER_STOP;
	case OBS_KEY_VK_BROWSER_SEARCH:
		return VK_BROWSER_SEARCH;
	case OBS_KEY_VK_BROWSER_FAVORITES:
		return VK_BROWSER_FAVORITES;
	case OBS_KEY_VK_BROWSER_HOME:
		return VK_BROWSER_HOME;
	case OBS_KEY_VK_VOLUME_MUTE:
		return VK_VOLUME_MUTE;
	case OBS_KEY_VK_VOLUME_DOWN:
		return VK_VOLUME_DOWN;
	case OBS_KEY_VK_VOLUME_UP:
		return VK_VOLUME_UP;
	case OBS_KEY_VK_MEDIA_NEXT_TRACK:
		return VK_MEDIA_NEXT_TRACK;
	case OBS_KEY_VK_MEDIA_PREV_TRACK:
		return VK_MEDIA_PREV_TRACK;
	case OBS_KEY_VK_MEDIA_STOP:
		return VK_MEDIA_STOP;
	case OBS_KEY_VK_MEDIA_PLAY_PAUSE:
		return VK_MEDIA_PLAY_PAUSE;
	case OBS_KEY_VK_LAUNCH_MAIL:
		return VK_LAUNCH_MAIL;
	case OBS_KEY_VK_LAUNCH_MEDIA_SELECT:
		return VK_LAUNCH_MEDIA_SELECT;
	case OBS_KEY_VK_LAUNCH_APP1:
		return VK_LAUNCH_APP1;
	case OBS_KEY_VK_LAUNCH_APP2:
		return VK_LAUNCH_APP2;
	case OBS_KEY_0xB8:
		return 0xB8;
	case OBS_KEY_0xB9:
		return 0xB9;
	case OBS_KEY_0xC1:
		return 0xC1;
	case OBS_KEY_0xC2:
		return 0xC2;
	case OBS_KEY_0xC3:
		return 0xC3;
	case OBS_KEY_0xC4:
		return 0xC4;
	case OBS_KEY_0xC5:
		return 0xC5;
	case OBS_KEY_0xC6:
		return 0xC6;
	case OBS_KEY_0xC7:
		return 0xC7;
	case OBS_KEY_0xC8:
		return 0xC8;
	case OBS_KEY_0xC9:
		return 0xC9;
	case OBS_KEY_0xCA:
		return 0xCA;
	case OBS_KEY_0xCB:
		return 0xCB;
	case OBS_KEY_0xCC:
		return 0xCC;
	case OBS_KEY_0xCD:
		return 0xCD;
	case OBS_KEY_0xCE:
		return 0xCE;
	case OBS_KEY_0xCF:
		return 0xCF;
	case OBS_KEY_0xD0:
		return 0xD0;
	case OBS_KEY_0xD1:
		return 0xD1;
	case OBS_KEY_0xD2:
		return 0xD2;
	case OBS_KEY_0xD3:
		return 0xD3;
	case OBS_KEY_0xD4:
		return 0xD4;
	case OBS_KEY_0xD5:
		return 0xD5;
	case OBS_KEY_0xD6:
		return 0xD6;
	case OBS_KEY_0xD7:
		return 0xD7;
	case OBS_KEY_0xD8:
		return 0xD8;
	case OBS_KEY_0xD9:
		return 0xD9;
	case OBS_KEY_0xDA:
		return 0xDA;
	case OBS_KEY_VK_OEM_8:
		return VK_OEM_8;
	case OBS_KEY_0xE0:
		return 0xE0;
	case OBS_KEY_VK_OEM_AX:
		return VK_OEM_AX;
	case OBS_KEY_VK_ICO_HELP:
		return VK_ICO_HELP;
	case OBS_KEY_VK_ICO_00:
		return VK_ICO_00;
	case OBS_KEY_VK_PROCESSKEY:
		return VK_PROCESSKEY;
	case OBS_KEY_VK_ICO_CLEAR:
		return VK_ICO_CLEAR;
	case OBS_KEY_VK_PACKET:
		return VK_PACKET;
	case OBS_KEY_0xE8:
		return 0xE8;
	case OBS_KEY_VK_OEM_RESET:
		return VK_OEM_RESET;
	case OBS_KEY_VK_OEM_JUMP:
		return VK_OEM_JUMP;
	case OBS_KEY_VK_OEM_PA1:
		return VK_OEM_PA1;
	case OBS_KEY_VK_OEM_PA2:
		return VK_OEM_PA2;
	case OBS_KEY_VK_OEM_PA3:
		return VK_OEM_PA3;
	case OBS_KEY_VK_OEM_WSCTRL:
		return VK_OEM_WSCTRL;
	case OBS_KEY_VK_OEM_CUSEL:
		return VK_OEM_CUSEL;
	case OBS_KEY_VK_OEM_ATTN:
		return VK_OEM_ATTN;
	case OBS_KEY_VK_OEM_FINISH:
		return VK_OEM_FINISH;
	case OBS_KEY_VK_OEM_COPY:
		return VK_OEM_COPY;
	case OBS_KEY_VK_OEM_AUTO:
		return VK_OEM_AUTO;
	case OBS_KEY_VK_OEM_ENLW:
		return VK_OEM_ENLW;
	case OBS_KEY_VK_ATTN:
		return VK_ATTN;
	case OBS_KEY_VK_CRSEL:
		return VK_CRSEL;
	case OBS_KEY_VK_EXSEL:
		return VK_EXSEL;
	case OBS_KEY_VK_EREOF:
		return VK_EREOF;
	case OBS_KEY_VK_PLAY:
		return VK_PLAY;
	case OBS_KEY_VK_ZOOM:
		return VK_ZOOM;
	case OBS_KEY_VK_NONAME:
		return VK_NONAME;
	case OBS_KEY_VK_PA1:
		return VK_PA1;
	case OBS_KEY_VK_OEM_CLEAR:
		return VK_OEM_CLEAR;

	/* TODO: Implement keys for non-US keyboards */
	default:;
	}
	return 0;
}

bool obs_hotkeys_platform_init(struct obs_core_hotkeys *hotkeys)
{
	hotkeys->platform_context = bzalloc(sizeof(obs_hotkeys_platform_t));

	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++)
		hotkeys->platform_context->vk_codes[i] = get_virtual_key(i);

	return true;
}

void obs_hotkeys_platform_free(struct obs_core_hotkeys *hotkeys)
{
	bfree(hotkeys->platform_context);
	hotkeys->platform_context = NULL;
}

static bool vk_down(DWORD vk)
{
	short state = GetAsyncKeyState(vk);
	bool down = (state & 0x8000) != 0;

	return down;
}

bool obs_hotkeys_platform_is_pressed(obs_hotkeys_platform_t *context,
				     obs_key_t key)
{
	if (key == OBS_KEY_META) {
		return vk_down(VK_LWIN) || vk_down(VK_RWIN);
	}

	UNUSED_PARAMETER(context);
	return vk_down(obs_key_to_virtual_key(key));
}

void obs_key_to_str(obs_key_t key, struct dstr *str)
{
	wchar_t name[128] = L"";
	UINT scan_code;
	int vk;

	if (key == OBS_KEY_NONE) {
		return;

	} else if (key >= OBS_KEY_F13 && key <= OBS_KEY_F24) {
		dstr_printf(str, "F%d", (int)(key - OBS_KEY_F13 + 13));
		return;
	} else if (key >= OBS_KEY_MOUSE1 && key <= OBS_KEY_MOUSE29) {
		if (obs->hotkeys.translations[key]) {
			dstr_copy(str, obs->hotkeys.translations[key]);
		} else {
			dstr_printf(str, "Mouse %d",
				    (int)(key - OBS_KEY_MOUSE1 + 1));
		}
		return;
	}
	if (key == OBS_KEY_PAUSE) {
		dstr_copy(str, obs_get_hotkey_translation(key, "Pause"));
		return;

	} else if (key == OBS_KEY_META) {
		dstr_copy(str, obs_get_hotkey_translation(key, "Windows"));
		return;
	}

	vk = obs_key_to_virtual_key(key);
	scan_code = MapVirtualKey(vk, 0) << 16;

	switch (vk) {
	case VK_HOME:
	case VK_END:
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_INSERT:
	case VK_DELETE:
	case VK_NUMLOCK:
		scan_code |= 0x01000000;
	}

	if ((key < OBS_KEY_VK_CANCEL || key > OBS_KEY_VK_OEM_CLEAR) &&
	    scan_code != 0 && GetKeyNameTextW(scan_code, name, 128) != 0) {
		dstr_from_wcs(str, name);
	} else if (key != OBS_KEY_NONE) {
		dstr_copy(str, obs_key_to_name(key));
	}
}

obs_key_t obs_key_from_virtual_key(int code)
{
	obs_hotkeys_platform_t *platform = obs->hotkeys.platform_context;

	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++) {
		if (platform->vk_codes[i] == code) {
			return (obs_key_t)i;
		}
	}

	return OBS_KEY_NONE;
}

int obs_key_to_virtual_key(obs_key_t key)
{
	if (key == OBS_KEY_META)
		return VK_LWIN;

	return obs->hotkeys.platform_context->vk_codes[(int)key];
}

static inline void add_combo_key(obs_key_t key, struct dstr *str)
{
	struct dstr key_str = {0};

	obs_key_to_str(key, &key_str);

	if (!dstr_is_empty(&key_str)) {
		if (!dstr_is_empty(str)) {
			dstr_cat(str, " + ");
		}
		dstr_cat_dstr(str, &key_str);
	}

	dstr_free(&key_str);
}

void obs_key_combination_to_str(obs_key_combination_t combination,
				struct dstr *str)
{
	if ((combination.modifiers & INTERACT_CONTROL_KEY) != 0) {
		add_combo_key(OBS_KEY_CONTROL, str);
	}
	if ((combination.modifiers & INTERACT_COMMAND_KEY) != 0) {
		add_combo_key(OBS_KEY_META, str);
	}
	if ((combination.modifiers & INTERACT_ALT_KEY) != 0) {
		add_combo_key(OBS_KEY_ALT, str);
	}
	if ((combination.modifiers & INTERACT_SHIFT_KEY) != 0) {
		add_combo_key(OBS_KEY_SHIFT, str);
	}
	if (combination.key != OBS_KEY_NONE) {
		add_combo_key(combination.key, str);
	}
}

bool sym_initialize_called = false;

void reset_win32_symbol_paths(void)
{
	static BOOL(WINAPI * sym_initialize_w)(HANDLE, const wchar_t *, BOOL);
	static BOOL(WINAPI * sym_set_search_path_w)(HANDLE, const wchar_t *);
	static bool funcs_initialized = false;
	static bool initialize_success = false;

	struct obs_module *module = obs->first_module;
	struct dstr path_str = {0};
	DARRAY(char *) paths;
	wchar_t *path_str_w = NULL;
	char *abspath;

	da_init(paths);

	if (!funcs_initialized) {
		HMODULE mod;
		funcs_initialized = true;

		mod = LoadLibraryW(L"DbgHelp");
		if (!mod)
			return;

		sym_initialize_w =
			(void *)GetProcAddress(mod, "SymInitializeW");
		sym_set_search_path_w =
			(void *)GetProcAddress(mod, "SymSetSearchPathW");
		if (!sym_initialize_w || !sym_set_search_path_w) {
			FreeLibrary(mod);
			return;
		}

		initialize_success = true;
		// Leaks 'mod' once.
	}

	if (!initialize_success)
		return;

	abspath = os_get_abs_path_ptr(".");
	if (abspath)
		da_push_back(paths, &abspath);

	while (module) {
		bool found = false;
		struct dstr path = {0};
		char *path_end;

		dstr_copy(&path, module->bin_path);
		dstr_replace(&path, "/", "\\");

		path_end = strrchr(path.array, '\\');
		if (!path_end) {
			module = module->next;
			dstr_free(&path);
			continue;
		}

		*path_end = 0;

		for (size_t i = 0; i < paths.num; i++) {
			const char *existing_path = paths.array[i];
			if (astrcmpi(path.array, existing_path) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			abspath = os_get_abs_path_ptr(path.array);
			if (abspath)
				da_push_back(paths, &abspath);
		}

		dstr_free(&path);

		module = module->next;
	}

	for (size_t i = 0; i < paths.num; i++) {
		const char *path = paths.array[i];
		if (path && *path) {
			if (i != 0)
				dstr_cat(&path_str, ";");
			dstr_cat(&path_str, paths.array[i]);
		}
	}

	if (path_str.array) {
		os_utf8_to_wcs_ptr(path_str.array, path_str.len, &path_str_w);
		if (path_str_w) {
			if (!sym_initialize_called) {
				sym_initialize_w(GetCurrentProcess(),
						 path_str_w, false);
				sym_initialize_called = true;
			} else {
				sym_set_search_path_w(GetCurrentProcess(),
						      path_str_w);
			}

			bfree(path_str_w);
		}
	}

	for (size_t i = 0; i < paths.num; i++)
		bfree(paths.array[i]);
	dstr_free(&path_str);
	da_free(paths);
}

extern void initialize_crash_handler(void);

void obs_init_win32_crash_handler(void)
{
	initialize_crash_handler();
}

bool initialize_com(void)
{
	const HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	const bool success = SUCCEEDED(hr);
	if (!success)
		blog(LOG_ERROR, "CoInitializeEx failed: 0x%08X", hr);
	return success;
}

void uninitialize_com(void)
{
	CoUninitialize();
}
