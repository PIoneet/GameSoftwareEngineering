/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <iostream>
#include <cstdio>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"
#include "Input.h"
#include "Game.h"
#include "Profiler.h"

static const int WINDOW_W = 1280;
static const int WINDOW_H = 720;

Renderer* g_Renderer = NULL;
Game*     g_Game     = NULL;

static int   s_PrevTimeMs = 0;
static float s_LastDt     = 0.0f;

void RenderScene(void)
{
	// GPU 질의는 실제 GL 명령만 감싸야 한다.
	Prof::GpuFrameBegin();

	g_Game->Render();

	Prof::GpuFrameEnd();

	glutSwapBuffers();

	// 계측은 UI 까지 다 그린 뒤 닫아야 한 프레임 전체가 잡힌다.
	Prof::EndFrame(s_LastDt);
}

void Idle(void)
{
	// 실시간 게임의 심장. 그리기만 반복하던 기존 구조와 달리
	// 경과 시간을 재서 Update 와 Render 를 분리한다.
	Prof::BeginFrame();

	int   now = glutGet(GLUT_ELAPSED_TIME);
	float dt  = (float)(now - s_PrevTimeMs) / 1000.0f;
	s_PrevTimeMs = now;

	// 창을 끌거나 멈췄다 돌아오면 dt 가 크게 튄다. 한 프레임 분량으로 자른다.
	if (dt > 0.1f)
		dt = 0.1f;

	s_LastDt = dt;

	g_Game->Update(dt);
	Input::EndFrame();

	glutPostRedisplay();
}

void Reshape(int w, int h)
{
	if (h < 1) h = 1;
	glViewport(0, 0, w, h);
	g_Renderer->Resize(w, h);
	g_Game->Resize(w, h);
}

void KeyDown(unsigned char key, int x, int y)
{
	if (key == 27)   // Esc
	{
		glutLeaveMainLoop();
		return;
	}

	// P — 프로파일러 출력 on/off
	if (key == 'p' || key == 'P')
	{
		Prof::SetEnabled(!Prof::Enabled());
		printf("\n-- 프로파일러: %s --\n", Prof::Enabled() ? "켜짐" : "꺼짐");
	}

	Input::OnKeyDown(key);
}

void KeyUp(unsigned char key, int x, int y)
{
	Input::OnKeyUp(key);
}

int main(int argc, char **argv)
{
	// 소스는 /utf-8 로 컴파일되므로 narrow 리터럴이 UTF-8 바이트다.
	// 콘솔 기본 코드페이지(한국어 윈도우면 949)로는 한글이 깨진다.
	SetConsoleOutputCP(65001);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(60, 40);
	glutInitWindowSize(WINDOW_W, WINDOW_H);
	glutCreateWindow("Ravenhold - The Name of the Sleeper (Prototype)");

	if (glewInit() != GLEW_OK)
	{
		std::cout << "GLEW initialization failed.\n";
		return -1;
	}
	if (!glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << "OpenGL 3.0 is not supported on this machine.\n";
		return -1;
	}

	g_Renderer = new Renderer(WINDOW_W, WINDOW_H);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized.\n";
		delete g_Renderer;
		return -1;
	}

	// GL 컨텍스트가 선 뒤에야 GPU 타이머 질의를 만들 수 있다.
	Prof::Init();

	g_Game = new Game();
	g_Game->Init(g_Renderer, WINDOW_W, WINDOW_H);

	Input::Reset();
	glutIgnoreKeyRepeat(1);   // 키를 누르고 있어도 KeyDown 이 반복 발생하지 않게

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(KeyDown);
	glutKeyboardUpFunc(KeyUp);

	printf("\n  라벤홀 — 프로토타입\n"
	       "  WASD 이동   E 상호작용   Space 공격   J 일지   C 능력치\n"
	       "  0 마을↔폐허   P 프로파일러 on/off   Esc 종료\n\n");

	s_PrevTimeMs = glutGet(GLUT_ELAPSED_TIME);
	glutMainLoop();

	Prof::Shutdown();

	delete g_Game;
	delete g_Renderer;

	return 0;
}
