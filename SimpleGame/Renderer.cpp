#include "stdafx.h"
#include "Renderer.h"

#include <cstring>

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}

Renderer::~Renderer()
{
	DestroyTargets();

	if (m_VBOQuad)    glDeleteBuffers(1, &m_VBOQuad);
	if (m_VBODiamond) glDeleteBuffers(1, &m_VBODiamond);

	if (m_SpriteProg) glDeleteProgram(m_SpriteProg);
	if (m_TexProg)    glDeleteProgram(m_TexProg);
	if (m_BrightProg) glDeleteProgram(m_BrightProg);
	if (m_BlurProg)   glDeleteProgram(m_BlurProg);
	if (m_CompProg)   glDeleteProgram(m_CompProg);

	m_FontSmall.Destroy();
	m_FontBody.Destroy();
	m_FontTitle.Destroy();
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	m_WindowSizeX = (float)windowSizeX;
	m_WindowSizeY = (float)windowSizeY;

	m_SpriteProg = CompileShaders("Sprite.vs",   "Sprite.fs");
	m_TexProg    = CompileShaders("Textured.vs", "Textured.fs");
	m_BrightProg = CompileShaders("Post.vs",     "Bright.fs");
	m_BlurProg   = CompileShaders("Post.vs",     "Blur.fs");
	m_CompProg   = CompileShaders("Post.vs",     "Composite.fs");

	CreateVertexBufferObjects();

	// uniform / attribute 위치는 여기서 한 번만 조회한다.
	// 드로우 콜마다 조회하면 오브젝트가 늘어날수록 비용이 쌓인다.
	if (m_SpriteProg && m_TexProg && m_BrightProg && m_BlurProg && m_CompProg &&
	    m_VBOQuad && m_VBODiamond)
	{
		m_SpriteAttribPos = glGetAttribLocation(m_SpriteProg, "a_Position");
		m_SpriteUniRect   = glGetUniformLocation(m_SpriteProg, "u_Rect");
		m_SpriteUniColor  = glGetUniformLocation(m_SpriteProg, "u_Color");
		m_SpriteUniRes    = glGetUniformLocation(m_SpriteProg, "u_Resolution");

		m_TexAttribPos = glGetAttribLocation(m_TexProg, "a_Position");
		m_TexUniRect   = glGetUniformLocation(m_TexProg, "u_Rect");
		m_TexUniUV     = glGetUniformLocation(m_TexProg, "u_UV");
		m_TexUniRes    = glGetUniformLocation(m_TexProg, "u_Resolution");
		m_TexUniColor  = glGetUniformLocation(m_TexProg, "u_Color");
		m_TexUniTex    = glGetUniformLocation(m_TexProg, "u_Tex");

		m_BrightAttribPos = glGetAttribLocation(m_BrightProg, "a_Position");
		m_BrightUniScene  = glGetUniformLocation(m_BrightProg, "u_Scene");
		m_BrightUniThr    = glGetUniformLocation(m_BrightProg, "u_Threshold");

		m_BlurAttribPos = glGetAttribLocation(m_BlurProg, "a_Position");
		m_BlurUniTex    = glGetUniformLocation(m_BlurProg, "u_Tex");
		m_BlurUniDir    = glGetUniformLocation(m_BlurProg, "u_Direction");

		m_CompAttribPos = glGetAttribLocation(m_CompProg, "a_Position");
		m_CompUniScene  = glGetUniformLocation(m_CompProg, "u_Scene");
		m_CompUniBloom  = glGetUniformLocation(m_CompProg, "u_Bloom");
		m_CompUniRes    = glGetUniformLocation(m_CompProg, "u_Resolution");
		m_CompUniDread  = glGetUniformLocation(m_CompProg, "u_Dread");
		m_CompUniTime   = glGetUniformLocation(m_CompProg, "u_Time");

		m_Initialized = true;
	}

	// 한글 폰트. Malgun Gothic 이 없으면 GDI 가 HANGEUL_CHARSET 기준으로 대체한다.
	if (!m_FontSmall.Create(L"Malgun Gothic", 14, false))
		m_FontSmall.Create(L"Gulim", 14, false);

	if (!m_FontBody.Create(L"Malgun Gothic", 17, false))
		m_FontBody.Create(L"Gulim", 17, false);

	if (!m_FontTitle.Create(L"Malgun Gothic", 25, true))
		m_FontTitle.Create(L"Gulim", 25, true);

	m_TargetsOK = CreateTargets(windowSizeX, windowSizeY);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// 깊이는 그리기 순서(painter's algorithm)로 해결한다.
	glDisable(GL_DEPTH_TEST);
}

bool Renderer::IsInitialized() const
{
	return m_Initialized;
}


// ─────────────────────────────────────────────────────────────────────────
//  렌더 타깃
// ─────────────────────────────────────────────────────────────────────────

static GLuint MakeColorTarget(int w, int h, GLuint* fboOut)
{
	GLuint tex = 0;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLuint fbo = 0;

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		glDeleteFramebuffers(1, &fbo);
		glDeleteTextures(1, &tex);

		*fboOut = 0;
		return 0;
	}

	*fboOut = fbo;

	return tex;
}

bool Renderer::CreateTargets(int w, int h)
{
	DestroyTargets();

	if (w < 1) w = 1;
	if (h < 1) h = 1;

	m_HalfW = w / 2; if (m_HalfW < 1) m_HalfW = 1;
	m_HalfH = h / 2; if (m_HalfH < 1) m_HalfH = 1;

	m_SceneTex = MakeColorTarget(w, h, &m_SceneFBO);

	if (m_SceneTex == 0)
		return false;

	// 블룸은 절반 해상도로 충분하다. 핑퐁으로 가로·세로 블러를 나눠 건다.
	for (int i = 0; i < 2; ++i)
	{
		m_BloomTex[i] = MakeColorTarget(m_HalfW, m_HalfH, &m_BloomFBO[i]);

		if (m_BloomTex[i] == 0)
			return false;
	}

	return true;
}

void Renderer::DestroyTargets()
{
	if (m_SceneFBO) { glDeleteFramebuffers(1, &m_SceneFBO); m_SceneFBO = 0; }
	if (m_SceneTex) { glDeleteTextures(1, &m_SceneTex);     m_SceneTex = 0; }

	for (int i = 0; i < 2; ++i)
	{
		if (m_BloomFBO[i]) { glDeleteFramebuffers(1, &m_BloomFBO[i]); m_BloomFBO[i] = 0; }
		if (m_BloomTex[i]) { glDeleteTextures(1, &m_BloomTex[i]);     m_BloomTex[i] = 0; }
	}
}

void Renderer::Resize(int windowSizeX, int windowSizeY)
{
	m_WindowSizeX = (float)windowSizeX;
	m_WindowSizeY = (float)windowSizeY;

	m_TargetsOK = CreateTargets(windowSizeX, windowSizeY);
}


// ─────────────────────────────────────────────────────────────────────────
//  셰이더 / VBO
// ─────────────────────────────────────────────────────────────────────────

void Renderer::CreateVertexBufferObjects()
{
	float quad[] =
	{
		0.f, 0.f, 0.f,   1.f, 0.f, 0.f,   1.f, 1.f, 0.f,
		0.f, 0.f, 0.f,   1.f, 1.f, 0.f,   0.f, 1.f, 0.f,
	};

	// 같은 [0,1] 바운딩 박스 안의 마름모
	float diamond[] =
	{
		0.5f, 0.f, 0.f,   1.f, 0.5f, 0.f,   0.5f, 1.f, 0.f,
		0.5f, 0.f, 0.f,   0.5f, 1.f, 0.f,   0.f, 0.5f, 0.f,
	};

	glGenBuffers(1, &m_VBOQuad);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	glGenBuffers(1, &m_VBODiamond);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBODiamond);
	glBufferData(GL_ARRAY_BUFFER, sizeof(diamond), diamond, GL_STATIC_DRAW);
}

void Renderer::AddShader(GLuint shaderProgram, const char* shaderText, GLenum shaderType)
{
	GLuint shaderObj = glCreateShader(shaderType);

	if (shaderObj == 0)
	{
		std::cout << "Error creating shader type " << shaderType << "\n";
		return;
	}

	const GLchar* p[1]     = { shaderText };
	GLint         lengths[1] = { (GLint)strlen(shaderText) };

	glShaderSource(shaderObj, 1, p, lengths);
	glCompileShader(shaderObj);

	GLint success = 0;
	glGetShaderiv(shaderObj, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		GLchar infoLog[1024] = { 0 };
		glGetShaderInfoLog(shaderObj, sizeof(infoLog), NULL, infoLog);

		std::cout << "Error compiling shader type " << shaderType << ": " << infoLog << "\n";
	}

	glAttachShader(shaderProgram, shaderObj);

	// 링크 후에는 프로그램이 참조를 갖는다. 여기서 지워야 셰이더 오브젝트가 남지 않는다.
	glDeleteShader(shaderObj);
}

bool Renderer::ReadShaderFile(const char* filename, std::string* target)
{
	// 실행 위치(VS 디버깅 / exe 직접 실행)에 따라 작업 디렉터리가 달라진다.
	const char* prefixes[] =
	{
		"./Shaders/",
		"./SimpleGame/Shaders/",
		"../SimpleGame/Shaders/",
		"../../SimpleGame/Shaders/",
	};

	for (int i = 0; i < 4; ++i)
	{
		std::string   path = std::string(prefixes[i]) + filename;
		std::ifstream file(path.c_str());

		if (file.fail())
			continue;

		std::string line;

		while (getline(file, line))
		{
			target->append(line);
			target->append("\n");
		}

		return true;
	}

	std::cout << "Shader file not found: " << filename << "\n";

	return false;
}

GLuint Renderer::CompileShaders(const char* filenameVS, const char* filenameFS)
{
	GLuint shaderProgram = glCreateProgram();

	if (shaderProgram == 0)
	{
		std::cout << "Error creating shader program\n";
		return 0;
	}

	std::string vs, fs;

	// 실패는 0 을 돌려준다. GLuint 에 -1 을 넣으면 거대한 양수가 되어 성공으로 오인된다.
	if (!ReadShaderFile(filenameVS, &vs) || !ReadShaderFile(filenameFS, &fs))
	{
		glDeleteProgram(shaderProgram);
		return 0;
	}

	AddShader(shaderProgram, vs.c_str(), GL_VERTEX_SHADER);
	AddShader(shaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	GLint  success       = 0;
	GLchar errorLog[1024] = { 0 };

	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

	if (success == 0)
	{
		glGetProgramInfoLog(shaderProgram, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " link error\n" << errorLog << "\n";

		glDeleteProgram(shaderProgram);
		return 0;
	}

	glValidateProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &success);

	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " validate error\n" << errorLog << "\n";

		glDeleteProgram(shaderProgram);
		return 0;
	}

	std::cout << filenameVS << ", " << filenameFS << " compiled.\n";

	return shaderProgram;
}


// ─────────────────────────────────────────────────────────────────────────
//  프레임 흐름
// ─────────────────────────────────────────────────────────────────────────

void Renderer::BeginScene(float r, float g, float b)
{
	if (m_TargetsOK)
		glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFBO);

	glViewport(0, 0, (int)m_WindowSizeX, (int)m_WindowSizeY);
	glClearColor(r, g, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::EndScene()
{
	if (m_TargetsOK)
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::DrawFullscreen(GLint attribLocation)
{
	glEnableVertexAttribArray(attribLocation);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glVertexAttribPointer(attribLocation, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(attribLocation);
}

void Renderer::PostProcess(float dread, float time)
{
	if (!m_Initialized || !m_TargetsOK)
		return;

	// 사후처리는 덮어쓰기다. 블렌딩이 켜져 있으면 결과가 섞인다.
	glDisable(GL_BLEND);

	// ── 1) 밝은 부분만 뽑아낸다 ──
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[0]);
	glViewport(0, 0, m_HalfW, m_HalfH);

	glUseProgram(m_BrightProg);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_SceneTex);
	glUniform1i(m_BrightUniScene, 0);
	glUniform1f(m_BrightUniThr, 0.58f);

	DrawFullscreen(m_BrightAttribPos);

	// ── 2) 가로 블러 ──
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[1]);

	glUseProgram(m_BlurProg);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_BloomTex[0]);
	glUniform1i(m_BlurUniTex, 0);
	glUniform2f(m_BlurUniDir, 1.0f / (float)m_HalfW, 0.0f);

	DrawFullscreen(m_BlurAttribPos);

	// ── 3) 세로 블러 ──
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[0]);

	glBindTexture(GL_TEXTURE_2D, m_BloomTex[1]);
	glUniform2f(m_BlurUniDir, 0.0f, 1.0f / (float)m_HalfH);

	DrawFullscreen(m_BlurAttribPos);

	// ── 4) 합성 → 백버퍼 ──
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, (int)m_WindowSizeX, (int)m_WindowSizeY);

	glUseProgram(m_CompProg);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_SceneTex);
	glUniform1i(m_CompUniScene, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_BloomTex[0]);
	glUniform1i(m_CompUniBloom, 1);

	glUniform2f(m_CompUniRes, m_WindowSizeX, m_WindowSizeY);
	glUniform1f(m_CompUniDread, dread);
	glUniform1f(m_CompUniTime, time);

	DrawFullscreen(m_CompAttribPos);

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_BLEND);
}


// ─────────────────────────────────────────────────────────────────────────
//  도형
// ─────────────────────────────────────────────────────────────────────────

void Renderer::DrawUnitShape(GLuint vbo, float x, float y, float w, float h,
                             float r, float g, float b, float a)
{
	if (!m_Initialized)
		return;

	glUseProgram(m_SpriteProg);

	glUniform4f(m_SpriteUniRect, x, y, w, h);
	glUniform4f(m_SpriteUniColor, r, g, b, a);
	glUniform2f(m_SpriteUniRes, m_WindowSizeX, m_WindowSizeY);

	glEnableVertexAttribArray(m_SpriteAttribPos);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(m_SpriteAttribPos, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(m_SpriteAttribPos);
}

void Renderer::DrawQuad(float x, float y, float w, float h,
                        float r, float g, float b, float a)
{
	DrawUnitShape(m_VBOQuad, x, y, w, h, r, g, b, a);
}

void Renderer::DrawDiamond(float cx, float cy, float w, float h,
                           float r, float g, float b, float a)
{
	DrawUnitShape(m_VBODiamond, cx - w * 0.5f, cy - h * 0.5f, w, h, r, g, b, a);
}


// ─────────────────────────────────────────────────────────────────────────
//  텍스트
// ─────────────────────────────────────────────────────────────────────────

Font& Renderer::FontFor(TextFont font)
{
	switch (font)
	{
	case FONT_SMALL: return m_FontSmall;
	case FONT_TITLE: return m_FontTitle;
	case FONT_BODY:
	default:         return m_FontBody;
	}
}

void Renderer::DrawString(float x, float y, const wchar_t* text,
                          float r, float g, float b, float a, TextFont font)
{
	if (!m_Initialized || text == 0)
		return;

	Font& f = FontFor(font);

	if (f.Texture() == 0)
		return;

	// 글리프를 먼저 전부 구워 둔다. 그리는 도중에 구우면 Bake 가
	// 텍스처 바인딩을 건드려서 뒤따르는 글자가 깨진다.
	for (const wchar_t* p = text; *p != L'\0'; ++p)
		f.Get(*p);

	glUseProgram(m_TexProg);
	glUniform2f(m_TexUniRes, m_WindowSizeX, m_WindowSizeY);
	glUniform4f(m_TexUniColor, r, g, b, a);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, f.Texture());
	glUniform1i(m_TexUniTex, 0);

	glEnableVertexAttribArray(m_TexAttribPos);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glVertexAttribPointer(m_TexAttribPos, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	float penX = x;
	float top  = y - (float)f.Ascent();

	for (const wchar_t* p = text; *p != L'\0'; ++p)
	{
		const Glyph* g = f.Get(*p);

		if (g == 0)
			continue;

		if (g->w > 0)
		{
			glUniform4f(m_TexUniRect, penX, top, (float)g->w, (float)g->h);
			glUniform4f(m_TexUniUV, g->u0, g->v0, g->u1, g->v1);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		penX += (float)g->advance;
	}

	glDisableVertexAttribArray(m_TexAttribPos);
	glBindTexture(GL_TEXTURE_2D, 0);
}

float Renderer::TextWidth(const wchar_t* text, TextFont font)
{
	return (float)FontFor(font).MeasureWidth(text);
}

float Renderer::LineHeight(TextFont font)
{
	return (float)FontFor(font).LineHeight();
}
