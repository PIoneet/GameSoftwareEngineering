#version 330

in vec2 v_World;
layout(location=0) out vec4 FragColor;

uniform vec4  u_Color;   // 기본 수면색
uniform float u_Time;
uniform float u_Depth;   // 0 = 얕은 물, 1 = 깊은 물
uniform float u_Lit;     // 주변 밝기

void main()
{
	// 방향과 주기가 다른 파동을 겹쳐 규칙성을 깬다.
	float w = sin(v_World.x * 3.1 + u_Time * 1.35)
	        + sin(v_World.y * 2.3 - u_Time * 0.95)
	        + sin((v_World.x + v_World.y) * 1.7 + u_Time * 1.80) * 0.6;

	w /= 2.6;

	vec3 col = u_Color.rgb * (0.88 + w * 0.18);

	// 마루에만 얇게 빛이 걸린다
	float crest = smoothstep(0.55, 0.95, w);
	col += vec3(0.16, 0.34, 0.36) * crest * u_Lit;

	// 얕은 물에는 거품이 인다
	float foam = smoothstep(0.74, 1.0,
	                        sin(v_World.x * 5.0 + v_World.y * 4.0 + u_Time * 2.4));

	col += vec3(0.22, 0.30, 0.30) * foam * (1.0 - u_Depth) * u_Lit;

	FragColor = vec4(col, 1.0);
}
