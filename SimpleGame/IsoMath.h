#pragma once

// ── 아이소메트릭 좌표 규약 ──────────────────────────────────────────────
//  월드(타일) 좌표 : 게임 로직 전용. 정사각 격자. x 는 화면 우하단,
//                    y 는 화면 좌하단 방향으로 뻗는다.
//  스크린 좌표     : 렌더러 전용. 픽셀 단위, 좌상단 원점.
//  깊이            : x + y 만으로 결정한다. 높이(z)는 화면 Y 를 위로
//                    밀 뿐 정렬에 관여하지 않는다 — 그래야 언덕 위/아래
//                    오브젝트의 앞뒤 관계가 뒤집히지 않는다.
// ────────────────────────────────────────────────────────────────────────

const float TILE_W = 64.0f;   // 타일 하나의 화면 폭
const float TILE_H = 32.0f;   // 타일 하나의 화면 높이 (폭의 절반 = 2:1 쿼터뷰)

inline float IsoScreenX(float wx, float wy)
{
	return (wx - wy) * (TILE_W * 0.5f);
}

inline float IsoScreenY(float wx, float wy, float wz = 0.0f)
{
	return (wx + wy) * (TILE_H * 0.5f) - wz;
}

inline float IsoDepth(float wx, float wy)
{
	return wx + wy;
}
