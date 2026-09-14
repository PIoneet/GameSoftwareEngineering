#pragma once

#include <map>
#include "Dependencies\glew.h"

// 아틀라스 안에서 글리프 한 글자가 차지하는 자리.
struct Glyph
{
	float u0, v0, u1, v1;
	int   w, h;
	int   advance;
};

// GDI 로 글자를 구워 텍스처 아틀라스에 캐싱한다.
// GLUT 비트맵 폰트는 ASCII 전용이라 한글을 못 찍는다. 한글 11,172자를
// 미리 굽는 것도 낭비이므로, 실제로 화면에 나온 글자만 그때그때 굽는다.
//
// Windows 타입(HDC/HFONT)은 헤더에 노출하지 않는다 — <windows.h> 가
// 다른 번역 단위로 새어 들어가면 DrawText 같은 매크로 사고가 난다.
class Font
{
public:
	~Font();

	bool Create(const wchar_t* faceName, int pixelHeight, bool bold);
	void Destroy();

	const Glyph* Get(wchar_t ch);

	GLuint Texture()    const { return m_Tex; }
	int    LineHeight() const { return m_LineHeight; }
	int    Ascent()     const { return m_Ascent; }

	int    MeasureWidth(const wchar_t* text);

private:
	bool Bake(wchar_t ch, Glyph& out);

	void*          m_DC      = 0;   // HDC
	void*          m_Font    = 0;   // HFONT
	void*          m_Bitmap  = 0;   // HBITMAP
	unsigned char* m_Bits    = 0;   // DIB 픽셀 (BGRX)
	int            m_CanvasW = 0;
	int            m_CanvasH = 0;

	GLuint m_Tex    = 0;
	int    m_AtlasW = 1024;
	int    m_AtlasH = 1024;
	int    m_PenX   = 1;
	int    m_PenY   = 1;
	int    m_RowH   = 0;

	int m_LineHeight = 0;
	int m_Ascent     = 0;

	std::map<wchar_t, Glyph> m_Glyphs;
};
