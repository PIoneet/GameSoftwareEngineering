#version 330

in vec2 v_Local;
layout(location=0) out vec4 FragColor;

uniform float u_Time;
uniform float u_Seed;        // 불꽃마다 다른 위상
uniform float u_Intensity;

float Hash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float Noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);

	f = f * f * (3.0 - 2.0 * f);

	float a = Hash(i);
	float b = Hash(i + vec2(1.0, 0.0));
	float c = Hash(i + vec2(0.0, 1.0));
	float d = Hash(i + vec2(1.0, 1.0));

	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main()
{
	float h = 1.0 - v_Local.y;          // 0 = 밑동, 1 = 끝
	float x = v_Local.x - 0.5;

	// 위로 흐르는 노이즈가 불꽃을 흔든다
	float n   = Noise(vec2(v_Local.x * 3.0 + u_Seed, h * 2.4 - u_Time * 2.2));
	float wob = (n - 0.5) * 0.34 * h;

	// 위로 갈수록 좁아지는 단면
	float width = 0.30 * (1.0 - h * 0.82) + 0.04;
	float d     = abs(x + wob) / max(width, 0.001);

	float body = (1.0 - smoothstep(0.55, 1.0, d)) * (1.0 - smoothstep(0.62, 1.0, h));
	body *= u_Intensity;

	if (body <= 0.003)
		discard;

	// 심지는 희고 끝으로 갈수록 붉어진다
	vec3 hot = vec3(1.00, 0.94, 0.72);
	vec3 mid = vec3(1.00, 0.58, 0.16);
	vec3 tip = vec3(0.78, 0.18, 0.06);

	float t   = clamp(h * 1.35 + d * 0.45, 0.0, 1.0);
	vec3  col = mix(hot, mid, smoothstep(0.0, 0.5, t));

	col = mix(col, tip, smoothstep(0.5, 1.0, t));

	FragColor = vec4(col, clamp(body, 0.0, 1.0) * 0.92);
}
