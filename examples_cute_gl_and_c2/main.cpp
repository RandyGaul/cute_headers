#include <glad/glad.h>
#include <glfw/glfw_config.h>
#include <glfw/glfw3.h>

#define CUTE_GL_IMPLEMENTATION
#include <cute_gl.h>

#define CUTE_TIME_IMPLEMENTATION
#include <cute_time.h>

#define CUTE_C2_IMPLEMENTATION
#include <cute_c2.h>

GLFWwindow* window;
float projection[16];
gl_shader_t simple;
int use_post_fx = 0;
gl_framebuffer_t fb;
gl_shader_t post_fx;
int spaced_pressed;
int arrow_pressed;
void* ctx;
float screen_w;
float screen_h;
c2v mp;
float wheel;

c2Circle user_circle;
c2Capsule user_capsule;
float user_rotation;

void* ReadFileToMemory(const char* path, int* size)
{
	void* data = 0;
	FILE* fp = fopen(path, "rb");
	int sizeNum = 0;

	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		sizeNum = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		data = malloc(sizeNum + 1);
		fread(data, sizeNum, 1, fp);
		((char*)data)[sizeNum] = 0;
		fclose(fp);
	}

	if (size) *size = sizeNum;
	return data;
}

void ErrorCB(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

void KeyCB(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);

	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		spaced_pressed = 1;

	if (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT && action == GLFW_PRESS)
		arrow_pressed = 1;

	if (key == GLFW_KEY_P && action == GLFW_PRESS)
		use_post_fx = !use_post_fx;
}

void ScrollCB(GLFWwindow* window, double x, double y)
{
	(void)x;
	wheel = (float)y;
}

void Rotate(c2v* src, c2v* dst, int count)
{
	if (!wheel) return;
	c2r r = c2Rot(wheel > 0 ? 3.14159265f / 16.0f : -3.14159265f / 16.0f);
	for (int i = 0; i < count; ++i) dst[i] = c2Mulrv(r, src[i]);
}

c2Capsule GetCapsule()
{
	c2Capsule cap = user_capsule;
	cap.a = c2Add(mp, cap.a);
	cap.b = c2Add(mp, cap.b);
	return cap;
}

int mouse_pressed;
float mouse_x;
float mouse_y;

void MouseButtonCB(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS) {
		if (button == GLFW_MOUSE_BUTTON_1) {
			mouse_pressed = 1;
		}
	} else if (action == GLFW_RELEASE) {
		if (button == GLFW_MOUSE_BUTTON_1) {
			mouse_pressed = 0;
		}
	}
}

void MouseCB(GLFWwindow* window, double x, double y)
{
	mouse_x = (float)x - screen_w / 2;
	mouse_y = -((float)y - screen_h / 2);
	mp = c2V(mouse_x, mouse_y);

	user_circle.p = mp;
	user_circle.r = 10.0f;
}

void ResizeFramebuffer(int w, int h)
{
	static int first = 1;
	if (first) 
	{
		first = 0;
		char* vs = (char*)ReadFileToMemory("postprocess.vs", 0);
		char* ps = (char*)ReadFileToMemory("postprocess.ps", 0);
		gl_load_shader(&post_fx, vs, ps);
		free(vs);
		free(ps);
	}
	else gl_free_frame_buffer(&fb);
	screen_w = (float)w;
	screen_h = (float)h;
	gl_make_frame_buffer(&fb, &post_fx, w, h, 0);
}

void Reshape(GLFWwindow* window, int width, int height)
{
	gl_ortho_2d((float)width / 1.0f, (float)height / 1.0f, 0, 0, projection);
	glViewport(0, 0, width, height);
	ResizeFramebuffer(width, height);
}

void SwapBuffers()
{
	glfwSwapBuffers(window);
}

// enable depth test here if you care, also clear
void GLSettings()
{
}

#include <vector>

struct Color
{
	float r;
	float g;
	float b;
};

struct Vertex
{
	c2v pos;
	Color col;
};

std::vector<Vertex> verts;

void DrawPoly(c2v* verts, int count)
{
	for (int i = 0; i < count; ++i)
	{
		int iA = i;
		int iB = (i + 1) % count;
		c2v a = verts[iA];
		c2v b = verts[iB];
		gl_line(ctx, a.x, a.y, 0, b.x, b.y, 0);
	}
}

void DrawNormals(c2v* verts, c2v* norms, int count)
{
	for (int i = 0; i < count; ++i)
	{
		int iA = i;
		int iB = (i + 1) % count;
		c2v a = verts[iA];
		c2v b = verts[iB];
		c2v p = c2Mulvs(c2Add(a, b), 0.5f);
		c2v n = norms[iA];
		gl_line(ctx, p.x, p.y, 0, p.x + n.x, p.y + n.y, 0);
	}
}

void DrawPoly2(c2Poly* p, c2x x)
{
	for (int i = 0; i < p->count; ++i)
	{
		int iA = i;
		int iB = (i + 1) % p->count;
		c2v a = c2Mulxv(x, p->verts[iA]);
		c2v b = c2Mulxv(x, p->verts[iB]);
		gl_line(ctx, a.x, a.y, 0, b.x, b.y, 0);
	}
}

void DrawAABB(c2v a, c2v b)
{
	c2v c = c2V(a.x, b.y);
	c2v d = c2V(b.x, a.y);
	gl_line(ctx, a.x, a.y, 0, c.x, c.y, 0);
	gl_line(ctx, c.x, c.y, 0, b.x, b.y, 0);
	gl_line(ctx, b.x, b.y, 0, d.x, d.y, 0);
	gl_line(ctx, d.x, d.y, 0, a.x, a.y, 0);
}

void DrawHalfCircle(c2v a, c2v b)
{
	c2v u = c2Sub(b, a);
	float r = c2Len(u);
	u = c2Skew(u);
	c2v v = c2CCW90(u);
	c2v s = c2Add(v, a);
	c2m m;
	m.x = c2Norm(u);
	m.y = c2Norm(v);

	int kSegs = 20;
	float theta = 0;
	float inc = 3.14159265f / (float)kSegs;
	c2v p0;
	c2SinCos(theta, &p0.y, &p0.x);
	p0 = c2Mulvs(p0, r);
	p0 = c2Add(c2Mulmv(m, p0), a);
	for (int i = 0; i < kSegs; ++i)
	{
		theta += inc;
		c2v p1;
		c2SinCos(theta, &p1.y, &p1.x);
		p1 = c2Mulvs(p1, r);
		p1 = c2Add(c2Mulmv(m, p1), a);
		gl_line(ctx, p0.x, p0.y, 0, p1.x, p1.y, 0);
		p0 = p1;
	}
}

void DrawCapsule(c2v a, c2v b, float r)
{
	c2v n = c2Norm(c2Sub(b, a));
	DrawHalfCircle(a, c2Add(a, c2Mulvs(n, -r)));
	DrawHalfCircle(b, c2Add(b, c2Mulvs(n, r)));
	c2v p0 = c2Add(a, c2Mulvs(c2Skew(n), r));
	c2v p1 = c2Add(b, c2Mulvs(c2CCW90(n), -r));
	gl_line(ctx, p0.x, p0.y, 0, p1.x, p1.y, 0);
	p0 = c2Add(a, c2Mulvs(c2Skew(n), -r));
	p1 = c2Add(b, c2Mulvs(c2CCW90(n), r));
	gl_line(ctx, p0.x, p0.y, 0, p1.x, p1.y, 0);
}

void DrawCircle(c2v p, float r)
{
	int kSegs = 40;
	float theta = 0;
	float inc = 3.14159265f * 2.0f / (float)kSegs;
	float px, py;
	c2SinCos(theta, &py, &px);
	px *= r; py *= r;
	px += p.x; py += p.y;
	for (int i = 0; i <= kSegs; ++i)
	{
		theta += inc;
		float x, y;
		c2SinCos(theta, &y, &x);
		x *= r; y *= r;
		x += p.x; y += p.y;
		gl_line(ctx, x, y, 0, px, py, 0);
		px = x; py = y;
	}
}

void DrawManifold(c2Manifold m)
{
	c2v n = m.n;
	gl_line_color(ctx, 1.0f, 0.2f, 0.4f);
	for (int i = 0; i < m.count; ++i)
	{
		c2v p = m.contact_points[i];
		float d = m.depths[i];
		DrawCircle(p, 3.0f);
		gl_line(ctx, p.x, p.y, 0, p.x + n.x * d, p.y + n.y * d, 0);
	}
}

// should see slow rotation CCW, then CW
// space toggles between two different rotation implements
// after toggling implementations space toggles rotation direction
void TestRotation()
{
	static int first = 1;
	static Vertex v[3];
	if (first)
	{
		first = 0;
		Color c = { 1.0f, 0.0f, 0.0f };
		v[0].col = c;
		v[1].col = c;
		v[2].col = c;
		v[0].pos = c2V(0, 100);
		v[1].pos = c2V(0, 0);
		v[2].pos = c2V(100, 0);
	}

	static int which0;
	static int which1;
	if (spaced_pressed) which0 = !which0;
	if (spaced_pressed && which0) which1 = !which1;

	if (which0)
	{
		c2m m;
		m.x = c2Norm(c2V(1, 0.01f));
		m.y = c2Skew(m.x);
		for (int i = 0; i < 3; ++i)
			v[i].pos = which1 ? c2Mulmv(m, v[i].pos) : c2MulmvT(m, v[i].pos);
	}

	else
	{
		c2r r = c2Rot(0.01f);
		for (int i = 0; i < 3; ++i)
			v[i].pos = which1 ? c2Mulrv(r, v[i].pos) : c2MulrvT(r, v[i].pos);
	}

	for (int i = 0; i < 3; ++i)
		verts.push_back(v[i]);
}

void TestDrawPrim()
{
	TestRotation();

	gl_line_color(ctx, 0.2f, 0.6f, 0.8f);
	gl_line(ctx, 0, 0, 0, 100, 100, 0);
	gl_line_color(ctx, 0.8f, 0.6f, 0.2f);
	gl_line(ctx, 100, 100, 0, -100, 200, 0);

	DrawCircle(c2V(0, 0), 100.0f);

	gl_line_color(ctx, 0, 1.0f, 0);
	DrawHalfCircle(c2V(0, 0), c2V(50, -50));

	gl_line_color(ctx, 0, 0, 1.0f);
	DrawCapsule(c2V(0, 200), c2V(75, 150), 20.0f);

	gl_line_color(ctx, 1.0f, 0, 0);
	DrawAABB(c2V(-20, -20), c2V(20, 20));

	gl_line_color(ctx, 0.5f, 0.9f, 0.1f);
	c2v poly[] = {
		{ 0, 0 },
		{ 20.0f, 10.0f },
		{ 5.0f, 15.0f },
		{ -3.0f, 7.0f },
	};
	DrawPoly(poly, 4);
}

void TestBoolean0()
{
	c2AABB aabb;
	aabb.min = c2V(-40.0f, -40.0f);
	aabb.max = c2V(-15.0f, -15.0f);

	c2Circle circle;
	circle.p = c2V(-70.0f, 0);
	circle.r = 20.0f;

	c2Capsule capsule;
	capsule.a = c2V(-40.0f, 40.0f);
	capsule.b = c2V(-20.0f, 100.0f);
	capsule.r = 10.0f;

	if (c2CircletoCircle(user_circle, circle)) gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
	else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
	DrawCircle(circle.p, circle.r);

	if (c2CircletoAABB(user_circle, aabb)) gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
	else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
	DrawAABB(aabb.min, aabb.max);

	if (c2CircletoCapsule(user_circle, capsule)) gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
	else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
	DrawCapsule(capsule.a, capsule.b, capsule.r);

	gl_line_color(ctx, 0.5f, 0.7f, 0.9f);
	DrawCircle(user_circle.p, user_circle.r);
}

void TestBoolean1()
{
	c2AABB bb;
	bb.min = c2V(-100.0f, -30.0f);
	bb.max = c2V(-50.0f, 30.0f);
	c2Capsule cap = GetCapsule();

	c2v a, b;
	c2GJK(&bb, C2_TYPE_AABB, 0, &cap, C2_TYPE_CAPSULE, 0, &a, &b, 1, 0, 0);
	DrawCircle(a, 2.0f);
	DrawCircle(b, 2.0f);
	gl_line(ctx, a.x, a.y, 0, b.x, b.y, 0);

	if (c2AABBtoCapsule(bb, cap)) gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
	else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
	DrawAABB(bb.min, bb.max);

	gl_line_color(ctx, 0.5f, 0.7f, 0.9f);
	DrawCapsule(cap.a, cap.b, cap.r);
}

float randf()
{
	float r = (float)(rand() & RAND_MAX);
	r /= RAND_MAX;
	r = 2.0f * r - 1.0f;
	return r;
}

c2v RandomVec()
{
	return c2V(randf() * 100.0f, randf() * 100.0f);
}

void TestBoolean2()
{
	static c2Poly poly;
	static c2Poly poly2;
	static int first = 1;
	if (first)
	{
		first = 0;
		poly.count = C2_MAX_POLYGON_VERTS;
		for (int i = 0; i < poly.count; ++i) poly.verts[i] = RandomVec();
		c2MakePoly(&poly);
		poly2.count = C2_MAX_POLYGON_VERTS;
		for (int i = 0; i < poly2.count; ++i) poly2.verts[i] = RandomVec();
		c2MakePoly(&poly2);
	}

	static int which;
	if (spaced_pressed) which = (which + 1) % 4;
	if (wheel) Rotate(poly2.verts, poly2.verts, poly2.count);

	switch (which)
	{
	case 0:
	{
		c2v a, b;
		c2GJK(&user_circle, C2_TYPE_CIRCLE, 0, &poly, C2_TYPE_POLY, 0, &a, &b, 1, 0, 0);
		DrawCircle(a, 2.0f);
		DrawCircle(b, 2.0f);
		gl_line(ctx, a.x, a.y, 0, b.x, b.y, 0);

#if 0
		if (c2CircletoPoly(user_circle, &poly, 0)) gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
		else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
#else
		c2Manifold m;
		c2CircletoPolyManifold(user_circle, &poly, 0, &m);
		if (m.count)
		{
			DrawManifold(m);
		}
#endif
		DrawPoly(poly.verts, poly.count);

		gl_line_color(ctx, 0.5f, 0.7f, 0.9f);
		DrawCircle(user_circle.p, user_circle.r);
	}	break;

	case 1:
	{
		c2v a, b;
		c2AABB bb;
		bb.min = c2V(-10.0f, -10.0f);
		bb.max = c2V(10.0f, 10.0f);
		bb.min = c2Add(bb.min, mp);
		bb.max = c2Add(bb.max, mp);
		c2GJK(&bb, C2_TYPE_AABB, 0, &poly, C2_TYPE_POLY, 0, &a, &b, 1, 0, 0);
		DrawCircle(a, 2.0f);
		DrawCircle(b, 2.0f);
		gl_line(ctx, a.x, a.y, 0, b.x, b.y, 0);

		if (c2AABBtoPoly(bb, &poly, 0)) gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
		else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
		DrawPoly(poly.verts, poly.count);

		gl_line_color(ctx, 0.5f, 0.7f, 0.9f);
		DrawAABB(bb.min, bb.max);
	}	break;

	case 2:
	{
		c2v a, b;
		c2Capsule cap = GetCapsule();
		c2GJK(&cap, C2_TYPE_CAPSULE, 0, &poly, C2_TYPE_POLY, 0, &a, &b, 1, 0, 0);
		DrawCircle(a, 2.0f);
		DrawCircle(b, 2.0f);
		gl_line(ctx, a.x, a.y, 0, b.x, b.y, 0);

		if (c2CapsuletoPoly(cap, &poly, 0)) gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
		else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
		DrawPoly(poly.verts, poly.count);

		gl_line_color(ctx, 0.5f, 0.7f, 0.9f);
		DrawCapsule(cap.a, cap.b, cap.r);
	}	break;

	case 3:
	{
		c2v a, b;
		c2Poly poly3;
		for (int i = 0; i < poly2.count; ++i) poly3.verts[i] = c2Add(mp, poly2.verts[i]);
		poly3.count = poly2.count;

		c2GJK(&poly, C2_TYPE_POLY, 0, &poly3, C2_TYPE_POLY, 0, &a, &b, 1, 0, 0);
		DrawCircle(a, 2.0f);
		DrawCircle(b, 2.0f);
		gl_line(ctx, a.x, a.y, 0, b.x, b.y, 0);

		if (c2PolytoPoly(&poly, 0, &poly3, 0)) gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
		else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
		DrawPoly(poly.verts, poly.count);

		gl_line_color(ctx, 0.5f, 0.7f, 0.9f);
		DrawPoly(poly3.verts, poly3.count);
	}	break;
	}
}

void TestRay0()
{
	c2Circle c;
	c.p = c2V(0, 0);
	c.r = 20.0f;

	c2AABB bb;
	bb.min = c2V(30.0f, 30.0f);
	bb.max = c2V(70.0f, 70.0f);

	c2Ray ray;
	ray.p = c2V(-100.0f, 100.0f);
	ray.d = c2Norm(c2Sub(mp, ray.p));
	ray.t = c2Dot(mp, ray.d) - c2Dot(ray.p, ray.d);

	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawCircle(c.p, c.r);
	DrawAABB(bb.min, bb.max);

	c2Raycast cast;
	int hit = c2RaytoCircle(ray, c, &cast);
	if (hit)
	{
		ray.t = cast.t;
		c2v impact = c2Impact(ray, ray.t);
		c2v end = c2Add(impact, c2Mulvs(cast.n, 15.0f));
		gl_line_color(ctx, 1.0f, 0.2f, 0.4f);
		gl_line(ctx, impact.x, impact.y, 0, end.x, end.y, 0);
		gl_line(ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0);
	}

	else
	{

		ray.d = c2Norm(c2Sub(mp, ray.p));
		ray.t = c2Dot(mp, ray.d) - c2Dot(ray.p, ray.d);

		if (c2RaytoAABB(ray, bb, &cast))
		{
			ray.t = cast.t;
			c2v impact = c2Impact(ray, ray.t);
			c2v end = c2Add(impact, c2Mulvs(cast.n, 15.0f));
			gl_line_color(ctx, 1.0f, 0.2f, 0.4f);
			gl_line(ctx, impact.x, impact.y, 0, end.x, end.y, 0);
		}
		else gl_line_color(ctx, 1.0f, 1.0f, 1.0f);

		gl_line(ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0);
	}
}

void TestRay1()
{
	c2Capsule cap;
	cap.a = c2V(-100.0f, 60.0f);
	cap.b = c2V(50.0f, -40.0f);
	cap.r = 20.0f;

	static float x = 75.0f;
	static float y = 100.0f;
	if (mouse_pressed) {
		x = mouse_x;
		y = mouse_y;
	}

	c2Ray ray;
	ray.p = c2V(x, y);
	ray.d = c2Norm(c2Sub(mp, ray.p));
	ray.t = c2Dot(mp, ray.d) - c2Dot(ray.p, ray.d);

	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawCapsule(cap.a, cap.b, cap.r);
	c2Raycast cast;
	if (c2RaytoCapsule(ray, cap, &cast))
	{
		ray.t = cast.t;
		c2v impact = c2Impact(ray, ray.t);
		c2v end = c2Add(impact, c2Mulvs(cast.n, 15.0f));
		gl_line_color(ctx, 1.0f, 0.2f, 0.4f);
		gl_line(ctx, impact.x, impact.y, 0, end.x, end.y, 0);
	}

	gl_line(ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0);
}

void TestRay2()
{
	static c2Poly poly;
	static int first = 1;
	if (first)
	{
		first = 0;
		poly.count = C2_MAX_POLYGON_VERTS;
		for (int i = 0; i < poly.count; ++i) poly.verts[i] = RandomVec();
		c2MakePoly(&poly);
	}

	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawPoly(poly.verts, poly.count);

	c2Ray ray;
	ray.p = c2V(-75.0f, 100.0f);
	ray.d = c2Norm(c2Sub(mp, ray.p));
	ray.t = c2Dot(mp, ray.d) - c2Dot(ray.p, ray.d);

	c2Raycast cast;
	if (c2RaytoPoly(ray, &poly, 0, &cast))
	{
		ray.t = cast.t;
		c2v impact = c2Impact(ray, ray.t);
		c2v end = c2Add(impact, c2Mulvs(cast.n, 15.0f));
		gl_line_color(ctx, 1.0f, 0.2f, 0.4f);
		gl_line(ctx, impact.x, impact.y, 0, end.x, end.y, 0);
	}

	gl_line(ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0);
}

void DrawCircles(c2Circle ca, c2Circle cb)
{
	c2Manifold m;
	m.count = 0;
	c2CircletoCircleManifold(ca, cb, &m);
	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawCircle(ca.p, ca.r);
	DrawCircle(cb.p, cb.r);
	DrawManifold(m);
}

void DrawCircleAABB(c2Circle c, c2AABB bb)
{
	c2Manifold m;
	m.count = 0;
	c2CircletoAABBManifold(c, bb, &m);
	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawCircle(c.p, c.r);
	DrawAABB(bb.min, bb.max);
	DrawManifold(m);
}

void DrawCircleCapsule(c2Circle c, c2Capsule cap)
{
	c2Manifold m;
	m.count = 0;
	c2CircletoCapsuleManifold(c, cap, &m);
	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawCircle(c.p, c.r);
	DrawCapsule(cap.a, cap.b, cap.r);
	DrawManifold(m);
}

void DrawBB(c2AABB ba, c2AABB bb)
{
	c2Manifold m;
	m.count = 0;
	c2AABBtoAABBManifold(ba, bb, &m);
	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawAABB(ba.min, ba.max);
	DrawAABB(bb.min, bb.max);
	DrawManifold(m);
}

void TestManifold0()
{
	c2Circle ca;
	ca.p = c2V(-200.0f, 0);
	ca.r = 20.0f;
	c2Circle cb;
	cb.p = c2V(-220.0f, 10.0f);
	cb.r = 15.0f;
	DrawCircles(ca, cb);

	cb.p = ca.p;
	cb.r = 10.0f;
	DrawCircles(ca, cb);

	c2AABB bb;
	bb.min = c2V(-150.0f, 20.0f);
	bb.max = c2V(-60.0f, 140.0f);

	// outside
	ca.p = c2V(-160.0f, 80.0f);
	ca.r = 15.0f;
	DrawCircleAABB(ca, bb);

	ca.p = c2V(-120.0f, 150.0f);
	ca.r = 15.0f;
	DrawCircleAABB(ca, bb);

	ca.p = c2V(-50.0f, 100.0f);
	ca.r = 15.0f;
	DrawCircleAABB(ca, bb);

	ca.p = c2V(-120.0f, 10.0f);
	ca.r = 15.0f;
	DrawCircleAABB(ca, bb);

	// inside
	ca.p = c2V(-140.0f, 60.0f);
	ca.r = 10.0f;
	DrawCircleAABB(ca, bb);

	ca.p = c2V(-100.0f, 40.0f);
	ca.r = 10.0f;
	DrawCircleAABB(ca, bb);

	ca.p = c2V(-80.0f, 70.0f);
	ca.r = 10.0f;
	DrawCircleAABB(ca, bb);

	ca.p = c2V(-80.0f, 130.0f);
	ca.r = 10.0f;
	DrawCircleAABB(ca, bb);

	// capsule things
	c2Capsule cap;
	cap.a = c2V(100.0f, 0);
	cap.b = c2V(250.0f, 50.0f);
	cap.r = 20.0f;
	ca.p = c2V(120.0f, 30.0f);
	ca.r = 25.0f;
	DrawCircleCapsule(ca, cap);

	ca.p = c2V(150.0f, 45.0f);
	ca.r = 15.0f;
	DrawCircleCapsule(ca, cap);

	ca.p = c2V(100.0f, 0);
	ca.r = 15.0f;
	DrawCircleCapsule(ca, cap);

	ca.p = c2V(260.0f, 60.0f);
	ca.r = 10.0f;
	DrawCircleCapsule(ca, cap);

	// bb things
	c2AABB ba;
	ba.min = c2V(-50.0f, -200.0f);
	ba.max = c2V(50.0f, -100.0f);
	bb.min = c2V(-10.0f, -110.0f);
	bb.max = c2V(10.0f, -80.0f);
	DrawBB(ba, bb);

	bb.min = c2V(20.0f, -140.0f);
	bb.max = c2V(40.0f, -110.0f);
	DrawBB(ba, bb);

	bb.min = c2V(-20.0f, -140.0f);
	bb.max = c2V(-40.0f, -110.0f);
	DrawBB(ba, bb);

	bb.min = c2V(-10.0f, -205.0f);
	bb.max = c2V(10.0f, -190.0f);
	DrawBB(ba, bb);

	//bb.min = c2Sub(mp, c2V(20.0f, 5.0f));
	//bb.max = c2Add(mp, c2V(20.0f, 5.0f));
	//DrawBB(ba, bb);
}

void TestManifold1()
{
	static c2Poly a;
	static c2Poly b;
	c2x ax = c2Transform(c2V(-50.0f, 0), 2.0f);
	c2x bx = c2Transform(mp, -1.0f);

	static int which = 0;
	if (spaced_pressed) which = !which;
	if (which)
	{
		srand(2);
		a.count = C2_MAX_POLYGON_VERTS;
		for (int i = 0; i < a.count; ++i) a.verts[i] = RandomVec();
		c2MakePoly(&a);
		b.count = C2_MAX_POLYGON_VERTS;
		for (int i = 0; i < b.count; ++i) b.verts[i] = RandomVec();
		c2MakePoly(&b);
		static float r;
		if (wheel) r += wheel;
		bx.r = c2Rot(-1.0f + r * 0.2f);
		bx.p = mp;
	}

	else
	{
		ax = c2xIdentity();
		bx = c2xIdentity();
		c2AABB ba;
		c2AABB bb;
		ba.min = c2V(-20.0f, -20.0f);
		ba.max = c2V(20.0f, 20.0f);
		bb.min = c2V(-40.0f, -40.0f);
		bb.max = c2V(-20.0f, -20.0f);
		ax.r = c2Rot(-1.0f);
		ax.p = c2V(50.0f, -50.0f);
		bx.p = mp;
		bx.r = c2Rot(1.0f);

		c2BBVerts(a.verts, &ba);
		a.count = 4;
		c2Norms(a.verts, a.norms, 4);

		c2BBVerts(b.verts, &bb);
		b.count = 4;
		c2Norms(b.verts, b.norms, 4);
	}

	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawPoly2(&a, ax);
	DrawPoly2(&b, bx);

	c2Manifold m;
	m.count = 0;
	c2PolytoPolyManifold(&a, &ax, &b, &bx, &m);
	DrawManifold(m);
}

void TestManifold2()
{
	static c2Poly a;
	c2x ax = c2Transform(c2V(-50.0f, 0), 2.0f);
	srand(3);
	a.count = C2_MAX_POLYGON_VERTS;
	for (int i = 0; i < a.count; ++i) a.verts[i] = RandomVec();
	c2MakePoly(&a);

	c2Capsule cap = GetCapsule();

	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DrawPoly2(&a, ax);
	DrawCapsule(cap.a, cap.b, cap.r);

	c2Manifold m;
	m.count = 0;
	c2CapsuletoPolyManifold(cap, &a, &ax, &m);
	DrawManifold(m);
}

void PlastburkRayBug()
{
	c2Poly p;
	p.verts[0] = c2V(0.875f, -11.5f);
	p.verts[1] = c2V(0.875f, 11.5f);
	p.verts[2] = c2V(-0.875f, 11.5f);
	p.verts[3] = c2V(-0.875f, -11.5f);
	p.norms[0] = c2V(1, 0);
	p.norms[1] = c2V(0, 1);
	p.norms[2] = c2V(-1, 0);
	p.norms[3] = c2V(0, -1);
	p.count = 4;

	c2Ray ray0 = { {-3.869416f, 13.0693407f}, {1, 0}, 4 };
	c2Ray ray1 = { {-3.869416f, 13.0693407f}, {0, -1}, 4 };

	c2Raycast out0;
	c2Raycast out1;
	int hit0 = c2RaytoPoly(ray0, &p, 0, &out0);
	int hit1 = c2RaytoPoly(ray0, &p, 0, &out1);

#define DBG_DRAW_RAY(ray) \
	gl_line(ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0)

	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	DBG_DRAW_RAY(ray0);
	DBG_DRAW_RAY(ray1);
	DrawPoly(p.verts, p.count);
	DrawNormals(p.verts, p.norms, p.count);

	if (hit0)
	{
		ray0.t = out0.t;
		c2v impact = c2Impact(ray0, ray0.t);
		c2v end = c2Add(impact, c2Mulvs(out0.n, 1.0f));
		gl_line_color(ctx, 1.0f, 0.2f, 0.4f);
		gl_line(ctx, impact.x, impact.y, 0, end.x, end.y, 0);
	}

	if (hit1)
	{
		ray1.t = out1.t;
		c2v impact = c2Impact(ray1, ray1.t);
		c2v end = c2Add(impact, c2Mulvs(out1.n, 1.0f));
		gl_line_color(ctx, 1.0f, 0.2f, 0.4f);
		gl_line(ctx, impact.x, impact.y, 0, end.x, end.y, 0);
	}
}

void sro5hRayBug()
{
	c2Ray ray;
	ray.p = c2V(100.0f, 100.0f);
	ray.d = c2Norm(c2V(100.0f, 100.0f));
	ray.t = 1.0f;

	c2Circle circle;
	circle.r = 30.0f;
	circle.p = c2V(200.0f, 200.0f);

	DrawCircle(circle.p, circle.r);

	c2Raycast cast;
	if (c2RaytoCircle(ray, circle, &cast))
	{
		c2v impact = c2Impact(ray, cast.t);
		c2v end = c2Add(impact, c2Mulvs(cast.n, 10.0f));
		gl_line_color(ctx, 1.0f, 0.2f, 0.4f);
		gl_line(ctx, impact.x, impact.y, 0, end.x, end.y, 0);
	}

	c2v end = c2Add(ray.p, c2Mulvs(ray.d, ray.t));
	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	gl_line(ctx, ray.p.x, ray.p.y, 0, end.x, end.y, 0);
}

void circle_to_aabb_bug()
{
	c2Circle a;
	a.p = mp;
	a.r = 10.0f;

	c2AABB b;
	b.min = c2V(-100, -50);
	b.max = c2V(100, 50);

	gl_line_color(ctx, 1, 1, 1);
	DrawCircle(a.p, a.r);
	DrawAABB(b.min, b.max);

	c2Manifold m;
	c2CircletoAABBManifold(a, b, &m);

	if (m.count)
	{
		DrawManifold(m);
	}
}

void DJLink_aabb_bug()
{
	bool draw_aabb = false;
	
	c2AABB ba;
	ba.min = c2V(-50.0f, -200.0f);
	ba.max = c2V(50.0f, -100.0f);

	c2AABB bb;
	bb.min = c2V(-10.0f, -225.0f);
	bb.max = c2V(50.0f, -180.0f); //<= if using 49 instead of 50 collision will work
	
	if (draw_aabb)
	{
		DrawBB(ba, bb);
	}
	else
	{
		//Dirty conversion for poly just for tests
		c2Poly p1;
		c2BBVerts(p1.verts, &ba);
		p1.count = 4;
		c2Norms(p1.verts, p1.norms, 4);

		c2Poly p2;
		c2BBVerts(p2.verts, &bb);
		p2.count = 4;
		c2Norms(p2.verts, p2.norms, 4);

		c2x cx = c2xIdentity();
		gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
		DrawPoly2(&p1, cx);
		DrawPoly2(&p2, cx);

		c2Manifold m;
		m.count = 0;
		c2PolytoPolyManifold(&p1, nullptr, &p2, nullptr, &m);
		DrawManifold(m);
	}
}

void lundmark_GJK_div_by_0_bug()
{
	c2Circle A;
	c2Capsule B;
	A = { { 1147.21912f, 1464.05212f }, 2.0f };
	B = { { 1133.07214f, 1443.59570f }, { 1127.39636f, 1440.69470f }, 6.0f };

	c2v a, b;
	float d = c2GJK(&A, C2_TYPE_CIRCLE, 0, &B, C2_TYPE_CAPSULE, 0, &a, &b, 1, 0, 0);
}

void gjk_make_sure_cache_helps_and_works()
{
	c2Circle A;
	c2Capsule B;
	A = { { 0, 0 }, 15.0f };
	B = { { 100, -25 }, { 75, 100 }, 10 };

	c2v a0, b0;
	c2v a, b;
	c2GJKCache cache;
	cache.count = 0;
	int iterations = -1;
	int cached_iterations = -1;
	float d0 = c2GJK(&A, C2_TYPE_CIRCLE, 0, &B, C2_TYPE_CAPSULE, 0, &a0, &b0, 1, &iterations, &cache);
	float d1 = c2GJK(&A, C2_TYPE_CIRCLE, 0, &B, C2_TYPE_CAPSULE, 0, &a, &b, 1, &cached_iterations, &cache);

	gl_line_color(ctx, 1, 1, 1);
	DrawCircle(A.p, A.r);
	DrawCapsule(B.a, B.b, B.r);
	gl_line(ctx, a.x, a.y, 0, b.x, b.y, 0);

	assert(iterations != -1);
	assert(cached_iterations != -1);
	assert(iterations > cached_iterations);
	assert(a0.x == a.x && a0.y == a.y);
	assert(b0.x == b.x && b0.y == b.y);
	assert(d0 == d1);
}

void try_out_toi_via_conservative_advancment()
{
	c2Circle A;
	c2Capsule B;
	A = { { mp }, 15.0f };
	B = { { 125, -50 }, { 75, 50 }, 20 };

	static c2v vA = c2V(150, -50);
	c2v vB = c2V(0, 0);
	Rotate(&vA, &vA, 1);

	int iterations = -1;
	int use_radius = 1;
	float t = c2TOI(&A, C2_TYPE_CIRCLE, NULL, vA, &B, C2_TYPE_CAPSULE, NULL, vB, use_radius, &iterations);

	gl_line_color(ctx, 1, 1, 1);
	DrawCircle(A.p, A.r);
	DrawCapsule(B.a, B.b, B.r);
	
	if (t != 1) gl_line_color(ctx, 1, 0, 0);
	gl_line(ctx, A.p.x, A.p.y, 0, A.p.x + vA.x, A.p.y + vA.y, 0);

	A.p = c2Add(A.p, c2Mulvs(vA, t));
	B.a = c2Add(B.a, c2Mulvs(vB, t));
	B.b = c2Add(B.b, c2Mulvs(vB, t));

	DrawCircle(A.p, A.r);
}

void draw_point_normal(c2v p, c2v n, float len)
{
	n.x *= len;
	n.y *= len;
	gl_line(ctx, p.x, p.y, 0, p.x + n.x, p.y + n.y, 0);
	DrawCircle(p, 5.0f);
}

void prime31_bad_toi_normal()
{
	c2Circle circle;
	circle.p = c2V(-300, 100);
	circle.r = 50;

	c2AABB aabb;
	aabb.min = c2V(-100, -100);
	aabb.max = c2V(100, 100);

	c2v vel = c2V(5000, 0);
	float toi = c2TOI(&circle, C2_TYPE_CIRCLE, NULL, vel, &aabb, C2_TYPE_AABB, NULL, c2V(0, 0), 1, NULL);

	gl_line_color(ctx, 1, 1, 1);
	DrawCircle(circle.p, circle.r);
	DrawAABB(aabb.min, aabb.max);

	gl_line_color(ctx, 1, 0, 0);
	circle.p = c2Add(circle.p, c2Mulvs(vel, toi));
	DrawCircle(circle.p, circle.r);
}

void prime31_bad_toi_normal_animated()
{
	c2Circle circle;
	circle.p = c2V(0, 200);
	circle.r = 25;
	
	c2AABB aabb;
	aabb.min = c2V(-100, -100);
	aabb.max = c2V(100, 100);

	static uint64_t frame_count;
	frame_count++;
	int i = (frame_count / 3) % 75;
	{
		circle.p.x += 2 * i;
		c2v vel = c2V(0, -500);
		float toi = c2TOI(&circle, C2_TYPE_CIRCLE, NULL, vel, &aabb, C2_TYPE_AABB, NULL, c2V(0, 0), 1, NULL);

		gl_line_color(ctx, 1, 1, 1);
		DrawCircle(circle.p, circle.r);
		DrawAABB(aabb.min, aabb.max);

		gl_line_color(ctx, 1, 0, 0);
		circle.p = c2Add(circle.p, c2Mulvs(vel, toi));
		DrawCircle(circle.p, circle.r);
	}
}

void prime31_bad_toi_normal_animated_aabb()
{
	c2AABB aabb_top;
	aabb_top.min = c2V(-25, 150);
	aabb_top.max = c2V(25, 200);
	
	c2AABB aabb;
	aabb.min = c2V(-100, -100);
	aabb.max = c2V(100, 100);

	static uint64_t frame_count;
	frame_count++;
	int i = (frame_count / 3) % 75;
	{
		aabb_top.min.x += 2 * i;
		aabb_top.max.x += 2 * i;
		c2v vel = c2V(0, -500);
		float toi = c2TOI(&aabb_top, C2_TYPE_AABB, NULL, vel, &aabb, C2_TYPE_AABB, NULL, c2V(0, 0), 1, NULL);

		gl_line_color(ctx, 1, 1, 1);
		DrawAABB(aabb_top.min, aabb_top.max);
		DrawAABB(aabb.min, aabb.max);

		gl_line_color(ctx, 1, 0, 0);
		aabb_top.min = c2Add(aabb_top.min, c2Mulvs(vel, toi));
		aabb_top.max = c2Add(aabb_top.max, c2Mulvs(vel, toi));
		DrawAABB(aabb_top.min, aabb_top.max);
	}
}

void prime31_cap_to_aabb_bug()
{
	c2Capsule capsule;
	capsule.r = 25;
	capsule.a = c2V(0, 0);
	capsule.b = c2V(0, 50);

	c2AABB aabb;
	aabb.min = c2V(0, 0);
	aabb.max = c2V(100, 10);

	if (c2Collided(&capsule, NULL, C2_TYPE_CAPSULE, &aabb, NULL, C2_TYPE_AABB))
	{
		c2Manifold mc;
		c2AABBtoCapsuleManifold(aabb, capsule, &mc);
		gl_line_color(ctx, 1, 0, 0);
		DrawManifold(mc);
	}

	gl_line_color(ctx, 1, 1, 1);
	DrawCapsule(capsule.a, capsule.b, capsule.r);
	DrawAABB(aabb.min, aabb.max);
}

void prime31_cap_to_aabb_bug2()
{
	c2AABB bb;
	bb.min = c2V(-100.0f, -30.0f);
	bb.max = c2V(-50.0f, 30.0f);

#if 1
	c2Capsule capsule;
	capsule.r = 10;
	capsule.a = c2V(-25, 0);
	capsule.b = c2V(25, 0);

	static uint64_t frame_count;
	frame_count++;
	int i = (frame_count / 3) % 60;
	capsule.a.x -= i * 1.0f;
	capsule.b.x -= i * 1.0f;
#else
	c2Capsule capsule;
	capsule.r = 10;
	capsule.a = c2V(-75, 50);
	capsule.b = c2V(25, 50);

	static uint64_t frame_count;
	frame_count++;
	int i = (frame_count / 3) % 60;
	capsule.a.y -= i * 1.0f;
	capsule.b.y -= i * 1.0f;
#endif

	c2Manifold m;
#if 1
	c2Collide(&bb, NULL, C2_TYPE_AABB, &capsule, NULL, C2_TYPE_CAPSULE, &m);
#else
	c2Collide(&capsule, NULL, C2_TYPE_CAPSULE, &bb, NULL, C2_TYPE_AABB, &m);
#endif

	if (m.count)
	{
		DrawManifold(m);
		gl_line_color(ctx, 1.0f, 0.0f, 0.0f);
	}
	else gl_line_color(ctx, 5.0f, 7.0f, 9.0f);
	DrawAABB(bb.min, bb.max);

	gl_line_color(ctx, 0.5f, 0.7f, 0.9f);
	DrawCapsule(capsule.a, capsule.b, capsule.r);
}

void martincohen_ray_bug()
{
	static uint64_t frame_count;
	frame_count++;
	int i = (frame_count / 5) % 75;

	c2Ray ray = {
		{ -50, (float)i - 20 },
		{ 1, 0 },
		55
	};
	c2AABB bb = {
		{ 0, 0 },
		{ 10, 30 },
	};

	c2Raycast raycast = {0};
	int hit = c2RaytoAABB(ray, bb, &raycast);

	gl_line_color(ctx, 1, 1, 1);
	DrawCircle(ray.p, 5.0f);
	gl_line(ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0);
	DrawAABB(bb.min, bb.max);

	if (hit) {
		gl_line_color(ctx, 1, 0, 0);

		c2v p = c2Impact(ray, raycast.t);
		DrawCircle(p, 5.0f);
		gl_line(ctx, p.x, p.y, 0, p.x + raycast.n.x * 10.0f, p.y + raycast.n.y * 10.0f, 0);
	}
}

float PointSegmentTOI(c2v p, c2v v, c2v a, c2v b){ //almost certainly can be reduced down to a simpler equation
	c2v AB = c2Sub(b, a);
	c2v n = c2CCW90(AB);
	c2v vP = c2Sub(a,p);

	float denom = (c2Dot(v,n));
	if(denom == 0) return INFINITY; //parallel, never collide
	float t = (c2Dot(vP,n))/denom;
	if(t<0) return INFINITY; //collided in the past

	c2v intersection = c2Add(p, c2Mulvs(v,t));
	c2v iA = c2Sub(intersection, a);
	c2v iB = c2Sub(intersection, b);

	if(c2Dot(iA,AB) < 0) return INFINITY; //off the sides, never collide
	if(c2Dot(iB,AB) > 0) return INFINITY; //off the sides, never collide
	return t;
}

float PolyToPolyTOI(const c2Poly* pA, const c2x* ax_ptr, c2v vA, const c2Poly* pB, const c2x* bx_ptr, c2v vB, c2v* out_normal, c2v* out_contact_point){
	//return c2TOI(pA, C2_TYPE_POLY, ax_ptr, vA, pB, C2_TYPE_POLY, bx_ptr, vB, true, out_normal, out_contact_point, NULL);
	
	//degenerate cases, zero movement or already colliding
	if((vA.x==vB.x && vA.y==vB.y) || c2PolytoPoly(pA, ax_ptr, pB, bx_ptr)){
		if(out_normal) *out_normal = {0,0};
		if(out_contact_point) *out_contact_point = {0,0};
		return 0;
	}

	float t = INFINITY;
	c2v n = {0,0};
	c2v p = {0,0};

	//pre-transform poly vertices to avoid doing it every access
	c2Poly A = *pA;
	c2Poly B = *pB;
	if(ax_ptr){
		for(int i = 0; i<A.count; i++){
			A.verts[i] = c2Mulxv(*ax_ptr, A.verts[i]);
			A.norms[i] = c2Mulrv(ax_ptr->r, A.norms[i]);
		}
	}
	if(bx_ptr){
		for(int i = 0; i<B.count; i++){
			B.verts[i] = c2Mulxv(*bx_ptr, B.verts[i]);
			B.norms[i] = c2Mulrv(bx_ptr->r, B.norms[i]);
		}
	}

	//TOI of A's vertices against B's edges
	c2v v = c2Sub(vA, vB);
	for(int j = 0; j<B.count; j++){
		if(c2Dot(B.norms[j], v) < 0){ //don't need to check edges facing away from the direction of movement
			for(int i = 0; i<A.count; i++){
				float v_t = PointSegmentTOI(A.verts[i], v, B.verts[j], B.verts[(j+1)%B.count]);
				if(v_t <= t){
					t = v_t;
					n = c2Neg(B.norms[j]);
					p = c2Add(A.verts[i], c2Mulvs(vA, v_t));
				}
			}
		}
	}

	//TOI of B's vertices against A's edges
	v = c2Sub(vB, vA);
	for(int j = 0; j<A.count; j++){
		if(c2Dot(A.norms[j], v) < 0){ //don't need to check edges facing away from the direction of movement
			for(int i = 0; i<B.count; i++){
				float v_t = PointSegmentTOI(B.verts[i], v, A.verts[j], A.verts[(j+1)%A.count]);
				if(v_t <= t){
					t = v_t;
					n = A.norms[j];
					p = c2Add(B.verts[i], c2Mulvs(vB, v_t));
				}
			}
		}
	}

	if(out_normal) *out_normal = c2SafeNorm(n);
	if(out_contact_point) *out_contact_point = p;
	return t;
}

#pragma optimize("", off)
void infinite_loop_tyler_glaiel_analytic_toi_and_gjk()
{
	int N = 10000;
	c2Poly* quads = (c2Poly*)malloc(sizeof(c2Poly) * N);

	for (int i = 0; i < N; ++i) {
		quads->count = 4;
		quads->verts[0] = c2Add(c2V(randf() * 0.1f, randf() * 0.1f), c2V(-1, -1));
		quads->verts[1] = c2Add(c2V(randf() * 0.1f, randf() * 0.1f), c2V(-1,  1));
		quads->verts[2] = c2Add(c2V(randf() * 0.1f, randf() * 0.1f), c2V( 1,  1));
		quads->verts[3] = c2Add(c2V(randf() * 0.1f, randf() * 0.1f), c2V( 1, -1));
		c2Norms(quads->verts, quads->norms, 4);
	}

	c2Poly moving_quad;
	moving_quad.count = 4;
	moving_quad.verts[0] = c2V(-1,  0);
	moving_quad.verts[1] = c2V( 0,  1);
	moving_quad.verts[2] = c2V( 1,  0);
	moving_quad.verts[3] = c2V( 0,  -1);
	c2Norms(moving_quad.verts, moving_quad.norms, 4);

	double inv_freq = 1.0 / (double)glfwGetTimerFrequency();

	while (1) {
		uint64_t t0 = glfwGetTimerValue();
		for (int i = 0; i < N; ++i) {
			c2TOI(quads + i, C2_TYPE_POLY, NULL, c2V(0, 0), &moving_quad, C2_TYPE_POLY, NULL, c2V(0, -100), 0, NULL);
		}
		uint64_t t1 = glfwGetTimerValue();
		float dt0 = (float)((double)(t1 - t0) * inv_freq);

		t0 = glfwGetTimerValue();
		for (int i = 0; i < N; ++i) {
			PolyToPolyTOI(quads + i, NULL, c2V(0, 0), &moving_quad, NULL, c2V(0, -100), NULL, NULL);
		}
		t1 = glfwGetTimerValue();
		float dt1 = (float)((double)(t1 - t0) * inv_freq);

		printf("Conservative Advancement %f\nAnalytic %f\n", dt0, dt1);
	}
}
#pragma optimize("", on)

void PDeveloper_c2PolytoPoly_bug()
{
	c2v p0[] = {
		{ -568.0f, 928.0f },
		{ -568.0f, 1056.0f },
		{ -848.0f, 1056.0f },
		{ -848.0f, 928.0f }
	};

	c2v p1[] = {
		{ -688.0f, 736.0f },
		{ -688.0f, 1184.0f },
		{ -912.0f, 1184.0f },
		{ -912.0f, 736.0f }
	};

	c2Poly poly0;
	c2Poly poly1;

	poly0.count = poly1.count = 4;
	for (int i = 0; i < 4; ++i) {
		poly0.verts[i] = p0[i];
		poly1.verts[i] = p1[i];
	}

	c2MakePoly(&poly0);
	c2MakePoly(&poly1);

	DrawPoly(poly0.verts, 4);
	DrawPoly(poly1.verts, 4);

	if (c2PolytoPoly(&poly0, nullptr, &poly1, nullptr)) printf("POLYGONS ARE COLLIDING\n");
	else printf("POLYGONS ARE ***NOT*** COLLIDING\n");
}

void tyler_glaiel_c2CapsuletoPolyManifold_normal_bug_on_deep_case()
{
	c2Capsule cap;
	cap.a = c2V(0, 0);
	cap.b = c2V(0, 100);
	cap.r = 20.0f;

	c2Poly poly;
	poly.verts[0] = c2Mulvs(c2V( 0,  1), 75.0f);
	poly.verts[1] = c2Mulvs(c2V(-1,  1), 75.0f);
	poly.verts[2] = c2Mulvs(c2V( 1, -1), 75.0f);
	poly.count = 3;
	c2MakePoly(&poly);

	for (int i = 0; i < 3; ++i) {
		poly.verts[i] = c2Add(poly.verts[i], c2V(mouse_x, mouse_y));
	}

	gl_line_color(ctx, 1, 1, 1);
	DrawCapsule(cap.a, cap.b, cap.r);
	DrawPoly(poly.verts, 3);

	c2Manifold m;
	c2CapsuletoPolyManifold(cap, &poly, NULL, &m);

	if (m.count) {
		DrawManifold(m);

		gl_line_color(ctx, 0.5f, 0.5f, 0.5f);
		float max_depth = 0;
		for (int i = 0; i < m.count; ++i) {
			if (max_depth < m.depths[i]) max_depth = m.depths[i];
		}
		for (int i = 0; i < 3; ++i) {
			poly.verts[i] = c2Add(poly.verts[i], c2Mulvs(m.n, max_depth));
		}
		DrawPoly(poly.verts, 3);
	}
}

void try_out_and_render_dual()
{
	c2Poly poly;
	poly.count = 5;
	poly.verts[0] = c2V(-30, 0);
	poly.verts[1] = c2V(-20, 10);
	poly.verts[2] = c2V(40, 30);
	poly.verts[3] = c2V(20, -20);
	poly.verts[4] = c2V(0, -40);
	c2Norms(poly.verts, poly.norms, poly.count);

	c2Poly inflated = poly;
	c2Inflate(&inflated, C2_TYPE_POLY, 5.0f);

	gl_line_color(ctx, 1, 1, 1);
	DrawPoly(poly.verts, poly.count);
	gl_line_color(ctx, 1, 0, 0);
	DrawPoly(inflated.verts, poly.count);

	c2Poly dual = c2Dual(poly, 5.0f);
	for (int i = 0; i < dual.count; ++i) {
		// Abitrarily resize the dual so we can see it nicely.
		// Try to make it the same size roughly as the input poly.
		dual.verts[i] = c2Mulvs(dual.verts[i], 1150);
	}
	gl_line_color(ctx, 0, 1, 1);
	DrawPoly(dual.verts, poly.count);
}

int main()
{
	// glfw and glad setup
	glfwSetErrorCallback(ErrorCB);

	if (!glfwInit())
		return 1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	int width = 640;
	int height = 480;
	window = glfwCreateWindow(width, height, "cute_c2 and cute_gl", NULL, NULL);

	if (!window)
	{
		glfwTerminate();
		return 1;
	}

	glfwSetScrollCallback(window, ScrollCB);
	glfwSetCursorPosCallback(window, MouseCB);
	glfwSetMouseButtonCallback(window, MouseButtonCB);
	glfwSetKeyCallback(window, KeyCB);
	glfwSetFramebufferSizeCallback(window, Reshape);

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	glfwSwapInterval(1);

	glfwGetFramebufferSize(window, &width, &height);
	Reshape(window, width, height);

	// tinygl setup
	// the clear bits are used in glClear, the settings bits are used in glEnable
	// use the | operator to mask together settings/bits, example settings_bits: GL_DEPTH_TEST | GL_STENCIL_TEST
	// either can be 0
	int max_draw_calls_per_flush = 32;
	int clear_bits = GL_COLOR_BUFFER_BIT;
	int settings_bits = 0;
	ctx = gl_make_ctx(max_draw_calls_per_flush, clear_bits, settings_bits);

	// define the attributes of vertices, which are inputs to the vertex shader
	// only accepts GL_TRIANGLES in 4th parameter
	// 5th parameter can be GL_STATIC_DRAW or GL_DYNAMIC_DRAW, which controls triple buffer or single buffering
	gl_vertex_data_t vd;
	gl_make_vertex_data(&vd, 1024 * 1024, GL_TRIANGLES, sizeof(Vertex), GL_DYNAMIC_DRAW);
	gl_add_attribute(&vd, "in_pos", 2, CUTE_GL_FLOAT, CUTE_GL_OFFSET_OF(Vertex, pos));
	gl_add_attribute(&vd, "in_col", 3, CUTE_GL_FLOAT, CUTE_GL_OFFSET_OF(Vertex, col));

	// a renderable holds a shader (gl_shader_t) + vertex definition (tgVertexData)
	// renderables are used to construct draw calls (see main loop below)
	gl_renderable_t r;
	gl_make_renderable(&r, &vd);
	char* vs = (char*)ReadFileToMemory("simple.vs", 0);
	char* ps = (char*)ReadFileToMemory("simple.ps", 0);
	CUTE_GL_ASSERT(vs);
	CUTE_GL_ASSERT(ps);
	gl_load_shader(&simple, vs, ps);
	free(vs);
	free(ps);
	gl_set_shader(&r, &simple);
	gl_send_matrix(&simple, "u_mvp", projection);
	gl_line_mvp(ctx, projection);

	// setup some models
	user_capsule.a = c2V(-30.0f, 0);
	user_capsule.b = c2V(30.0f, 0);
	user_capsule.r = 10.0f;

	//infinite_loop_tyler_glaiel_analytic_toi_and_gjk();

	// main loop
	glClearColor(0, 0, 0, 1);
	float t = 0;
	while (!glfwWindowShouldClose(window))
	{
		if (spaced_pressed == 1) spaced_pressed = 0;
		if (arrow_pressed == 1) arrow_pressed = 0;
		wheel = 0;
		glfwPollEvents();

		float dt = ct_time();
		t += dt;
		t = fmod(t, 2.0f * 3.14159265f);
		//tgSendF32(&post_fx, "u_time", 1, &t, 1);

		if (wheel) Rotate((c2v*)&user_capsule, (c2v*)&user_capsule, 2);

		static int code = 24;
		if (arrow_pressed) code = (code + 1) % 25;
		switch (code)
		{
		case 0: TestDrawPrim(); break;
		case 1: TestBoolean0(); break;
		case 2: TestBoolean1(); break;
		case 3: TestBoolean2(); break;
		case 4: TestRay0(); break;
		case 5: TestRay1(); break;
		case 6: TestRay2(); break;
		case 7: TestManifold0(); break;
		case 8: TestManifold1(); break;
		case 9: TestManifold2(); break;
		case 10: PlastburkRayBug(); break;
		case 11: sro5hRayBug(); break;
		case 12: circle_to_aabb_bug(); break;
		case 13: DJLink_aabb_bug(); break;
		case 14: lundmark_GJK_div_by_0_bug(); break;
		case 15: gjk_make_sure_cache_helps_and_works(); break;
		case 16: try_out_toi_via_conservative_advancment(); break;
		case 17: prime31_bad_toi_normal(); break;
		case 18: prime31_bad_toi_normal_animated(); break;
		case 19: prime31_bad_toi_normal_animated_aabb(); break;
		case 20: prime31_cap_to_aabb_bug(); break;
		case 21: prime31_cap_to_aabb_bug2(); break;
		case 22: martincohen_ray_bug(); break;
		case 23: tyler_glaiel_c2CapsuletoPolyManifold_normal_bug_on_deep_case(); break;
		case 24: try_out_and_render_dual();
		}

		// push a draw call to tinygl
		// all members of a tgDrawCall *must* be initialized
		//if (verts.size())
		//{
		//	tgDrawCall call;
		//	call.r = &r;
		//	call.texture_count = 0;
		//	call.verts = &verts[0];
		//	call.vert_count = verts.size();
		//	tgPushDrawCall(ctx, call);
		//}

		// flush all draw calls to screen
		// optionally the fb can be NULL or 0 to signify no post-processing fx
		gl_flush(ctx, SwapBuffers, use_post_fx ? &fb : 0, width, height);
		CUTE_GL_PRINT_GL_ERRORS();
		verts.clear();
	}

	gl_free_ctx(ctx);
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
