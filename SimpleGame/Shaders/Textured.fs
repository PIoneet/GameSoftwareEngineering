#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Tex;
uniform vec4      u_Color;

// 글리프 아틀라스는 커버리지 한 채널만 담는다.
void main()
{
	float a = texture(u_Tex, v_UV).r;
	FragColor = vec4(u_Color.rgb, u_Color.a * a);
}
