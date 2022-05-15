#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long long i64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef i32 b32;

#define global static
#define internal static

#define OFFSET_PTR_BYTES(type, ptr, offset) ((type*)((u8*)ptr + (offset)))

#define KEY_SPACE 32
#define KEY_COMMA 188
#define KEY_MINUS 189
#define KEY_PERIOD 190
#define KEY_D0 48
#define KEY_D1 49
#define KEY_D2 50
#define KEY_D3 51
#define KEY_D4 52
#define KEY_D5 53
#define KEY_D6 54
#define KEY_D7 55
#define KEY_D8 56
#define KEY_D9 57
#define KEY_SEMICOLON 186
#define KEY_A 65
#define KEY_B 66
#define KEY_C 67
#define KEY_D 68
#define KEY_E 69
#define KEY_F 70
#define KEY_G 71
#define KEY_H 72
#define KEY_I 73
#define KEY_J 74
#define KEY_K 75
#define KEY_L 76
#define KEY_M 77
#define KEY_N 78
#define KEY_O 79
#define KEY_P 80
#define KEY_Q 81
#define KEY_R 82
#define KEY_S 83
#define KEY_T 84
#define KEY_U 85
#define KEY_V 86
#define KEY_W 87
#define KEY_X 88
#define KEY_Y 89
#define KEY_Z 90
#define KEY_LEFTBRACKET 219
#define KEY_BACKSLASH 220
#define KEY_RIGHTBRACKENT 221
#define KEY_GRAVEACCENT 222
#define KEY_BACKSPACE 8
#define KEY_ENTER 13
#define KEY_TAB 9
#define KEY_PAUSE 19
#define KEY_NUMLOCK 144
#define KEY_SCROLLLOCK 145
#define KEY_CAPSLOCK 20
#define KEY_ESCAPE 27
#define KEY_PAGEUP 33
#define KEY_PAGEDOWN 34
#define KEY_END 35
#define KEY_HOME 36
#define KEY_LEFT 37
#define KEY_UP 38
#define KEY_RIGHT 39
#define KEY_DOWN 40
#define KEY_PRINTSCREEN 44
#define KEY_INSERT 45
#define KEY_DELETE 46
#define KEY_F1 112
#define KEY_F2 113
#define KEY_F3 114
#define KEY_F4 115
#define KEY_F5 116
#define KEY_F6 117
#define KEY_F7 118
#define KEY_F8 119
#define KEY_F9 120
#define KEY_F10 121
#define KEY_F11 122
#define KEY_F12 123
#define KEY_F13 124
#define KEY_F14 125
#define KEY_F15 126
#define KEY_F16 127
#define KEY_F17 128
#define KEY_F18 129
#define KEY_F19 130
#define KEY_F20 131
#define KEY_F21 132
#define KEY_F22 133
#define KEY_F23 134
#define KEY_F24 135
#define KEY_KP0 96
#define KEY_KP1 97
#define KEY_KP2 98
#define KEY_KP3 99
#define KEY_KP4 100
#define KEY_KP5 101
#define KEY_KP6 102
#define KEY_KP7 103
#define KEY_KP8 104
#define KEY_KP9 105
#define KEY_KPMULTIPLY 106
#define KEY_KPADD 107
#define KEY_KPEQUAL 108
#define KEY_KPSUBSTRACT 109
#define KEY_KPDECIMAL 110
#define KEY_KPDIVIDE 111
#define KEY_LEFTSHIFT 160
#define KEY_RIGHTSHIFT 161
#define KEY_LEFTCONTROL 162
#define KEY_RIGHTCONTROL 163
#define KEY_LEFTALT 164
#define KEY_RIGHTALT 165
#define MOUSE_LEFT 1
#define MOUSE_RIGHT 2
#define MOUSE_MIDDLE 3

#endif