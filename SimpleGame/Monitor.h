#pragma once

// 로컬 모니터링 서버.
//
// 게임이 매 초 JSON 스냅샷을 만들어 넘기면, 여기서 작은 HTTP 서버가
// 브라우저에 그대로 흘려 준다. 대시보드(monitor.html)는 그 JSON 만 읽는다.
//
// 설계상 지키는 것:
//  - **127.0.0.1 에만 바인딩한다.** 같은 네트워크의 다른 기기에 열리면 안 된다.
//  - **게임 루프를 멈추지 않는다.** 소켓은 논블로킹이고, 프레임당 한 번만 훑는다.
//    받을 게 없으면 즉시 돌아온다.
//  - **실패해도 게임은 돈다.** 포트가 막혀 있으면 조용히 꺼진 채로 둔다.
//
// Windows 타입은 .cpp 안에 가둔다 — winsock2.h 가 다른 번역 단위로 새면
// windows.h 매크로 사고(CLAUDE.md §4-5)가 또 난다.

namespace Monitor
{
	// 실패하면 false. 게임은 그대로 진행한다.
	bool Start(int port = 8777);
	void Stop();

	bool Running();
	int  Port();

	// 최신 스냅샷을 갈아 끼운다. 호출한 쪽의 버퍼는 여기서 복사한다.
	void SetSnapshot(const char* json);

	// 대기 중인 요청을 처리한다. 매 프레임 한 번 부른다.
	void Poll();
}
