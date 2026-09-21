#pragma once

#include <string>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"
#include "Font.h"

enum TextFont
{
	FONT_SMALL,
	FONT_BODY,
	FONT_TITLE
};

// 2D 렌더러. 화면 픽셀 좌표(좌상단 원점)만 받는다.
// 월드 → 스크린 변환은 호출하는 쪽의 책임이다.
//
// 한 프레임의 흐름:
//   BeginScene()  → 게임 월드를 오프스크린 버퍼에 그린다
//   EndScene()
//   PostProcess() → 블룸 + 색보정 + 비네트 + 그레인을 얹어 백버퍼로 내보낸다
//   (UI 는 그 위에 직접 그린다)
class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized() const;
	void Resize(int windowSizeX, int windowSizeY);

	void BeginScene(float r, float g, float b);
	void EndScene();
	void PostProcess(float dread, float time);

	// 축 정렬 사각형. (x, y) 는 좌상단.
	void DrawQuad(float x, float y, float w, float h,
	              float r, float g, float b, float a = 1.0f);

	// 아이소메트릭 지면 타일. (cx, cy) 는 마름모의 중심.
	void DrawDiamond(float cx, float cy, float w, float h,
	                 float r, float g, float b, float a = 1.0f);

	// 원과 타원. 생물 몸통, 구슬, 둥근 창에 쓴다.
	void DrawEllipse(float cx, float cy, float w, float h,
	                 float r, float g, float b, float a = 1.0f);

	// 물 타일. 파동은 전부 셰이더가 그린다.
	// (worldX, worldY) 를 넘겨야 타일 경계에서 물결이 끊기지 않는다.
	void DrawWater(float cx, float cy, float w, float h,
	               float worldX, float worldY, float depth, float lit, float time,
	               float r, float g, float b);

	// 불꽃. 흔들림과 색 그라데이션도 셰이더가 만든다.
	// (cx, cyBase) 는 불꽃의 밑동 중앙.
	void DrawFlame(float cx, float cyBase, float w, float h,
	               float time, float seed, float intensity);

	// (x, y) 는 글자의 베이스라인 좌측.
	// 주의: 이름을 DrawText 로 두면 안 된다. <windows.h> 가 DrawText 를
	//       DrawTextW 매크로로 치환해 버려서, windows.h 를 포함한 번역 단위에서만
	//       멤버 이름이 달라지는 지옥이 열린다.
	void DrawString(float x, float y, const wchar_t* text,
	                float r, float g, float b, float a = 1.0f,
	                TextFont font = FONT_BODY);

	// 글리프를 즉석에서 굽기 때문에 const 가 아니다.
	float TextWidth(const wchar_t* text, TextFont font = FONT_BODY);
	float LineHeight(TextFont font);

	int WindowWidth()  const { return (int)m_WindowSizeX; }
	int WindowHeight() const { return (int)m_WindowSizeY; }

private:
	void   Initialize(int windowSizeX, int windowSizeY);
	bool   ReadShaderFile(const char* filename, std::string* target);
	void   AddShader(GLuint shaderProgram, const char* shaderText, GLenum shaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void   CreateVertexBufferObjects();

	bool   CreateTargets(int w, int h);
	void   DestroyTargets();
	void   DrawFullscreen(GLint attribLocation);

	void   DrawUnitShape(GLuint vbo, int vertexCount, float x, float y, float w, float h,
	                     float r, float g, float b, float a);

	Font&  FontFor(TextFont font);

	bool m_Initialized = false;
	bool m_TargetsOK   = false;

	float m_WindowSizeX = 0.0f;
	float m_WindowSizeY = 0.0f;

	GLuint m_VBOQuad    = 0;   // 단위 사각형
	GLuint m_VBODiamond = 0;   // 단위 마름모
	GLuint m_VBOEllipse = 0;   // 단위 타원 (삼각형 부채꼴)
	int    m_EllipseVerts = 0;

	// ── 씬 셰이더 ──
	GLuint m_SpriteProg      = 0;
	GLint  m_SpriteAttribPos = -1;
	GLint  m_SpriteUniRect   = -1;
	GLint  m_SpriteUniColor  = -1;
	GLint  m_SpriteUniRes    = -1;

	// ── 텍스처 쿼드 (글리프) ──
	GLuint m_TexProg      = 0;
	GLint  m_TexAttribPos = -1;
	GLint  m_TexUniRect   = -1;
	GLint  m_TexUniUV     = -1;
	GLint  m_TexUniRes    = -1;
	GLint  m_TexUniColor  = -1;
	GLint  m_TexUniTex    = -1;

	// ── 물 / 불꽃 ──
	GLuint m_WaterProg      = 0;
	GLint  m_WaterAttribPos = -1;
	GLint  m_WaterUniRect   = -1;
	GLint  m_WaterUniRes    = -1;
	GLint  m_WaterUniWorld  = -1;
	GLint  m_WaterUniColor  = -1;
	GLint  m_WaterUniTime   = -1;
	GLint  m_WaterUniDepth  = -1;
	GLint  m_WaterUniLit    = -1;

	GLuint m_FlameProg      = 0;
	GLint  m_FlameAttribPos = -1;
	GLint  m_FlameUniRect   = -1;
	GLint  m_FlameUniRes    = -1;
	GLint  m_FlameUniTime   = -1;
	GLint  m_FlameUniSeed   = -1;
	GLint  m_FlameUniInten  = -1;

	// ── 사후처리 ──
	GLuint m_BrightProg      = 0;
	GLint  m_BrightAttribPos = -1;
	GLint  m_BrightUniScene  = -1;
	GLint  m_BrightUniThr    = -1;

	GLuint m_BlurProg      = 0;
	GLint  m_BlurAttribPos = -1;
	GLint  m_BlurUniTex    = -1;
	GLint  m_BlurUniDir    = -1;

	GLuint m_CompProg      = 0;
	GLint  m_CompAttribPos = -1;
	GLint  m_CompUniScene  = -1;
	GLint  m_CompUniBloom  = -1;
	GLint  m_CompUniRes    = -1;
	GLint  m_CompUniDread  = -1;
	GLint  m_CompUniTime   = -1;

	// ── 렌더 타깃 ──
	GLuint m_SceneFBO   = 0;
	GLuint m_SceneTex   = 0;
	GLuint m_BloomFBO[2] = { 0, 0 };
	GLuint m_BloomTex[2] = { 0, 0 };
	int    m_HalfW = 0;
	int    m_HalfH = 0;

	Font m_FontSmall;
	Font m_FontBody;
	Font m_FontTitle;
};
