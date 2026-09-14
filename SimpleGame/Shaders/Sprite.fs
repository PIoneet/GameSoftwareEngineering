#version 330

layout(location=0) out vec4 FragColor;

uniform vec4 u_Color;

void main()
{
	FragColor = u_Color;
}
