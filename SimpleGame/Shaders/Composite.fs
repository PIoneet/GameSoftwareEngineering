#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform vec2      u_Resolution;
uniform float     u_Dread;    // 0 = 마을, 1 = 심연
uniform float     u_Time;

float Hash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
	vec3 scene = texture(u_Scene, v_UV).rgb;
	vec3 bloom = texture(u_Bloom, v_UV).rgb;

	vec3 col = scene + bloom * (0.90 + 0.40 * u_Dread);

	// ── 색보정 : 그림자는 푸르게, 빛은 따뜻하게 ──
	float lum        = dot(col, vec3(0.299, 0.587, 0.114));
	vec3  shadowTint = mix(vec3(0.84, 0.93, 1.14), vec3(0.78, 1.08, 1.10), u_Dread);
	vec3  lightTint  = vec3(1.10, 1.02, 0.90);
	col *= mix(shadowTint, lightTint, smoothstep(0.04, 0.52, lum));

	// 대비
	col = (col - 0.5) * 1.14 + 0.5;

	// 심연에 가까울수록 색이 빠진다
	float grey = dot(col, vec3(0.299, 0.587, 0.114));
	col = mix(vec3(grey), col, mix(1.06, 0.68, u_Dread));

	// ── 비네트 ──
	vec2  c   = (v_UV - 0.5) * vec2(1.0, 0.88);
	float d   = length(c) * 1.95;
	float vig = 1.0 - smoothstep(0.40, 1.32, d) * (0.74 + 0.20 * u_Dread);
	col *= vig;

	// 전역 감광 — 황혼
	col *= 1.0 - (0.08 + 0.28 * u_Dread);

	// 심연 틴트
	col = mix(col, col * vec3(0.60, 1.12, 1.06), u_Dread * 0.55);

	// ── 필름 그레인 ──
	float grain = Hash(v_UV * u_Resolution + fract(u_Time) * 137.0) - 0.5;
	col += grain * (0.020 + 0.032 * u_Dread);

	FragColor = vec4(max(col, vec3(0.0)), 1.0);
}
