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

/* TODO: incomplete/may not be necessary */

#ifdef __cplusplus
extern "C" {
#endif

#define KBC_ESCAPE 0x0
#define KBC_1 0x1
#define KBC_2 0x2
#define KBC_3 0x3
#define KBC_4 0x4
#define KBC_5 0x5
#define KBC_6 0x6
#define KBC_7 0x7
#define KBC_8 0x8
#define KBC_9 0x9
#define KBC_0 0xA
#define KBC_MINUS 0xB
#define KBC_EQUALS 0xC
#define KBC_BACK 0xD
#define KBC_TAB 0xE
#define KBC_Q 0xF
#define KBC_W 0x10
#define KBC_E 0x11
#define KBC_R 0x12
#define KBC_T 0x13
#define KBC_Y 0x14
#define KBC_U 0x15
#define KBC_I 0x16
#define KBC_O 0x17
#define KBC_P 0x18
#define KBC_LBRACKET 0x19
#define KBC_RBRACKET 0x1A
#define KBC_RETURN 0x1B
#define KBC_LCONTROL 0x1C
#define KBC_A 0x1D
#define KBC_S 0x1E
#define KBC_D 0x1F
#define KBC_F 0x20
#define KBC_G 0x21
#define KBC_H 0x22
#define KBC_J 0x23
#define KBC_K 0x24
#define KBC_L 0x25
#define KBC_SEMICOLON 0x26
#define KBC_APOSTROPHE 0x27
#define KBC_TILDE 0x28
#define KBC_LSHIFT 0x29
#define KBC_BACKSLASH 0x2A
#define KBC_Z 0x2B
#define KBC_X 0x2C
#define KBC_C 0x2D
#define KBC_V 0x2E
#define KBC_B 0x2F
#define KBC_N 0x30
#define KBC_M 0x31
#define KBC_COMMA 0x32
#define KBC_PERIOD 0x33
#define KBC_SLASH 0x34
#define KBC_RSHIFT 0x35
#define KBC_MULTIPLY 0x36
#define KBC_LALT 0x37
#define KBC_SPACE 0x38
#define KBC_CAPSLOCK 0x39
#define KBC_F1 0x3A
#define KBC_F2 0x3B
#define KBC_F3 0x3C
#define KBC_F4 0x3D
#define KBC_F5 0x3E
#define KBC_F6 0x3F
#define KBC_F7 0x40
#define KBC_F8 0x41
#define KBC_F9 0x42
#define KBC_F10 0x43
#define KBC_NUMLOCK 0x44
#define KBC_SCROLLLOCK 0x45
#define KBC_NUMPAD7 0x46
#define KBC_NUMPAD8 0x47
#define KBC_NUMPAD9 0x48
#define KBC_SUBTRACT 0x49
#define KBC_NUMPAD4 0x4A
#define KBC_NUMPAD5 0x4B
#define KBC_NUMPAD6 0x4C
#define KBC_ADD 0x4D
#define KBC_NUMPAD1 0x4E
#define KBC_NUMPAD2 0x4F
#define KBC_NUMPAD3 0x50
#define KBC_NUMPAD0 0x51
#define KBC_DECIMAL 0x52
#define KBC_F11 0x53
#define KBC_F12 0x54
#define KBC_NUMPADENTER 0x55
#define KBC_RCONTROL 0x56
#define KBC_DIVIDE 0x57
#define KBC_SYSRQ 0x58
#define KBC_RALT 0x59
#define KBC_PAUSE 0x5A
#define KBC_HOME 0x5B
#define KBC_UP 0x5C
#define KBC_PAGEDOWN 0x5D
#define KBC_LEFT 0x5E
#define KBC_RIGHT 0x5F
#define KBC_END 0x60
#define KBC_DOWN 0x61
#define KBC_PAGEUP 0x62
#define KBC_INSERT 0x63
#define KBC_DELETE 0x64

#define MOUSE_LEFTBUTTON 0x65
#define MOUSE_MIDDLEBUTTON 0x66
#define MOUSE_RIGHTBUTTON 0x67
#define MOUSE_WHEEL 0x68
#define MOUSE_MOVE 0x69

#define KBC_CONTROL 0xFFFFFFFE
#define KBC_ALT 0xFFFFFFFD
#define KBC_SHIFT 0xFFFFFFFC

#define STATE_LBUTTONDOWN (1 << 0)
#define STATE_RBUTTONDOWN (1 << 1)
#define STATE_MBUTTONDOWN (1 << 2)
#define STATE_X4BUTTONDOWN (1 << 3)
#define STATE_X5BUTTONDOWN (1 << 4)

/* wrapped opaque data types */
struct input_subsystem;
typedef struct input_subsystem input_t;

EXPORT int input_getbuttonstate(input_t *input, uint32_t button);

#ifdef __cplusplus
}
#endif
