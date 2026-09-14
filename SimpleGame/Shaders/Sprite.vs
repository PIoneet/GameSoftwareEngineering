#version 330

// 단위 도형(사각형 / 마름모)의 정점. x, y 는 [0,1] 범위.
in vec3 a_Position;

uniform vec4 u_Rect;        // x, y, w, h  — 화면 픽셀, 좌상단 원점
uniform vec2 u_Resolution;  // 창 크기(픽셀)

void main()
{
	vec2 p = u_Rect.xy + a_Position.xy * u_Rect.zw;

	// 픽셀 → NDC. y 는 위아래가 뒤집힌다.
	vec2 ndc = vec2(p.x / u_Resolution.x * 2.0 - 1.0,
	                1.0 - p.y / u_Resolution.y * 2.0);

	gl_Position = vec4(ndc, 0.0, 1.0);
}
