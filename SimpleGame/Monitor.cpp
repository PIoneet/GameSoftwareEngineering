#include "stdafx.h"
#include "Monitor.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace
{
	bool   s_Running = false;
	int    s_Port    = 0;
	SOCKET s_Listen  = INVALID_SOCKET;

	std::string s_Snapshot = "{}";

	// 대시보드는 디스크에서 읽는다. 셰이더와 같은 방식이라 고칠 때
	// 다시 빌드하지 않아도 된다 — 새로고침이면 끝이다.
	//
	// 실행 위치가 디버거냐 탐색기냐에 따라 작업 디렉터리가 달라서,
	// 셰이더와 똑같이 후보를 차례로 훑는다.
	const char* HTML_CANDIDATES[] =
	{
		"./monitor.html",
		"./SimpleGame/monitor.html",
		"../SimpleGame/monitor.html",
		"../../SimpleGame/monitor.html",
	};

	void CloseSocket(SOCKET& s)
	{
		if (s != INVALID_SOCKET)
		{
			closesocket(s);
			s = INVALID_SOCKET;
		}
	}

	void SendAll(SOCKET s, const char* data, int len)
	{
		int sent = 0;

		while (sent < len)
		{
			int n = send(s, data + sent, len - sent, 0);

			// 논블로킹이라 버퍼가 차면 WOULDBLOCK 이 난다. 로컬 상대라
			// 잠깐 양보하면 곧 빠진다. 그 외 오류면 포기한다.
			if (n == SOCKET_ERROR)
			{
				if (WSAGetLastError() == WSAEWOULDBLOCK)
					continue;

				return;
			}

			sent += n;
		}
	}

	void Respond(SOCKET s, const char* status, const char* type, const std::string& body)
	{
		char head[320];

		int n = _snprintf_s(head, sizeof(head), _TRUNCATE,
		                    "HTTP/1.1 %s\r\n"
		                    "Content-Type: %s\r\n"
		                    "Content-Length: %d\r\n"
		                    "Cache-Control: no-store\r\n"
		                    "Connection: close\r\n"
		                    "\r\n",
		                    status, type, (int)body.size());

		SendAll(s, head, n);
		SendAll(s, body.data(), (int)body.size());
	}

	std::string ReadTextFile(const char* path)
	{
		std::ifstream f(path, std::ios::binary);

		if (!f)
			return std::string();

		std::ostringstream ss;
		ss << f.rdbuf();

		return ss.str();
	}

	void Serve(SOCKET client)
	{
		// 로컬 상대라 요청은 한 번에 들어온다. 그래도 짧은 제한 시간을 걸어
		// 이상한 클라이언트가 프레임을 붙잡지 못하게 한다.
		DWORD timeout = 50;
		setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

		char req[1024];
		int  n = recv(client, req, sizeof(req) - 1, 0);

		if (n <= 0)
			return;

		req[n] = '\0';

		// "GET /path HTTP/1.1" 에서 경로만 떼 온다.
		const char* p = strchr(req, ' ');
		std::string path = "/";

		if (p != 0)
		{
			++p;

			const char* q = strchr(p, ' ');

			if (q != 0)
				path.assign(p, q - p);
		}

		if (path == "/state.json")
		{
			Respond(client, "200 OK", "application/json; charset=utf-8", s_Snapshot);

			return;
		}

		if (path == "/" || path == "/index.html" || path == "/monitor.html")
		{
			std::string html;

			for (int i = 0; i < (int)(sizeof(HTML_CANDIDATES) / sizeof(HTML_CANDIDATES[0])); ++i)
			{
				html = ReadTextFile(HTML_CANDIDATES[i]);

				if (!html.empty())
					break;
			}

			if (html.empty())
			{
				Respond(client, "500 Internal Server Error", "text/plain; charset=utf-8",
				        "monitor.html 을 찾지 못했습니다. 실행 파일 옆에 두세요.");
			}
			else
			{
				Respond(client, "200 OK", "text/html; charset=utf-8", html);
			}

			return;
		}

		Respond(client, "404 Not Found", "text/plain; charset=utf-8", "not found");
	}
}

bool Monitor::Start(int port)
{
	if (s_Running)
		return true;

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	s_Listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s_Listen == INVALID_SOCKET)
	{
		WSACleanup();
		return false;
	}

	// 껐다 바로 켤 때 "이미 쓰는 중" 으로 막히지 않게.
	BOOL reuse = TRUE;
	setsockopt(s_Listen, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port   = htons((u_short)port);

	// 루프백에만 연다. INADDR_ANY 로 열면 같은 공유기의 아무 기기나 들여다본다.
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(s_Listen, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
	    listen(s_Listen, 8) == SOCKET_ERROR)
	{
		CloseSocket(s_Listen);
		WSACleanup();

		return false;
	}

	u_long nonblocking = 1;
	ioctlsocket(s_Listen, FIONBIO, &nonblocking);

	s_Running = true;
	s_Port    = port;

	printf("[MONITOR] http://127.0.0.1:%d  — 브라우저로 열면 상태판이 뜹니다\n", port);

	return true;
}

void Monitor::Stop()
{
	if (!s_Running)
		return;

	CloseSocket(s_Listen);
	WSACleanup();

	s_Running = false;
}

bool Monitor::Running() { return s_Running; }
int  Monitor::Port()    { return s_Port; }

void Monitor::SetSnapshot(const char* json)
{
	if (json != 0)
		s_Snapshot = json;
}

void Monitor::Poll()
{
	if (!s_Running)
		return;

	// 한 프레임에 몇 건까지만 받는다. 브라우저가 쏟아부어도
	// 게임 루프가 거기 묶이지 않게.
	for (int i = 0; i < 4; ++i)
	{
		SOCKET client = accept(s_Listen, 0, 0);

		if (client == INVALID_SOCKET)
			return;

		Serve(client);

		closesocket(client);
	}
}
