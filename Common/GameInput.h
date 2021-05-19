#pragma once
#include "Windows.h"

enum INPUT_KEY
{
	// Keyboard : Num
	KEYBOARD_1 = 0,
	KEYBOARD_2 = KEYBOARD_1 + 1,
	KEYBOARD_3 = KEYBOARD_2 + 1,
	KEYBOARD_4 = KEYBOARD_3 + 1,
	KEYBOARD_5 = KEYBOARD_4 + 1,
	KEYBOARD_6 = KEYBOARD_5 + 1,
	KEYBOARD_7 = KEYBOARD_6 + 1,
	KEYBOARD_8 = KEYBOARD_7 + 1,
	KEYBOARD_9 = KEYBOARD_8 + 1,
	KEYBOARD_0 = KEYBOARD_9 + 1,

	// Keyboard : En
	KEYBOARD_A = KEYBOARD_0 + 1,
	KEYBOARD_B = KEYBOARD_A + 1,
	KEYBOARD_C = KEYBOARD_B + 1,
	KEYBOARD_D = KEYBOARD_C + 1,
	KEYBOARD_E = KEYBOARD_D + 1,
	KEYBOARD_F = KEYBOARD_E + 1,
	KEYBOARD_G = KEYBOARD_F + 1,
	KEYBOARD_H = KEYBOARD_G + 1,
	KEYBOARD_I = KEYBOARD_H + 1,
	KEYBOARD_J = KEYBOARD_I + 1,
	KEYBOARD_K = KEYBOARD_J + 1,
	KEYBOARD_L = KEYBOARD_K + 1,
	KEYBOARD_M = KEYBOARD_L + 1,
	KEYBOARD_N = KEYBOARD_M + 1,
	KEYBOARD_O = KEYBOARD_N + 1,
	KEYBOARD_P = KEYBOARD_O + 1,
	KEYBOARD_Q = KEYBOARD_P + 1,
	KEYBOARD_R = KEYBOARD_Q + 1,
	KEYBOARD_S = KEYBOARD_R + 1,
	KEYBOARD_T = KEYBOARD_S + 1,
	KEYBOARD_U = KEYBOARD_T + 1,
	KEYBOARD_V = KEYBOARD_U + 1,
	KEYBOARD_W = KEYBOARD_V + 1,
	KEYBOARD_X = KEYBOARD_W + 1,
	KEYBOARD_Y = KEYBOARD_X + 1,
	KEYBOARD_Z = KEYBOARD_Y + 1,

	// Keyboard : F
	KEYBOARD_F1 = KEYBOARD_Z + 1,
	KEYBOARD_F2 = KEYBOARD_F1 + 1,
	KEYBOARD_F3 = KEYBOARD_F2 + 1,
	KEYBOARD_F4 = KEYBOARD_F3 + 1,
	KEYBOARD_F5 = KEYBOARD_F4 + 1,
	KEYBOARD_F6 = KEYBOARD_F5 + 1,
	KEYBOARD_F7 = KEYBOARD_F6 + 1,
	KEYBOARD_F8 = KEYBOARD_F7 + 1,
	KEYBOARD_F9 = KEYBOARD_F8 + 1,
	KEYBOARD_F10 = KEYBOARD_F9 + 1,
	KEYBOARD_F11 = KEYBOARD_F10 + 1,
	KEYBOARD_F12 = KEYBOARD_F11 + 1,

	ENUM_SIZE = KEYBOARD_F12 + 1,
};

class GameInput
{
public:
	static bool IsKeyPressed(INPUT_KEY key)
	{
		switch (key)
		{
		case KEYBOARD_1: return GetAsyncKeyState('1') & 0x8000;
		case KEYBOARD_2: return GetAsyncKeyState('2') & 0x8000;
		case KEYBOARD_3: return GetAsyncKeyState('3') & 0x8000;
		case KEYBOARD_4: return GetAsyncKeyState('4') & 0x8000;
		case KEYBOARD_5: return GetAsyncKeyState('5') & 0x8000;
		case KEYBOARD_6: return GetAsyncKeyState('6') & 0x8000;
		case KEYBOARD_7: return GetAsyncKeyState('7') & 0x8000;
		case KEYBOARD_8: return GetAsyncKeyState('8') & 0x8000;
		case KEYBOARD_9: return GetAsyncKeyState('9') & 0x8000;
		case KEYBOARD_0: return GetAsyncKeyState('0') & 0x8000;
		case KEYBOARD_A: return GetAsyncKeyState('A') & 0x8000;
		case KEYBOARD_B: return GetAsyncKeyState('B') & 0x8000;
		case KEYBOARD_C: return GetAsyncKeyState('C') & 0x8000;
		case KEYBOARD_D: return GetAsyncKeyState('D') & 0x8000;
		case KEYBOARD_E: return GetAsyncKeyState('E') & 0x8000;
		case KEYBOARD_F: return GetAsyncKeyState('F') & 0x8000;
		case KEYBOARD_G: return GetAsyncKeyState('G') & 0x8000;
		case KEYBOARD_H: return GetAsyncKeyState('H') & 0x8000;
		case KEYBOARD_I: return GetAsyncKeyState('I') & 0x8000;
		case KEYBOARD_J: return GetAsyncKeyState('J') & 0x8000;
		case KEYBOARD_K: return GetAsyncKeyState('K') & 0x8000;
		case KEYBOARD_L: return GetAsyncKeyState('L') & 0x8000;
		case KEYBOARD_M: return GetAsyncKeyState('M') & 0x8000;
		case KEYBOARD_N: return GetAsyncKeyState('N') & 0x8000;
		case KEYBOARD_O: return GetAsyncKeyState('O') & 0x8000;
		case KEYBOARD_P: return GetAsyncKeyState('P') & 0x8000;
		case KEYBOARD_Q: return GetAsyncKeyState('Q') & 0x8000;
		case KEYBOARD_R: return GetAsyncKeyState('R') & 0x8000;
		case KEYBOARD_S: return GetAsyncKeyState('S') & 0x8000;
		case KEYBOARD_T: return GetAsyncKeyState('T') & 0x8000;
		case KEYBOARD_U: return GetAsyncKeyState('U') & 0x8000;
		case KEYBOARD_V: return GetAsyncKeyState('V') & 0x8000;
		case KEYBOARD_W: return GetAsyncKeyState('W') & 0x8000;
		case KEYBOARD_X: return GetAsyncKeyState('X') & 0x8000;
		case KEYBOARD_Y: return GetAsyncKeyState('Y') & 0x8000;
		case KEYBOARD_Z: return GetAsyncKeyState('Z') & 0x8000;
		case KEYBOARD_F1: return GetAsyncKeyState(VK_F1) & 0x8000;
		case KEYBOARD_F2: return GetAsyncKeyState(VK_F2) & 0x8000;
		case KEYBOARD_F3: return GetAsyncKeyState(VK_F3) & 0x8000;
		case KEYBOARD_F4: return GetAsyncKeyState(VK_F4) & 0x8000;
		case KEYBOARD_F5: return GetAsyncKeyState(VK_F5) & 0x8000;
		case KEYBOARD_F6: return GetAsyncKeyState(VK_F6) & 0x8000;
		case KEYBOARD_F7: return GetAsyncKeyState(VK_F7) & 0x8000;
		case KEYBOARD_F8: return GetAsyncKeyState(VK_F8) & 0x8000;
		case KEYBOARD_F9: return GetAsyncKeyState(VK_F9) & 0x8000;
		case KEYBOARD_F10: return GetAsyncKeyState(VK_F10) & 0x8000;
		case KEYBOARD_F11: return GetAsyncKeyState(VK_F11) & 0x8000;
		case KEYBOARD_F12: return GetAsyncKeyState(VK_F12) & 0x8000;
		}

		return false;
	}
};