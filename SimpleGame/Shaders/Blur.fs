#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Tex;
uniform vec2      u_Direction;   // UV 단위 오프셋. 가로/세로 두 번에 나눠 부른다.

// 선형 샘플링을 이용한 9탭 가우시안 (5회 샘플로 근사)
void main()
{
	vec2 o1 = u_Direction * 1.3846153846;
	vec2 o2 = u_Direction * 3.2307692308;

	vec3 sum = texture(u_Tex, v_UV).rgb * 0.2270270270;
	sum += (texture(u_Tex, v_UV + o1).rgb + texture(u_Tex, v_UV - o1).rgb) * 0.3162162162;
	sum += (texture(u_Tex, v_UV + o2).rgb + texture(u_Tex, v_UV - o2).rgb) * 0.0702702703;

	FragColor = vec4(sum, 1.0);
}
