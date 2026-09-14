#version 330

in vec3 a_Position;          // 단위 사각형 [0,1]

uniform vec4 u_Rect;         // x, y, w, h — 화면 픽셀, 좌상단 원점
uniform vec2 u_Resolution;

out vec2 v_Local;            // y = 0 이 위쪽이다 (u_Rect 가 좌상단 기준이므로)

void main()
{
	v_Local = a_Position.xy;

	vec2 p = u_Rect.xy + a_Position.xy * u_Rect.zw;

	gl_Position = vec4(p.x / u_Resolution.x * 2.0 - 1.0,
	                   1.0 - p.y / u_Resolution.y * 2.0,
	                   0.0, 1.0);
}
