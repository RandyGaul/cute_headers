#version 410

in vec2 in_pos;
in vec2 in_uv;

out vec2 v_uv;

void main()
{
	v_uv = in_uv;
	gl_Position = vec4(in_pos.x, in_pos.y, 0.0f, 1.0f);
}