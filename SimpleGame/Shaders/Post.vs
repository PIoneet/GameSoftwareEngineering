#version 330

// 사후처리 공용 정점 셰이더. 단위 사각형을 화면 전체로 펼친다.
in vec3 a_Position;

out vec2 v_UV;

void main()
{
	v_UV = a_Position.xy;
	gl_Position = vec4(a_Position.xy * 2.0 - 1.0, 0.0, 1.0);
}
