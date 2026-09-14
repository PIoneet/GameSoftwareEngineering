#pragma once

// GLUT 키보드 콜백은 이벤트 단발이라 "동시에 눌린 두 키"나 "누르고 있는 중"을
// 표현하지 못한다. 콜백에서는 상태 배열만 갱신하고, 게임 로직은 매 프레임
// 이 상태를 폴링한다.
class Input
{
public:
	static void Reset();

	// GLUT 콜백에서 호출
	static void OnKeyDown(unsigned char key);
	static void OnKeyUp(unsigned char key);

	// 매 프레임 Update 뒤에 한 번 호출 — 눌린 프레임 플래그를 지운다.
	static void EndFrame();

	static bool Held(unsigned char key);      // 누르고 있는 동안 계속 true
	static bool Pressed(unsigned char key);   // 눌린 그 프레임에만 true

private:
	static unsigned char Normalize(unsigned char key);

	static bool s_Held[256];
	static bool s_Pressed[256];
};
