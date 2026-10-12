#version 330

// 단위 도형(사각형 / 마름모)의 정점. x, y 는 [0,1] 범위.
in vec3 a_Position;

uniform vec4 u_Rect;        // x, y, w, h  — 화면 픽셀, 좌상단 원점
uniform vec2 u_Resolution;  // 창 크기(픽셀)

// (cos, sin). 사각형 중심을 축으로 돈다. 칼날처럼 축에 정렬되지 않는 것에 쓴다.
// (1, 0) 이면 아래 식이 u_Rect.xy + a_Position.xy * u_Rect.zw 로 정확히 환원되므로,
// 회전을 쓰지 않는 기존 도형은 결과가 한 픽셀도 달라지지 않는다.
uniform vec2 u_Rot;

void main()
{
	vec2 local  = (a_Position.xy - vec2(0.5)) * u_Rect.zw;

	vec2 turned = vec2(local.x * u_Rot.x - local.y * u_Rot.y,
	                   local.x * u_Rot.y + local.y * u_Rot.x);

	vec2 p = u_Rect.xy + u_Rect.zw * 0.5 + turned;

	// 픽셀 → NDC. y 는 위아래가 뒤집힌다.
	vec2 ndc = vec2(p.x / u_Resolution.x * 2.0 - 1.0,
	                1.0 - p.y / u_Resolution.y * 2.0);

	gl_Position = vec4(ndc, 0.0, 1.0);
}
