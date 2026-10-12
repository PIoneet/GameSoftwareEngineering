#include "stdafx.h"
#include "Profiler.h"

#include "Dependencies\glew.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace
{
	typedef std::chrono::high_resolution_clock Clock;

	bool s_Enabled = true;

	char s_Label[32] = "?";

	// ── 이번 프레임 ──
	Clock::time_point s_ZoneStart[PROF_ZONE_COUNT];
	double            s_ZoneMs[PROF_ZONE_COUNT]    = { 0.0 };
	int               s_Counter[PROF_COUNTER_COUNT] = { 0 };

	// ── 1초 누적 ──
	int    s_Frames = 0;
	double s_AccZoneMs[PROF_ZONE_COUNT]   = { 0.0 };
	double s_AccCounter[PROF_COUNTER_COUNT] = { 0.0 };
	double s_AccDt      = 0.0;
	double s_AccGpuMs   = 0.0;
	int    s_GpuSamples = 0;
	double s_MaxFrameMs = 0.0;
	double s_MinFrameMs = 1e9;

	// 마지막으로 닫힌 창의 값. 출력을 꺼도 남는다 — 모니터가 읽는다.
	double s_LastFps     = 0.0;
	double s_LastFrameMs = 0.0;
	double s_LastGpuMs   = -1.0;

	// 켠 직후 몇 초는 CPU/GPU 클럭이 아직 올라오지 않아 프레임이 느리게 잡힌다.
	// 창마다 경과 시간을 같이 찍어, 정상 상태의 수치인지 구분할 수 있게 한다.
	Clock::time_point s_StartTime;

	// ── GPU 타이머 질의 ──
	// 두 개를 번갈아 쓴다. 방금 낸 질의를 곧바로 읽으면 GPU 가 끝날 때까지
	// CPU 가 멈춰 서므로, 한 프레임 전 결과를 읽는다.
	bool   s_GpuReady   = false;
	GLuint s_GpuQuery[2] = { 0, 0 };
	int    s_GpuIndex    = 0;
	bool   s_GpuIssued[2] = { false, false };
	double s_GpuLastMs   = -1.0;

	double Avg(double acc, int frames)
	{
		return (frames > 0) ? acc / (double)frames : 0.0;
	}

	double Ratio(double part, double whole)
	{
		return (whole > 0.0) ? part / whole : 0.0;
	}

	// 1초 창을 비운다. 출력하든 안 하든 같은 자리에서 끊어야
	// 켰다 껐다 할 때 수치가 튀지 않는다.
	void ResetWindow()
	{
		s_Frames     = 0;
		s_AccDt      = 0.0;
		s_AccGpuMs   = 0.0;
		s_GpuSamples = 0;
		s_MaxFrameMs = 0.0;
		s_MinFrameMs = 1e9;

		for (int i = 0; i < PROF_ZONE_COUNT; ++i)
			s_AccZoneMs[i] = 0.0;

		for (int i = 0; i < PROF_COUNTER_COUNT; ++i)
			s_AccCounter[i] = 0.0;
	}
}

void Prof::Init()
{
	s_StartTime = Clock::now();

	if (!glewIsSupported("GL_ARB_timer_query"))
	{
		printf("[PROF] GPU 타이머 질의 미지원 — CPU 시간만 잽니다.\n");
		return;
	}

	glGenQueries(2, s_GpuQuery);
	s_GpuReady = (s_GpuQuery[0] != 0 && s_GpuQuery[1] != 0);
}

void Prof::Shutdown()
{
	if (!s_GpuReady)
		return;

	glDeleteQueries(2, s_GpuQuery);

	s_GpuReady = false;
}

float Prof::Fps()     { return (float)s_LastFps; }
float Prof::FrameMs() { return (float)s_LastFrameMs; }
float Prof::GpuMs()   { return (float)s_LastGpuMs; }

void Prof::SetEnabled(bool on) { s_Enabled = on; }
bool Prof::Enabled()           { return s_Enabled; }

void Prof::SetLabel(const char* label)
{
	if (label == 0)
		return;

	size_t n = strlen(label);

	if (n > sizeof(s_Label) - 1)
		n = sizeof(s_Label) - 1;

	memcpy(s_Label, label, n);
	s_Label[n] = '\0';
}

void Prof::BeginFrame()
{
	for (int i = 0; i < PROF_ZONE_COUNT; ++i)
		s_ZoneMs[i] = 0.0;

	for (int i = 0; i < PROF_COUNTER_COUNT; ++i)
		s_Counter[i] = 0;

	ZoneBegin(PROF_FRAME);
}

void Prof::ZoneBegin(int zone)
{
	if (zone < 0 || zone >= PROF_ZONE_COUNT)
		return;

	s_ZoneStart[zone] = Clock::now();
}

void Prof::ZoneEnd(int zone)
{
	if (zone < 0 || zone >= PROF_ZONE_COUNT)
		return;

	std::chrono::duration<double, std::milli> d = Clock::now() - s_ZoneStart[zone];
	s_ZoneMs[zone] += d.count();
}

void Prof::Add(int counter, int n)
{
	if (counter >= 0 && counter < PROF_COUNTER_COUNT)
		s_Counter[counter] += n;
}

void Prof::Set(int counter, int v)
{
	if (counter >= 0 && counter < PROF_COUNTER_COUNT)
		s_Counter[counter] = v;
}

void Prof::GpuFrameBegin()
{
	if (!s_GpuReady)
		return;

	glBeginQuery(GL_TIME_ELAPSED, s_GpuQuery[s_GpuIndex]);
}

void Prof::GpuFrameEnd()
{
	if (!s_GpuReady)
		return;

	glEndQuery(GL_TIME_ELAPSED);
	s_GpuIssued[s_GpuIndex] = true;

	// 한 프레임 전 결과를 읽는다. 준비가 안 됐으면 그냥 건너뛴다.
	int prev = 1 - s_GpuIndex;

	if (s_GpuIssued[prev])
	{
		GLint available = 0;
		glGetQueryObjectiv(s_GpuQuery[prev], GL_QUERY_RESULT_AVAILABLE, &available);

		if (available)
		{
			GLuint64 ns = 0;
			glGetQueryObjectui64v(s_GpuQuery[prev], GL_QUERY_RESULT, &ns);

			s_GpuLastMs = (double)ns / 1000000.0;
		}
	}

	s_GpuIndex = prev;
}

void Prof::EndFrame(float dt)
{
	ZoneEnd(PROF_FRAME);

	// s_Enabled 는 "콘솔에 찍을지" 만 정한다. 집계는 늘 돌아야
	// 모니터가 끈 동안에도 같은 숫자를 읽는다.
	++s_Frames;
	s_AccDt += (double)dt;

	for (int i = 0; i < PROF_ZONE_COUNT; ++i)
		s_AccZoneMs[i] += s_ZoneMs[i];

	for (int i = 0; i < PROF_COUNTER_COUNT; ++i)
		s_AccCounter[i] += (double)s_Counter[i];

	if (s_ZoneMs[PROF_FRAME] > s_MaxFrameMs)
		s_MaxFrameMs = s_ZoneMs[PROF_FRAME];

	if (s_ZoneMs[PROF_FRAME] < s_MinFrameMs)
		s_MinFrameMs = s_ZoneMs[PROF_FRAME];

	if (s_GpuLastMs >= 0.0)
	{
		s_AccGpuMs += s_GpuLastMs;
		++s_GpuSamples;
	}

	if (s_AccDt < 1.0 || s_Frames == 0)
		return;

	// ── 1초 요약 ──
	double fps      = (double)s_Frames / s_AccDt;
	double frameMs  = Avg(s_AccZoneMs[PROF_FRAME], s_Frames);
	double cpuMs    = frameMs;
	double gpuMs    = (s_GpuSamples > 0) ? s_AccGpuMs / (double)s_GpuSamples : -1.0;

	s_LastFps     = fps;
	s_LastFrameMs = frameMs;
	s_LastGpuMs   = gpuMs;

	const char* bound = "unknown";

	if (gpuMs >= 0.0)
	{
		if      (cpuMs > gpuMs * 1.25) bound = "cpu";
		else if (gpuMs > cpuMs * 1.25) bound = "gpu";
		else                           bound = "mixed";
	}

	double dcShape = Avg(s_AccCounter[PROF_DC_SHAPE], s_Frames);
	double dcText  = Avg(s_AccCounter[PROF_DC_TEXT],  s_Frames);
	double dcPost  = Avg(s_AccCounter[PROF_DC_POST],  s_Frames);
	double dcTotal = dcShape + dcText + dcPost;

	double aTotal   = Avg(s_AccCounter[PROF_ACTOR_TOTAL],   s_Frames);
	double aVisited = Avg(s_AccCounter[PROF_ACTOR_VISITED], s_Frames);
	double aVisible = Avg(s_AccCounter[PROF_ACTOR_VISIBLE], s_Frames);
	double aUpdated = Avg(s_AccCounter[PROF_ACTOR_UPDATED], s_Frames);
	double aReaped  = Avg(s_AccCounter[PROF_ACTOR_REAPED],  s_Frames);

	double tScanned = Avg(s_AccCounter[PROF_TILE_SCANNED], s_Frames);
	double tDrawn   = Avg(s_AccCounter[PROF_TILE_DRAWN],   s_Frames);

	double cCalls = Avg(s_AccCounter[PROF_COLLIDE_CALLS],      s_Frames);
	double cCand  = Avg(s_AccCounter[PROF_COLLIDE_CANDIDATES], s_Frames);

	double qCalls   = Avg(s_AccCounter[PROF_QUERY_CALLS],   s_Frames);
	double qScanned = Avg(s_AccCounter[PROF_QUERY_SCANNED], s_Frames);

	double glyphs = Avg(s_AccCounter[PROF_GLYPH_BAKED], s_Frames);

	double tileWaste = 1.0 - Ratio(tDrawn, tScanned);
	double cullRatio = 1.0 - Ratio(aVisible, aVisited);
	double candPer   = Ratio(cCand, cCalls);
	double scanPer   = Ratio(qScanned, qCalls);

	if (!s_Enabled)
	{
		ResetWindow();
		return;
	}

	printf("[PROF] ==================================================\n");

	std::chrono::duration<double> since = Clock::now() - s_StartTime;

	printf("[PROF] label=%s t=%.0fs fps=%.1f frame_ms=%.2f frame_min_ms=%.2f "
	       "frame_max_ms=%.2f cpu_ms=%.2f ",
	       s_Label, since.count(), fps, frameMs, s_MinFrameMs, s_MaxFrameMs, cpuMs);

	if (gpuMs >= 0.0) printf("gpu_ms=%.2f bound=%s\n", gpuMs, bound);
	else              printf("gpu_ms=n/a bound=%s\n", bound);

	printf("[PROF] update_ms=%.2f scene=%.2f motes=%.2f pickup=%.2f\n",
	       Avg(s_AccZoneMs[PROF_UPDATE],     s_Frames),
	       Avg(s_AccZoneMs[PROF_UPD_SCENE],  s_Frames),
	       Avg(s_AccZoneMs[PROF_UPD_MOTES],  s_Frames),
	       Avg(s_AccZoneMs[PROF_UPD_PICKUP], s_Frames));

	printf("[PROF] render_ms=%.2f tiles=%.2f glow=%.2f collect=%.2f sort=%.2f "
	       "actors=%.2f motes=%.2f post=%.2f ui=%.2f\n",
	       Avg(s_AccZoneMs[PROF_RENDER],      s_Frames),
	       Avg(s_AccZoneMs[PROF_RND_TILES],   s_Frames),
	       Avg(s_AccZoneMs[PROF_RND_GLOW],    s_Frames),
	       Avg(s_AccZoneMs[PROF_RND_COLLECT], s_Frames),
	       Avg(s_AccZoneMs[PROF_RND_SORT],    s_Frames),
	       Avg(s_AccZoneMs[PROF_RND_ACTORS],  s_Frames),
	       Avg(s_AccZoneMs[PROF_RND_MOTES],   s_Frames),
	       Avg(s_AccZoneMs[PROF_RND_POST],    s_Frames),
	       Avg(s_AccZoneMs[PROF_RND_UI],      s_Frames));

	printf("[PROF] drawcall=%.0f shape=%.0f text=%.0f post=%.0f\n",
	       dcTotal, dcShape, dcText, dcPost);

	printf("[PROF] actor total=%.0f visited=%.0f visible=%.0f updated=%.0f "
	       "reaped=%.1f cull_ratio=%.2f\n",
	       aTotal, aVisited, aVisible, aUpdated, aReaped, cullRatio);

	printf("[PROF] tile scanned=%.0f drawn=%.0f waste_ratio=%.2f\n",
	       tScanned, tDrawn, tileWaste);

	printf("[PROF] query collide=%.0f cand=%.0f cand_per_call=%.1f "
	       "scene=%.0f scanned=%.0f scan_per_query=%.0f\n",
	       cCalls, cCand, candPer, qCalls, qScanned, scanPer);

	printf("[PROF] font glyph_baked=%.1f\n", glyphs);

	// ── 임계값을 넘은 것만 짚어 준다 ──
	// 숫자를 보고 결론까지 가는 단계를 여기서 한 번 줄여 둔다.
	if (tileWaste > 0.5 && tScanned > 0.0)
	{
		printf("[PROF] HINT tile.waste_ratio=%.2f — 맵 전체를 매 프레임 순회 중. "
		       "가시 범위만 돌면 대부분 사라진다.\n", tileWaste);
	}

	// 선형 탐색이라는 사실만으로는 짚지 않는다. 늘 켜져 있는 경고는 읽히지 않는다.
	// 프레임당 총 스캔이 실제 비용이 될 만큼 쌓였을 때만 말한다.
	if (qScanned > 20000.0)
	{
		printf("[PROF] HINT query scanned=%.0f/frame (scan_per_query=%.0f) — "
		       "씬 질의가 전 액터 선형 탐색. 공간 격자가 필요하다.\n", qScanned, scanPer);
	}

	if (candPer > 20.0)
	{
		printf("[PROF] HINT query.cand_per_call=%.1f — 충돌 격자 한 칸에 오브젝트가 많다. "
		       "GRID_CELL 을 줄이는 것을 검토.\n", candPer);
	}

	if (glyphs > 0.5)
	{
		printf("[PROF] HINT font.glyph_baked=%.1f — 매 프레임 글리프를 굽고 있다. "
		       "정상이면 0 에 수렴해야 한다.\n", glyphs);
	}

	// 예산에 여유가 있는 동안에는 말하지 않는다. 배칭은 지금 할 일이 아니다.
	if (dcTotal > 3000.0 && frameMs > 8.0 && strcmp(bound, "cpu") == 0)
	{
		printf("[PROF] HINT drawcall=%.0f 이면서 cpu bound — 스프라이트 배칭으로 "
		       "한 자릿수까지 줄일 수 있다.\n", dcTotal);
	}

	if (frameMs > 12.0)
	{
		printf("[PROF] HINT frame_ms=%.2f — 60fps 예산(16.6ms)의 72%% 초과. "
		       "최적화 착수 시점.\n", frameMs);
	}

	ResetWindow();
}
