#include "stdafx.h"
#include "Input.h"

bool Input::s_Held[256]    = { false };
bool Input::s_Pressed[256] = { false };

unsigned char Input::Normalize(unsigned char key)
{
	// Shift 상태와 무관하게 같은 키로 취급한다.
	if (key >= 'A' && key <= 'Z')
		return (unsigned char)(key - 'A' + 'a');
	return key;
}

void Input::Reset()
{
	for (int i = 0; i < 256; ++i)
	{
		s_Held[i]    = false;
		s_Pressed[i] = false;
	}
}

void Input::OnKeyDown(unsigned char key)
{
	unsigned char k = Normalize(key);

	// glutIgnoreKeyRepeat(1) 을 걸어두었지만, 만약을 대비해
	// 이미 눌려 있으면 Pressed 를 다시 세우지 않는다.
	if (!s_Held[k])
		s_Pressed[k] = true;

	s_Held[k] = true;
}

void Input::OnKeyUp(unsigned char key)
{
	s_Held[Normalize(key)] = false;
}

void Input::EndFrame()
{
	for (int i = 0; i < 256; ++i)
		s_Pressed[i] = false;
}

bool Input::Held(unsigned char key)
{
	return s_Held[Normalize(key)];
}

bool Input::Pressed(unsigned char key)
{
	return s_Pressed[Normalize(key)];
}
