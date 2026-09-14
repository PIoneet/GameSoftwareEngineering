#version 330

in vec3 a_Position;

uniform vec4 u_Rect;         // x, y, w, h — 화면 픽셀, 좌상단 원점
uniform vec2 u_Resolution;
uniform vec2 u_World;        // 타일의 월드 좌표. 물결이 타일 경계에서 끊기지 않게 한다.

out vec2 v_World;

void main()
{
	v_World = u_World + a_Position.xy;

	vec2 p = u_Rect.xy + a_Position.xy * u_Rect.zw;

	gl_Position = vec4(p.x / u_Resolution.x * 2.0 - 1.0,
	                   1.0 - p.y / u_Resolution.y * 2.0,
	                   0.0, 1.0);
}
