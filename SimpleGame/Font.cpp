#include "stdafx.h"
#include "Font.h"

#include <windows.h>
#include <vector>
#include <cstring>
#include <iostream>

Font::~Font()
{
	Destroy();
}

bool Font::Create(const wchar_t* faceName, int pixelHeight, bool bold)
{
	Destroy();

	HDC dc = CreateCompatibleDC(NULL);

	if (dc == NULL)
		return false;

	// 음수 높이 = 문자 높이(em)를 픽셀로 지정. HANGEUL_CHARSET 을 줘야
	// 한글이 없는 폰트로 대체되는 사고를 막을 수 있다.
	HFONT font = CreateFontW(
		-pixelHeight, 0, 0, 0,
		bold ? FW_BOLD : FW_NORMAL,
		FALSE, FALSE, FALSE,
		HANGEUL_CHARSET,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,      // ClearType 은 서브픽셀이라 단채널 아틀라스에 안 맞는다
		DEFAULT_PITCH | FF_DONTCARE,
		faceName);

	if (font == NULL)
	{
		DeleteDC(dc);
		return false;
	}

	SelectObject(dc, font);

	TEXTMETRICW tm;
	GetTextMetricsW(dc, &tm);

	m_LineHeight = tm.tmHeight;
	m_Ascent     = tm.tmAscent;

	// ── 글자 한 자를 그릴 임시 캔버스 ──
	m_CanvasW = pixelHeight * 3 + 8;
	m_CanvasH = m_LineHeight + 8;

	BITMAPINFO bmi;
	ZeroMemory(&bmi, sizeof(bmi));

	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = m_CanvasW;
	bmi.bmiHeader.biHeight      = -m_CanvasH;      // 음수 = 위에서 아래로
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void*   bits = 0;
	HBITMAP bmp  = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);

	if (bmp == NULL)
	{
		DeleteObject(font);
		DeleteDC(dc);
		return false;
	}

	SelectObject(dc, bmp);
	SetBkMode(dc, OPAQUE);
	SetBkColor(dc, RGB(0, 0, 0));
	SetTextColor(dc, RGB(255, 255, 255));

	m_DC     = dc;
	m_Font   = font;
	m_Bitmap = bmp;
	m_Bits   = (unsigned char*)bits;

	// ── 아틀라스 텍스처 ──
	glGenTextures(1, &m_Tex);
	glBindTexture(GL_TEXTURE_2D, m_Tex);

	std::vector<unsigned char> zero((size_t)m_AtlasW * m_AtlasH, 0);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_AtlasW, m_AtlasH, 0,
	             GL_RED, GL_UNSIGNED_BYTE, &zero[0]);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

	m_PenX = 1;
	m_PenY = 1;
	m_RowH = 0;

	return true;
}

void Font::Destroy()
{
	// 나란한 정리 구문은 표처럼 붙여 쓴다.
	if (m_Tex)    { glDeleteTextures(1, &m_Tex);     m_Tex    = 0; }
	if (m_Bitmap) { DeleteObject((HBITMAP)m_Bitmap); m_Bitmap = 0; }
	if (m_Font)   { DeleteObject((HFONT)m_Font);     m_Font   = 0; }
	if (m_DC)     { DeleteDC((HDC)m_DC);             m_DC     = 0; }

	m_Bits = 0;
	m_Glyphs.clear();

	m_PenX = 1;
	m_PenY = 1;
	m_RowH = 0;
}

bool Font::Bake(wchar_t ch, Glyph& out)
{
	HDC dc = (HDC)m_DC;

	if (dc == NULL)
		return false;

	SIZE sz;

	if (!GetTextExtentPoint32W(dc, &ch, 1, &sz))
		return false;

	int gw = sz.cx;
	int gh = sz.cy;

	// 공백처럼 폭만 있고 그림이 없는 글자
	if (gw <= 0 || gh <= 0)
	{
		out.u0 = out.v0 = out.u1 = out.v1 = 0.0f;
		out.w  = 0;
		out.h  = 0;
		out.advance = (gw > 0) ? gw : m_LineHeight / 3;

		return true;
	}

	if (gw > m_CanvasW) gw = m_CanvasW;
	if (gh > m_CanvasH) gh = m_CanvasH;

	// 캔버스를 지우고 한 글자만 그린다.
	memset(m_Bits, 0, (size_t)m_CanvasW * m_CanvasH * 4);
	TextOutW(dc, 0, 0, &ch, 1);
	GdiFlush();

	// ── 아틀라스에 자리 잡기 — 행 단위로 채운다 ──
	if (m_PenX + gw + 1 > m_AtlasW)
	{
		m_PenX = 1;
		m_PenY += m_RowH + 1;
		m_RowH = 0;
	}

	if (m_PenY + gh + 1 > m_AtlasH)
	{
		// 이 프로토타입 규모에서는 도달하지 않는다.
		std::cout << "Font atlas is full.\n";
		return false;
	}

	// BGRX 중 한 채널만 커버리지로 쓴다.
	std::vector<unsigned char> pixels((size_t)gw * gh);

	for (int y = 0; y < gh; ++y)
	for (int x = 0; x < gw; ++x)
		pixels[(size_t)y * gw + x] = m_Bits[((size_t)y * m_CanvasW + x) * 4 + 2];

	glBindTexture(GL_TEXTURE_2D, m_Tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, m_PenX, m_PenY, gw, gh,
	                GL_RED, GL_UNSIGNED_BYTE, &pixels[0]);
	glBindTexture(GL_TEXTURE_2D, 0);

	out.u0 = (float)m_PenX / (float)m_AtlasW;
	out.v0 = (float)m_PenY / (float)m_AtlasH;
	out.u1 = (float)(m_PenX + gw) / (float)m_AtlasW;
	out.v1 = (float)(m_PenY + gh) / (float)m_AtlasH;

	out.w       = gw;
	out.h       = gh;
	out.advance = sz.cx;

	m_PenX += gw + 1;

	if (gh > m_RowH)
		m_RowH = gh;

	return true;
}

const Glyph* Font::Get(wchar_t ch)
{
	std::map<wchar_t, Glyph>::iterator it = m_Glyphs.find(ch);

	if (it != m_Glyphs.end())
		return &it->second;

	Glyph g;

	if (!Bake(ch, g))
		return 0;

	// map 의 노드는 주소가 안정적이라 포인터를 돌려줘도 안전하다.
	return &(m_Glyphs[ch] = g);
}

int Font::MeasureWidth(const wchar_t* text)
{
	if (text == 0)
		return 0;

	int w = 0;

	for (const wchar_t* p = text; *p != L'\0'; ++p)
	{
		const Glyph* g = Get(*p);

		if (g != 0)
			w += g->advance;
	}

	return w;
}
