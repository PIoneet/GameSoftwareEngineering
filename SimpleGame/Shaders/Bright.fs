#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform float     u_Threshold;

// 블룸용 밝은 영역 추출. 창문 불빛·모닥불·비석의 인광만 걸러낸다.
void main()
{
	vec3  c   = texture(u_Scene, v_UV).rgb;
	float lum = dot(c, vec3(0.299, 0.587, 0.114));

	float k = max(lum - u_Threshold, 0.0) / max(lum, 0.0001);

	FragColor = vec4(c * k, 1.0);
}
