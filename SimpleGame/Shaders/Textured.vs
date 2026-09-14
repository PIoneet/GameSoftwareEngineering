#version 330

in vec3 a_Position;          // 단위 사각형 [0,1]

uniform vec4 u_Rect;         // x, y, w, h  — 화면 픽셀, 좌상단 원점
uniform vec4 u_UV;           // u0, v0, u1, v1
uniform vec2 u_Resolution;

out vec2 v_UV;

void main()
{
	vec2 p = u_Rect.xy + a_Position.xy * u_Rect.zw;

	v_UV = mix(u_UV.xy, u_UV.zw, a_Position.xy);

	gl_Position = vec4(p.x / u_Resolution.x * 2.0 - 1.0,
	                   1.0 - p.y / u_Resolution.y * 2.0,
	                   0.0, 1.0);
}
