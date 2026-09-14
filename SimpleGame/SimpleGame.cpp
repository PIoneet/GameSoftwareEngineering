/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <iostream>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"
#include "Input.h"
#include "Game.h"

static const int WINDOW_W = 1280;
static const int WINDOW_H = 720;

Renderer* g_Renderer = NULL;
Game*     g_Game     = NULL;

static int s_PrevTimeMs = 0;

void RenderScene(void)
{
	g_Game->Render();
	glutSwapBuffers();
}

void Idle(void)
{
	// 실시간 게임의 심장. 그리기만 반복하던 기존 구조와 달리
	// 경과 시간을 재서 Update 와 Render 를 분리한다.
	int   now = glutGet(GLUT_ELAPSED_TIME);
	float dt  = (float)(now - s_PrevTimeMs) / 1000.0f;
	s_PrevTimeMs = now;

	// 창을 끌거나 멈췄다 돌아오면 dt 가 크게 튄다. 한 프레임 분량으로 자른다.
	if (dt > 0.1f)
		dt = 0.1f;

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
	Input::OnKeyDown(key);
}

void KeyUp(unsigned char key, int x, int y)
{
	Input::OnKeyUp(key);
}

int main(int argc, char **argv)
{
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

	g_Game = new Game();
	g_Game->Init(g_Renderer, WINDOW_W, WINDOW_H);

	Input::Reset();
	glutIgnoreKeyRepeat(1);   // 키를 누르고 있어도 KeyDown 이 반복 발생하지 않게

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(KeyDown);
	glutKeyboardUpFunc(KeyUp);

	std::cout << "\n  Ravenhold - prototype\n"
	          << "  WASD move   E interact   J journal   Esc quit\n\n";

	s_PrevTimeMs = glutGet(GLUT_ELAPSED_TIME);
	glutMainLoop();

	delete g_Game;
	delete g_Renderer;

	return 0;
}
