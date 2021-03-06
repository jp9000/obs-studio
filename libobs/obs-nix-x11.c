/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>
    Copyright (C) 2014 by Zachary Lund <admin@computerquip.com>
    Copyright (C) 2019 by Jason Francis <cycl0ps@tuta.io>

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

#include "obs-internal.h"
#include "obs-nix-platform.h"
#include "obs-nix-x11.h"

#include <xcb/xcb.h>
#if USE_XINPUT
#include <xcb/xinput.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>
#include <X11/XF86keysym.h>
#include <X11/Sunkeysym.h>

void obs_nix_x11_log_info(void)
{
	Display *dpy = obs_get_nix_platform_display();
	if (!dpy) {
		blog(LOG_INFO, "Unable to open X display");
		return;
	}

	int protocol_version = ProtocolVersion(dpy);
	int protocol_revision = ProtocolRevision(dpy);
	int vendor_release = VendorRelease(dpy);
	const char *vendor_name = ServerVendor(dpy);

	if (strstr(vendor_name, "X.Org")) {
		blog(LOG_INFO,
		     "Window System: X%d.%d, Vendor: %s, Version: %d"
		     ".%d.%d",
		     protocol_version, protocol_revision, vendor_name,
		     vendor_release / 10000000, (vendor_release / 100000) % 100,
		     (vendor_release / 1000) % 100);
	} else {
		blog(LOG_INFO,
		     "Window System: X%d.%d - vendor string: %s - "
		     "vendor release: %d",
		     protocol_version, protocol_revision, vendor_name,
		     vendor_release);
	}
}

/* So here's how linux works with key mapping:
 *
 * First, there's a global key symbol enum (xcb_keysym_t) which has unique
 * values for all possible symbols keys can have (e.g., '1' and '!' are
 * different values).
 *
 * Then there's a key code (xcb_keycode_t), which is basically an index to the
 * actual key itself on the keyboard (e.g., '1' and '!' will share the same
 * value).
 *
 * xcb_keysym_t values should be given to libobs, and libobs will translate it
 * to an obs_key_t, and although xcb_keysym_t can differ ('!' vs '1'), it will
 * get the obs_key_t value that represents the actual key pressed; in other
 * words it will be based on the key code rather than the key symbol.  The same
 * applies to checking key press states.
 */

struct keycode_list {
	DARRAY(xcb_keycode_t) list;
};

struct obs_hotkeys_platform {
	Display *display;
	xcb_keysym_t base_keysyms[OBS_KEY_LAST_VALUE];
	struct keycode_list keycodes[OBS_KEY_LAST_VALUE];
	xcb_keycode_t min_keycode;
	xcb_keycode_t super_l_code;
	xcb_keycode_t super_r_code;

	/* stores a copy of the keysym map for keycodes */
	xcb_keysym_t *keysyms;
	int num_keysyms;
	int syms_per_code;

#if USE_XINPUT
	bool pressed[XINPUT_MOUSE_LEN];
	bool update[XINPUT_MOUSE_LEN];
	bool button_pressed[XINPUT_MOUSE_LEN];
#endif
};

#define MOUSE_1 (1 << 16)
#define MOUSE_2 (2 << 16)
#define MOUSE_3 (3 << 16)
#define MOUSE_4 (4 << 16)
#define MOUSE_5 (5 << 16)

static int get_keysym(obs_key_t key)
{
	switch (key) {
	case OBS_KEY_RETURN:
		return XK_Return;
	case OBS_KEY_ESCAPE:
		return XK_Escape;
	case OBS_KEY_TAB:
		return XK_Tab;
	case OBS_KEY_BACKSPACE:
		return XK_BackSpace;
	case OBS_KEY_INSERT:
		return XK_Insert;
	case OBS_KEY_DELETE:
		return XK_Delete;
	case OBS_KEY_PAUSE:
		return XK_Pause;
	case OBS_KEY_PRINT:
		return XK_Print;
	case OBS_KEY_HOME:
		return XK_Home;
	case OBS_KEY_END:
		return XK_End;
	case OBS_KEY_LEFT:
		return XK_Left;
	case OBS_KEY_UP:
		return XK_Up;
	case OBS_KEY_RIGHT:
		return XK_Right;
	case OBS_KEY_DOWN:
		return XK_Down;
	case OBS_KEY_PAGEUP:
		return XK_Prior;
	case OBS_KEY_PAGEDOWN:
		return XK_Next;

	case OBS_KEY_SHIFT:
		return XK_Shift_L;
	case OBS_KEY_CONTROL:
		return XK_Control_L;
	case OBS_KEY_ALT:
		return XK_Alt_L;
	case OBS_KEY_CAPSLOCK:
		return XK_Caps_Lock;
	case OBS_KEY_NUMLOCK:
		return XK_Num_Lock;
	case OBS_KEY_SCROLLLOCK:
		return XK_Scroll_Lock;

	case OBS_KEY_F1:
		return XK_F1;
	case OBS_KEY_F2:
		return XK_F2;
	case OBS_KEY_F3:
		return XK_F3;
	case OBS_KEY_F4:
		return XK_F4;
	case OBS_KEY_F5:
		return XK_F5;
	case OBS_KEY_F6:
		return XK_F6;
	case OBS_KEY_F7:
		return XK_F7;
	case OBS_KEY_F8:
		return XK_F8;
	case OBS_KEY_F9:
		return XK_F9;
	case OBS_KEY_F10:
		return XK_F10;
	case OBS_KEY_F11:
		return XK_F11;
	case OBS_KEY_F12:
		return XK_F12;
	case OBS_KEY_F13:
		return XK_F13;
	case OBS_KEY_F14:
		return XK_F14;
	case OBS_KEY_F15:
		return XK_F15;
	case OBS_KEY_F16:
		return XK_F16;
	case OBS_KEY_F17:
		return XK_F17;
	case OBS_KEY_F18:
		return XK_F18;
	case OBS_KEY_F19:
		return XK_F19;
	case OBS_KEY_F20:
		return XK_F20;
	case OBS_KEY_F21:
		return XK_F21;
	case OBS_KEY_F22:
		return XK_F22;
	case OBS_KEY_F23:
		return XK_F23;
	case OBS_KEY_F24:
		return XK_F24;
	case OBS_KEY_F25:
		return XK_F25;
	case OBS_KEY_F26:
		return XK_F26;
	case OBS_KEY_F27:
		return XK_F27;
	case OBS_KEY_F28:
		return XK_F28;
	case OBS_KEY_F29:
		return XK_F29;
	case OBS_KEY_F30:
		return XK_F30;
	case OBS_KEY_F31:
		return XK_F31;
	case OBS_KEY_F32:
		return XK_F32;
	case OBS_KEY_F33:
		return XK_F33;
	case OBS_KEY_F34:
		return XK_F34;
	case OBS_KEY_F35:
		return XK_F35;

	case OBS_KEY_MENU:
		return XK_Menu;
	case OBS_KEY_HYPER_L:
		return XK_Hyper_L;
	case OBS_KEY_HYPER_R:
		return XK_Hyper_R;
	case OBS_KEY_HELP:
		return XK_Help;
	case OBS_KEY_CANCEL:
		return XK_Cancel;
	case OBS_KEY_FIND:
		return XK_Find;
	case OBS_KEY_REDO:
		return XK_Redo;
	case OBS_KEY_UNDO:
		return XK_Undo;
	case OBS_KEY_SPACE:
		return XK_space;

	case OBS_KEY_COPY:
		return XF86XK_Copy;
	case OBS_KEY_CUT:
		return XF86XK_Cut;
	case OBS_KEY_OPEN:
		return XF86XK_Open;
	case OBS_KEY_PASTE:
		return XF86XK_Paste;
	case OBS_KEY_FRONT:
		return SunXK_Front;
	case OBS_KEY_PROPS:
		return SunXK_Props;

	case OBS_KEY_EXCLAM:
		return XK_exclam;
	case OBS_KEY_QUOTEDBL:
		return XK_quotedbl;
	case OBS_KEY_NUMBERSIGN:
		return XK_numbersign;
	case OBS_KEY_DOLLAR:
		return XK_dollar;
	case OBS_KEY_PERCENT:
		return XK_percent;
	case OBS_KEY_AMPERSAND:
		return XK_ampersand;
	case OBS_KEY_APOSTROPHE:
		return XK_apostrophe;
	case OBS_KEY_PARENLEFT:
		return XK_parenleft;
	case OBS_KEY_PARENRIGHT:
		return XK_parenright;
	case OBS_KEY_ASTERISK:
		return XK_asterisk;
	case OBS_KEY_PLUS:
		return XK_plus;
	case OBS_KEY_COMMA:
		return XK_comma;
	case OBS_KEY_MINUS:
		return XK_minus;
	case OBS_KEY_PERIOD:
		return XK_period;
	case OBS_KEY_SLASH:
		return XK_slash;
	case OBS_KEY_0:
		return XK_0;
	case OBS_KEY_1:
		return XK_1;
	case OBS_KEY_2:
		return XK_2;
	case OBS_KEY_3:
		return XK_3;
	case OBS_KEY_4:
		return XK_4;
	case OBS_KEY_5:
		return XK_5;
	case OBS_KEY_6:
		return XK_6;
	case OBS_KEY_7:
		return XK_7;
	case OBS_KEY_8:
		return XK_8;
	case OBS_KEY_9:
		return XK_9;
	case OBS_KEY_NUMEQUAL:
		return XK_KP_Equal;
	case OBS_KEY_NUMASTERISK:
		return XK_KP_Multiply;
	case OBS_KEY_NUMPLUS:
		return XK_KP_Add;
	case OBS_KEY_NUMCOMMA:
		return XK_KP_Separator;
	case OBS_KEY_NUMMINUS:
		return XK_KP_Subtract;
	case OBS_KEY_NUMPERIOD:
		return XK_KP_Decimal;
	case OBS_KEY_NUMSLASH:
		return XK_KP_Divide;
	case OBS_KEY_NUM0:
		return XK_KP_0;
	case OBS_KEY_NUM1:
		return XK_KP_1;
	case OBS_KEY_NUM2:
		return XK_KP_2;
	case OBS_KEY_NUM3:
		return XK_KP_3;
	case OBS_KEY_NUM4:
		return XK_KP_4;
	case OBS_KEY_NUM5:
		return XK_KP_5;
	case OBS_KEY_NUM6:
		return XK_KP_6;
	case OBS_KEY_NUM7:
		return XK_KP_7;
	case OBS_KEY_NUM8:
		return XK_KP_8;
	case OBS_KEY_NUM9:
		return XK_KP_9;
	case OBS_KEY_COLON:
		return XK_colon;
	case OBS_KEY_SEMICOLON:
		return XK_semicolon;
	case OBS_KEY_LESS:
		return XK_less;
	case OBS_KEY_EQUAL:
		return XK_equal;
	case OBS_KEY_GREATER:
		return XK_greater;
	case OBS_KEY_QUESTION:
		return XK_question;
	case OBS_KEY_AT:
		return XK_at;
	case OBS_KEY_A:
		return XK_A;
	case OBS_KEY_B:
		return XK_B;
	case OBS_KEY_C:
		return XK_C;
	case OBS_KEY_D:
		return XK_D;
	case OBS_KEY_E:
		return XK_E;
	case OBS_KEY_F:
		return XK_F;
	case OBS_KEY_G:
		return XK_G;
	case OBS_KEY_H:
		return XK_H;
	case OBS_KEY_I:
		return XK_I;
	case OBS_KEY_J:
		return XK_J;
	case OBS_KEY_K:
		return XK_K;
	case OBS_KEY_L:
		return XK_L;
	case OBS_KEY_M:
		return XK_M;
	case OBS_KEY_N:
		return XK_N;
	case OBS_KEY_O:
		return XK_O;
	case OBS_KEY_P:
		return XK_P;
	case OBS_KEY_Q:
		return XK_Q;
	case OBS_KEY_R:
		return XK_R;
	case OBS_KEY_S:
		return XK_S;
	case OBS_KEY_T:
		return XK_T;
	case OBS_KEY_U:
		return XK_U;
	case OBS_KEY_V:
		return XK_V;
	case OBS_KEY_W:
		return XK_W;
	case OBS_KEY_X:
		return XK_X;
	case OBS_KEY_Y:
		return XK_Y;
	case OBS_KEY_Z:
		return XK_Z;
	case OBS_KEY_BRACKETLEFT:
		return XK_bracketleft;
	case OBS_KEY_BACKSLASH:
		return XK_backslash;
	case OBS_KEY_BRACKETRIGHT:
		return XK_bracketright;
	case OBS_KEY_ASCIICIRCUM:
		return XK_asciicircum;
	case OBS_KEY_UNDERSCORE:
		return XK_underscore;
	case OBS_KEY_QUOTELEFT:
		return XK_quoteleft;
	case OBS_KEY_BRACELEFT:
		return XK_braceleft;
	case OBS_KEY_BAR:
		return XK_bar;
	case OBS_KEY_BRACERIGHT:
		return XK_braceright;
	case OBS_KEY_ASCIITILDE:
		return XK_grave;
	case OBS_KEY_NOBREAKSPACE:
		return XK_nobreakspace;
	case OBS_KEY_EXCLAMDOWN:
		return XK_exclamdown;
	case OBS_KEY_CENT:
		return XK_cent;
	case OBS_KEY_STERLING:
		return XK_sterling;
	case OBS_KEY_CURRENCY:
		return XK_currency;
	case OBS_KEY_YEN:
		return XK_yen;
	case OBS_KEY_BROKENBAR:
		return XK_brokenbar;
	case OBS_KEY_SECTION:
		return XK_section;
	case OBS_KEY_DIAERESIS:
		return XK_diaeresis;
	case OBS_KEY_COPYRIGHT:
		return XK_copyright;
	case OBS_KEY_ORDFEMININE:
		return XK_ordfeminine;
	case OBS_KEY_GUILLEMOTLEFT:
		return XK_guillemotleft;
	case OBS_KEY_NOTSIGN:
		return XK_notsign;
	case OBS_KEY_HYPHEN:
		return XK_hyphen;
	case OBS_KEY_REGISTERED:
		return XK_registered;
	case OBS_KEY_MACRON:
		return XK_macron;
	case OBS_KEY_DEGREE:
		return XK_degree;
	case OBS_KEY_PLUSMINUS:
		return XK_plusminus;
	case OBS_KEY_TWOSUPERIOR:
		return XK_twosuperior;
	case OBS_KEY_THREESUPERIOR:
		return XK_threesuperior;
	case OBS_KEY_ACUTE:
		return XK_acute;
	case OBS_KEY_MU:
		return XK_mu;
	case OBS_KEY_PARAGRAPH:
		return XK_paragraph;
	case OBS_KEY_PERIODCENTERED:
		return XK_periodcentered;
	case OBS_KEY_CEDILLA:
		return XK_cedilla;
	case OBS_KEY_ONESUPERIOR:
		return XK_onesuperior;
	case OBS_KEY_MASCULINE:
		return XK_masculine;
	case OBS_KEY_GUILLEMOTRIGHT:
		return XK_guillemotright;
	case OBS_KEY_ONEQUARTER:
		return XK_onequarter;
	case OBS_KEY_ONEHALF:
		return XK_onehalf;
	case OBS_KEY_THREEQUARTERS:
		return XK_threequarters;
	case OBS_KEY_QUESTIONDOWN:
		return XK_questiondown;
	case OBS_KEY_AGRAVE:
		return XK_Agrave;
	case OBS_KEY_AACUTE:
		return XK_Aacute;
	case OBS_KEY_ACIRCUMFLEX:
		return XK_Acircumflex;
	case OBS_KEY_ATILDE:
		return XK_Atilde;
	case OBS_KEY_ADIAERESIS:
		return XK_Adiaeresis;
	case OBS_KEY_ARING:
		return XK_Aring;
	case OBS_KEY_AE:
		return XK_AE;
	case OBS_KEY_CCEDILLA:
		return XK_cedilla;
	case OBS_KEY_EGRAVE:
		return XK_Egrave;
	case OBS_KEY_EACUTE:
		return XK_Eacute;
	case OBS_KEY_ECIRCUMFLEX:
		return XK_Ecircumflex;
	case OBS_KEY_EDIAERESIS:
		return XK_Ediaeresis;
	case OBS_KEY_IGRAVE:
		return XK_Igrave;
	case OBS_KEY_IACUTE:
		return XK_Iacute;
	case OBS_KEY_ICIRCUMFLEX:
		return XK_Icircumflex;
	case OBS_KEY_IDIAERESIS:
		return XK_Idiaeresis;
	case OBS_KEY_ETH:
		return XK_ETH;
	case OBS_KEY_NTILDE:
		return XK_Ntilde;
	case OBS_KEY_OGRAVE:
		return XK_Ograve;
	case OBS_KEY_OACUTE:
		return XK_Oacute;
	case OBS_KEY_OCIRCUMFLEX:
		return XK_Ocircumflex;
	case OBS_KEY_ODIAERESIS:
		return XK_Odiaeresis;
	case OBS_KEY_MULTIPLY:
		return XK_multiply;
	case OBS_KEY_OOBLIQUE:
		return XK_Ooblique;
	case OBS_KEY_UGRAVE:
		return XK_Ugrave;
	case OBS_KEY_UACUTE:
		return XK_Uacute;
	case OBS_KEY_UCIRCUMFLEX:
		return XK_Ucircumflex;
	case OBS_KEY_UDIAERESIS:
		return XK_Udiaeresis;
	case OBS_KEY_YACUTE:
		return XK_Yacute;
	case OBS_KEY_THORN:
		return XK_Thorn;
	case OBS_KEY_SSHARP:
		return XK_ssharp;
	case OBS_KEY_DIVISION:
		return XK_division;
	case OBS_KEY_YDIAERESIS:
		return XK_Ydiaeresis;
	case OBS_KEY_MULTI_KEY:
		return XK_Multi_key;
	case OBS_KEY_CODEINPUT:
		return XK_Codeinput;
	case OBS_KEY_SINGLECANDIDATE:
		return XK_SingleCandidate;
	case OBS_KEY_MULTIPLECANDIDATE:
		return XK_MultipleCandidate;
	case OBS_KEY_PREVIOUSCANDIDATE:
		return XK_PreviousCandidate;
	case OBS_KEY_MODE_SWITCH:
		return XK_Mode_switch;
	case OBS_KEY_KANJI:
		return XK_Kanji;
	case OBS_KEY_MUHENKAN:
		return XK_Muhenkan;
	case OBS_KEY_HENKAN:
		return XK_Henkan;
	case OBS_KEY_ROMAJI:
		return XK_Romaji;
	case OBS_KEY_HIRAGANA:
		return XK_Hiragana;
	case OBS_KEY_KATAKANA:
		return XK_Katakana;
	case OBS_KEY_HIRAGANA_KATAKANA:
		return XK_Hiragana_Katakana;
	case OBS_KEY_ZENKAKU:
		return XK_Zenkaku;
	case OBS_KEY_HANKAKU:
		return XK_Hankaku;
	case OBS_KEY_ZENKAKU_HANKAKU:
		return XK_Zenkaku_Hankaku;
	case OBS_KEY_TOUROKU:
		return XK_Touroku;
	case OBS_KEY_MASSYO:
		return XK_Massyo;
	case OBS_KEY_KANA_LOCK:
		return XK_Kana_Lock;
	case OBS_KEY_KANA_SHIFT:
		return XK_Kana_Shift;
	case OBS_KEY_EISU_SHIFT:
		return XK_Eisu_Shift;
	case OBS_KEY_EISU_TOGGLE:
		return XK_Eisu_toggle;
	case OBS_KEY_HANGUL:
		return XK_Hangul;
	case OBS_KEY_HANGUL_START:
		return XK_Hangul_Start;
	case OBS_KEY_HANGUL_END:
		return XK_Hangul_End;
	case OBS_KEY_HANGUL_HANJA:
		return XK_Hangul_Hanja;
	case OBS_KEY_HANGUL_JAMO:
		return XK_Hangul_Jamo;
	case OBS_KEY_HANGUL_ROMAJA:
		return XK_Hangul_Romaja;
	case OBS_KEY_HANGUL_BANJA:
		return XK_Hangul_Banja;
	case OBS_KEY_HANGUL_PREHANJA:
		return XK_Hangul_PreHanja;
	case OBS_KEY_HANGUL_POSTHANJA:
		return XK_Hangul_PostHanja;
	case OBS_KEY_HANGUL_SPECIAL:
		return XK_Hangul_Special;
	case OBS_KEY_DEAD_GRAVE:
		return XK_dead_grave;
	case OBS_KEY_DEAD_ACUTE:
		return XK_dead_acute;
	case OBS_KEY_DEAD_CIRCUMFLEX:
		return XK_dead_circumflex;
	case OBS_KEY_DEAD_TILDE:
		return XK_dead_tilde;
	case OBS_KEY_DEAD_MACRON:
		return XK_dead_macron;
	case OBS_KEY_DEAD_BREVE:
		return XK_dead_breve;
	case OBS_KEY_DEAD_ABOVEDOT:
		return XK_dead_abovedot;
	case OBS_KEY_DEAD_DIAERESIS:
		return XK_dead_diaeresis;
	case OBS_KEY_DEAD_ABOVERING:
		return XK_dead_abovering;
	case OBS_KEY_DEAD_DOUBLEACUTE:
		return XK_dead_doubleacute;
	case OBS_KEY_DEAD_CARON:
		return XK_dead_caron;
	case OBS_KEY_DEAD_CEDILLA:
		return XK_dead_cedilla;
	case OBS_KEY_DEAD_OGONEK:
		return XK_dead_ogonek;
	case OBS_KEY_DEAD_IOTA:
		return XK_dead_iota;
	case OBS_KEY_DEAD_VOICED_SOUND:
		return XK_dead_voiced_sound;
	case OBS_KEY_DEAD_SEMIVOICED_SOUND:
		return XK_dead_semivoiced_sound;
	case OBS_KEY_DEAD_BELOWDOT:
		return XK_dead_belowdot;
	case OBS_KEY_DEAD_HOOK:
		return XK_dead_hook;
	case OBS_KEY_DEAD_HORN:
		return XK_dead_horn;

	case OBS_KEY_MOUSE1:
		return MOUSE_1;
	case OBS_KEY_MOUSE2:
		return MOUSE_2;
	case OBS_KEY_MOUSE3:
		return MOUSE_3;
	case OBS_KEY_MOUSE4:
		return MOUSE_4;
	case OBS_KEY_MOUSE5:
		return MOUSE_5;

	/* TODO: Implement keys for non-US keyboards */
	default:;
	}
	return 0;
}

static inline void fill_base_keysyms(struct obs_core_hotkeys *hotkeys)
{
	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++)
		hotkeys->platform_context->base_keysyms[i] = get_keysym(i);
}

static obs_key_t key_from_base_keysym(obs_hotkeys_platform_t *context,
				      xcb_keysym_t code)
{
	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++) {
		if (context->base_keysyms[i] == (xcb_keysym_t)code) {
			return (obs_key_t)i;
		}
	}

	return OBS_KEY_NONE;
}

static inline void add_key(obs_hotkeys_platform_t *context, obs_key_t key,
			   int code)
{
	xcb_keycode_t kc = (xcb_keycode_t)code;
	da_push_back(context->keycodes[key].list, &kc);

	if (context->keycodes[key].list.num > 1) {
		blog(LOG_DEBUG,
		     "found alternate keycode %d for %s "
		     "which already has keycode %d",
		     code, obs_key_to_name(key),
		     (int)context->keycodes[key].list.array[0]);
	}
}

static inline bool fill_keycodes(struct obs_core_hotkeys *hotkeys)
{
	obs_hotkeys_platform_t *context = hotkeys->platform_context;
	xcb_connection_t *connection = XGetXCBConnection(context->display);
	const struct xcb_setup_t *setup = xcb_get_setup(connection);
	xcb_get_keyboard_mapping_cookie_t cookie;
	xcb_get_keyboard_mapping_reply_t *reply;
	xcb_generic_error_t *error = NULL;
	int code;

	int mincode = setup->min_keycode;
	int maxcode = setup->max_keycode;

	context->min_keycode = setup->min_keycode;

	cookie = xcb_get_keyboard_mapping(connection, mincode,
					  maxcode - mincode + 1);

	reply = xcb_get_keyboard_mapping_reply(connection, cookie, &error);

	if (error || !reply) {
		blog(LOG_WARNING, "xcb_get_keyboard_mapping_reply failed");
		goto error1;
	}

	const xcb_keysym_t *keysyms = xcb_get_keyboard_mapping_keysyms(reply);
	int syms_per_code = (int)reply->keysyms_per_keycode;

	context->num_keysyms = (maxcode - mincode + 1) * syms_per_code;
	context->syms_per_code = syms_per_code;
	context->keysyms =
		bmemdup(keysyms, sizeof(xcb_keysym_t) * context->num_keysyms);

	for (code = mincode; code <= maxcode; code++) {
		const xcb_keysym_t *sym;
		obs_key_t key;

		sym = &keysyms[(code - mincode) * syms_per_code];

		for (int i = 0; i < syms_per_code; i++) {
			if (!sym[i])
				break;

			if (sym[i] == XK_Super_L) {
				context->super_l_code = code;
				break;
			} else if (sym[i] == XK_Super_R) {
				context->super_r_code = code;
				break;
			} else {
				key = key_from_base_keysym(context, sym[i]);

				if (key != OBS_KEY_NONE) {
					add_key(context, key, code);
					break;
				}
			}
		}
	}

error1:
	free(reply);
	free(error);

	return error != NULL || reply == NULL;
}

static xcb_screen_t *default_screen(obs_hotkeys_platform_t *context,
				    xcb_connection_t *connection)
{
	int def_screen_idx = XDefaultScreen(context->display);
	xcb_screen_iterator_t iter;

	iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
	while (iter.rem) {
		if (def_screen_idx-- == 0)
			return iter.data;

		xcb_screen_next(&iter);
	}

	return NULL;
}

static inline xcb_window_t root_window(obs_hotkeys_platform_t *context,
				       xcb_connection_t *connection)
{
	xcb_screen_t *screen = default_screen(context, connection);
	if (screen)
		return screen->root;
	return 0;
}

#if USE_XINPUT
static inline void registerMouseEvents(struct obs_core_hotkeys *hotkeys)
{
	obs_hotkeys_platform_t *context = hotkeys->platform_context;
	xcb_connection_t *connection = XGetXCBConnection(context->display);
	xcb_window_t window = root_window(context, connection);

	struct {
		xcb_input_event_mask_t head;
		xcb_input_xi_event_mask_t mask;
	} mask;
	mask.head.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
	mask.head.mask_len = sizeof(mask.mask) / sizeof(uint32_t);
	mask.mask = XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_PRESS |
		    XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_RELEASE;

	xcb_input_xi_select_events(connection, window, 1, &mask.head);
	xcb_flush(connection);
}
#endif

static bool obs_nix_x11_hotkeys_platform_init(struct obs_core_hotkeys *hotkeys)
{
	Display *display = obs_get_nix_platform_display();
	if (!display)
		return false;

	hotkeys->platform_context = bzalloc(sizeof(obs_hotkeys_platform_t));
	hotkeys->platform_context->display = display;

#if USE_XINPUT
	registerMouseEvents(hotkeys);
#endif
	fill_base_keysyms(hotkeys);
	fill_keycodes(hotkeys);
	return true;
}

static void obs_nix_x11_hotkeys_platform_free(struct obs_core_hotkeys *hotkeys)
{
	obs_hotkeys_platform_t *context = hotkeys->platform_context;

	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++)
		da_free(context->keycodes[i].list);

	bfree(context->keysyms);
	bfree(context);

	hotkeys->platform_context = NULL;
}

static bool mouse_button_pressed(xcb_connection_t *connection,
				 obs_hotkeys_platform_t *context, obs_key_t key)
{
	bool ret = false;

#if USE_XINPUT
	memset(context->pressed, 0, XINPUT_MOUSE_LEN);
	memset(context->update, 0, XINPUT_MOUSE_LEN);

	xcb_generic_event_t *ev;
	while ((ev = xcb_poll_for_event(connection))) {
		if ((ev->response_type & ~80) == XCB_GE_GENERIC) {
			switch (((xcb_ge_event_t *)ev)->event_type) {
			case XCB_INPUT_RAW_BUTTON_PRESS: {
				xcb_input_raw_button_press_event_t *mot;
				mot = (xcb_input_raw_button_press_event_t *)ev;
				if (mot->detail < XINPUT_MOUSE_LEN) {
					context->pressed[mot->detail - 1] =
						true;
					context->update[mot->detail - 1] = true;
				} else {
					blog(LOG_WARNING, "Unsupported button");
				}
				break;
			}
			case XCB_INPUT_RAW_BUTTON_RELEASE: {
				xcb_input_raw_button_release_event_t *mot;
				mot = (xcb_input_raw_button_release_event_t *)ev;
				if (mot->detail < XINPUT_MOUSE_LEN)
					context->update[mot->detail - 1] = true;
				else
					blog(LOG_WARNING, "Unsupported button");
				break;
			}
			default:
				break;
			}
		}
		free(ev);
	}

	// Mouse 2 for OBS is Right Click and Mouse 3 is Wheel Click.
	// Mouse Wheel axis clicks (xinput mot->detail 4 5 6 7) are ignored.
	switch (key) {
	case OBS_KEY_MOUSE1:
		ret = context->pressed[0] || context->button_pressed[0];
		break;
	case OBS_KEY_MOUSE2:
		ret = context->pressed[2] || context->button_pressed[2];
		break;
	case OBS_KEY_MOUSE3:
		ret = context->pressed[1] || context->button_pressed[1];
		break;
	case OBS_KEY_MOUSE4:
		ret = context->pressed[7] || context->button_pressed[7];
		break;
	case OBS_KEY_MOUSE5:
		ret = context->pressed[8] || context->button_pressed[8];
		break;
	case OBS_KEY_MOUSE6:
		ret = context->pressed[9] || context->button_pressed[9];
		break;
	case OBS_KEY_MOUSE7:
		ret = context->pressed[10] || context->button_pressed[10];
		break;
	case OBS_KEY_MOUSE8:
		ret = context->pressed[11] || context->button_pressed[11];
		break;
	case OBS_KEY_MOUSE9:
		ret = context->pressed[12] || context->button_pressed[12];
		break;
	case OBS_KEY_MOUSE10:
		ret = context->pressed[13] || context->button_pressed[13];
		break;
	case OBS_KEY_MOUSE11:
		ret = context->pressed[14] || context->button_pressed[14];
		break;
	case OBS_KEY_MOUSE12:
		ret = context->pressed[15] || context->button_pressed[15];
		break;
	case OBS_KEY_MOUSE13:
		ret = context->pressed[16] || context->button_pressed[16];
		break;
	case OBS_KEY_MOUSE14:
		ret = context->pressed[17] || context->button_pressed[17];
		break;
	case OBS_KEY_MOUSE15:
		ret = context->pressed[18] || context->button_pressed[18];
		break;
	case OBS_KEY_MOUSE16:
		ret = context->pressed[19] || context->button_pressed[19];
		break;
	case OBS_KEY_MOUSE17:
		ret = context->pressed[20] || context->button_pressed[20];
		break;
	case OBS_KEY_MOUSE18:
		ret = context->pressed[21] || context->button_pressed[21];
		break;
	case OBS_KEY_MOUSE19:
		ret = context->pressed[22] || context->button_pressed[22];
		break;
	case OBS_KEY_MOUSE20:
		ret = context->pressed[23] || context->button_pressed[23];
		break;
	case OBS_KEY_MOUSE21:
		ret = context->pressed[24] || context->button_pressed[24];
		break;
	case OBS_KEY_MOUSE22:
		ret = context->pressed[25] || context->button_pressed[25];
		break;
	case OBS_KEY_MOUSE23:
		ret = context->pressed[26] || context->button_pressed[26];
		break;
	case OBS_KEY_MOUSE24:
		ret = context->pressed[27] || context->button_pressed[27];
		break;
	case OBS_KEY_MOUSE25:
		ret = context->pressed[28] || context->button_pressed[28];
		break;
	case OBS_KEY_MOUSE26:
		ret = context->pressed[29] || context->button_pressed[29];
		break;
	case OBS_KEY_MOUSE27:
		ret = context->pressed[30] || context->button_pressed[30];
		break;
	case OBS_KEY_MOUSE28:
		ret = context->pressed[31] || context->button_pressed[31];
		break;
	case OBS_KEY_MOUSE29:
		ret = context->pressed[32] || context->button_pressed[32];
		break;
	default:
		break;
	}

	for (int i = 0; i != XINPUT_MOUSE_LEN; i++)
		if (context->update[i])
			context->button_pressed[i] = context->pressed[i];
#else
	xcb_generic_error_t *error = NULL;
	xcb_query_pointer_cookie_t qpc;
	xcb_query_pointer_reply_t *reply;

	qpc = xcb_query_pointer(connection, root_window(context, connection));
	reply = xcb_query_pointer_reply(connection, qpc, &error);

	if (error) {
		blog(LOG_WARNING, "xcb_query_pointer_reply failed");
	} else {
		uint16_t buttons = reply->mask;

		switch (key) {
		case OBS_KEY_MOUSE1:
			ret = buttons & XCB_BUTTON_MASK_1;
			break;
		case OBS_KEY_MOUSE2:
			ret = buttons & XCB_BUTTON_MASK_3;
			break;
		case OBS_KEY_MOUSE3:
			ret = buttons & XCB_BUTTON_MASK_2;
			break;
		default:;
		}
	}

	free(reply);
	free(error);
#endif
	return ret;
}

static inline bool keycode_pressed(xcb_query_keymap_reply_t *reply,
				   xcb_keycode_t code)
{
	return (reply->keys[code / 8] & (1 << (code % 8))) != 0;
}

static bool key_pressed(xcb_connection_t *connection,
			obs_hotkeys_platform_t *context, obs_key_t key)
{
	struct keycode_list *codes = &context->keycodes[key];
	xcb_generic_error_t *error = NULL;
	xcb_query_keymap_reply_t *reply;
	bool pressed = false;

	reply = xcb_query_keymap_reply(connection, xcb_query_keymap(connection),
				       &error);
	if (error) {
		blog(LOG_WARNING, "xcb_query_keymap failed");

	} else if (key == OBS_KEY_META) {
		pressed = keycode_pressed(reply, context->super_l_code) ||
			  keycode_pressed(reply, context->super_r_code);

	} else {
		for (size_t i = 0; i < codes->list.num; i++) {
			if (keycode_pressed(reply, codes->list.array[i])) {
				pressed = true;
				break;
			}
		}
	}

	free(reply);
	free(error);
	return pressed;
}

static bool
obs_nix_x11_hotkeys_platform_is_pressed(obs_hotkeys_platform_t *context,
					obs_key_t key)
{
	xcb_connection_t *conn = XGetXCBConnection(context->display);

	if (key >= OBS_KEY_MOUSE1 && key <= OBS_KEY_MOUSE29) {
		return mouse_button_pressed(conn, context, key);
	} else {
		return key_pressed(conn, context, key);
	}
}

static bool get_key_translation(struct dstr *dstr, xcb_keycode_t keycode)
{
	xcb_connection_t *connection;
	char name[128];

	connection = XGetXCBConnection(obs->hotkeys.platform_context->display);

	XKeyEvent event = {0};
	event.type = KeyPress;
	event.display = obs->hotkeys.platform_context->display;
	event.keycode = keycode;
	event.root = root_window(obs->hotkeys.platform_context, connection);
	event.window = event.root;

	if (keycode) {
		int len = XLookupString(&event, name, 128, NULL, NULL);
		if (len) {
			dstr_ncopy(dstr, name, len);
			dstr_to_upper(dstr);
			return true;
		}
	}

	return false;
}

static void obs_nix_x11_key_to_str(obs_key_t key, struct dstr *dstr)
{
	if (key >= OBS_KEY_MOUSE1 && key <= OBS_KEY_MOUSE29) {
		if (obs->hotkeys.translations[key]) {
			dstr_copy(dstr, obs->hotkeys.translations[key]);
		} else {
			dstr_printf(dstr, "Mouse %d",
				    (int)(key - OBS_KEY_MOUSE1 + 1));
		}
		return;
	}

	if (key >= OBS_KEY_NUM0 && key <= OBS_KEY_NUM9) {
		if (obs->hotkeys.translations[key]) {
			dstr_copy(dstr, obs->hotkeys.translations[key]);
		} else {
			dstr_printf(dstr, "Numpad %d",
				    (int)(key - OBS_KEY_NUM0));
		}
		return;
	}

#define translate_key(key, def) \
	dstr_copy(dstr, obs_get_hotkey_translation(key, def))

	switch (key) {
	case OBS_KEY_INSERT:
		return translate_key(key, "Insert");
	case OBS_KEY_DELETE:
		return translate_key(key, "Delete");
	case OBS_KEY_HOME:
		return translate_key(key, "Home");
	case OBS_KEY_END:
		return translate_key(key, "End");
	case OBS_KEY_PAGEUP:
		return translate_key(key, "Page Up");
	case OBS_KEY_PAGEDOWN:
		return translate_key(key, "Page Down");
	case OBS_KEY_NUMLOCK:
		return translate_key(key, "Num Lock");
	case OBS_KEY_SCROLLLOCK:
		return translate_key(key, "Scroll Lock");
	case OBS_KEY_CAPSLOCK:
		return translate_key(key, "Caps Lock");
	case OBS_KEY_BACKSPACE:
		return translate_key(key, "Backspace");
	case OBS_KEY_TAB:
		return translate_key(key, "Tab");
	case OBS_KEY_PRINT:
		return translate_key(key, "Print");
	case OBS_KEY_PAUSE:
		return translate_key(key, "Pause");
	case OBS_KEY_LEFT:
		return translate_key(key, "Left");
	case OBS_KEY_RIGHT:
		return translate_key(key, "Right");
	case OBS_KEY_UP:
		return translate_key(key, "Up");
	case OBS_KEY_DOWN:
		return translate_key(key, "Down");
	case OBS_KEY_SHIFT:
		return translate_key(key, "Shift");
	case OBS_KEY_ALT:
		return translate_key(key, "Alt");
	case OBS_KEY_CONTROL:
		return translate_key(key, "Control");
	case OBS_KEY_META:
		return translate_key(key, "Super");
	case OBS_KEY_MENU:
		return translate_key(key, "Menu");
	case OBS_KEY_NUMASTERISK:
		return translate_key(key, "Numpad *");
	case OBS_KEY_NUMPLUS:
		return translate_key(key, "Numpad +");
	case OBS_KEY_NUMCOMMA:
		return translate_key(key, "Numpad ,");
	case OBS_KEY_NUMPERIOD:
		return translate_key(key, "Numpad .");
	case OBS_KEY_NUMSLASH:
		return translate_key(key, "Numpad /");
	case OBS_KEY_SPACE:
		return translate_key(key, "Space");
	case OBS_KEY_ESCAPE:
		return translate_key(key, "Escape");
	default:;
	}

	if (key >= OBS_KEY_F1 && key <= OBS_KEY_F35) {
		dstr_printf(dstr, "F%d", (int)(key - OBS_KEY_F1 + 1));
		return;
	}

	obs_hotkeys_platform_t *context = obs->hotkeys.platform_context;
	struct keycode_list *keycodes = &context->keycodes[key];

	for (size_t i = 0; i < keycodes->list.num; i++) {
		if (get_key_translation(dstr, keycodes->list.array[i])) {
			break;
		}
	}

	if (key != OBS_KEY_NONE && dstr_is_empty(dstr)) {
		dstr_copy(dstr, obs_key_to_name(key));
	}
}

static obs_key_t key_from_keycode(obs_hotkeys_platform_t *context,
				  xcb_keycode_t code)
{
	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++) {
		struct keycode_list *codes = &context->keycodes[i];

		for (size_t j = 0; j < codes->list.num; j++) {
			if (codes->list.array[j] == code) {
				return (obs_key_t)i;
			}
		}
	}

	return OBS_KEY_NONE;
}

static obs_key_t obs_nix_x11_key_from_virtual_key(int sym)
{
	obs_hotkeys_platform_t *context = obs->hotkeys.platform_context;
	const xcb_keysym_t *keysyms = context->keysyms;
	int syms_per_code = context->syms_per_code;
	int num_keysyms = context->num_keysyms;

	if (sym == 0)
		return OBS_KEY_NONE;

	for (int i = 0; i < num_keysyms; i++) {
		if (keysyms[i] == (xcb_keysym_t)sym) {
			xcb_keycode_t code = (xcb_keycode_t)(i / syms_per_code);
			code += context->min_keycode;
			obs_key_t key = key_from_keycode(context, code);

			return key;
		}
	}

	return OBS_KEY_NONE;
}

static int obs_nix_x11_key_to_virtual_key(obs_key_t key)
{
	if (key == OBS_KEY_META)
		return XK_Super_L;

	return (int)obs->hotkeys.platform_context->base_keysyms[(int)key];
}

static const struct obs_nix_hotkeys_vtable x11_hotkeys_vtable = {
	.init = obs_nix_x11_hotkeys_platform_init,
	.free = obs_nix_x11_hotkeys_platform_free,
	.is_pressed = obs_nix_x11_hotkeys_platform_is_pressed,
	.key_to_str = obs_nix_x11_key_to_str,
	.key_from_virtual_key = obs_nix_x11_key_from_virtual_key,
	.key_to_virtual_key = obs_nix_x11_key_to_virtual_key,
};

const struct obs_nix_hotkeys_vtable *obs_nix_x11_get_hotkeys_vtable(void)
{
	return &x11_hotkeys_vtable;
}
