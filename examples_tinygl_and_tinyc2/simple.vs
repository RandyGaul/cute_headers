#version 410

uniform mat4 u_mvp;

in vec2 in_pos;
in vec3 in_col;

out vec3 v_col;

void main()
{
	v_col = in_col;
	gl_Position = u_mvp * vec4(in_pos, 0, 1);
}
