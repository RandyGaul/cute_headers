/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	tinyspheremesh - v1.0 - 10/06/17

	To create implementation (the function definitions)
		#define TINYSPHEREMESH_IMPL
	in *one* C/CPP file (translation unit) that includes this file

	This header contains functions to generate a spherical mesh. The output
	is an array of floats representing triplets of vectors. Each triplet is
	a triangle. Gathering up the triangles is a matter of writing a loop somewhat
	like so (for 3 component vectors):

	for (int i = 0; i < vert_count * 3; i += 3)
	{
		vector v = { floats[i], floats[i + 1], floats[i + 2] };
		out_vectors[i / 3] = v;
	}

	for (int i = 0; i < vert_count; i += 3)
	{
		vector a = out_vectors[i];
		vector b = out_vectors[i + 1];
		vector c = out_vectors[i + 2];

		DrawTriangle(a, b, c);
	}

	Since some vector libraries use 3 component vectors, and some use 4 component
	vectors this header has support for both kinds.

	Generating a sphere mesh requires some scratch memory provided by the user. Once
	a mesh is generated the floats of the triangles are returned. free() them when
	done.

	number_of_subdivisions represents how many recursive steps to take. The minimum
	output of vertices is 24, and each subdivision will multiply the previous vert
	count by 4. Here is a visualization: https://stackoverflow.com/a/7687312/6410671

	The output sphere is a modified geodesic sphere
	(see: https://en.wikipedia.org/wiki/Geodesic_polyhedron). The modification is
	to perform normalization after each subdivision, which results in triangles with
	uniform area. Normalization after more than one subdivision will result in triangles
	with varying areas (not implemented in this header, as uniform area is way cooler).

	EXAMPLE DEMO PROGRAM:

		// make sphere verts
		int num_subdivisions = 5;
		int sphere_bytes_scratch = tsmBytesRequired3(num_subdivisions);
		void* sphere_scratch = malloc(sphere_bytes_scratch);
		int sphere_verts_count;
		float* sphere_floats = tsmGenerateSphereMeshVerts3(sphere_scratch, num_subdivisions, &sphere_verts_count);
		free(sphere_scratch);

		// example of drawing sphere with tinygl.h (https://github.com/RandyGaul/tinyheaders/blob/master/tinygl.h)
		for (int i = 0; i < sphere_verts_count * 3; i += 9)
		{
			float ax = sphere_floats[i];
			float ay = sphere_floats[i + 1];
			float az = sphere_floats[i + 2];
			float bx = sphere_floats[i + 3];
			float by = sphere_floats[i + 4];
			float bz = sphere_floats[i + 5];
			float cx = sphere_floats[i + 6];
			float cy = sphere_floats[i + 7];
			float cz = sphere_floats[i + 8];

			tgLine(ctx_tg, ax, ay, az, bx, by, bz);
			tgLine(ctx_tg, bx, by, bz, cx, cy, cz);
			tgLine(ctx_tg, cx, cy, cz, ax, ay, az);
		}

		free(sphere_floats);
*/

#if !defined( TINYSPHEREMESH_H )

int tsmBytesRequired3(int number_of_subdivisions);
float* tsmGenerateSphereMeshVerts3(void* scratch_memory, int number_of_subdivisions, int* vert_count);

int tsmBytesRequired4(int number_of_subdivisions);
float* tsmGenerateSphereMeshVerts4(void* scratch_memory, int number_of_subdivisions, int* vert_count);

#define TINYSPHEREMESH_H
#endif

#if defined( TINYSPHEREMESH_IMPL )

#include <math.h>

typedef struct
{
	float x;
	float y;
	float z;
} tsmv3;

tsmv3 tsm_add_v3(tsmv3 a, tsmv3 b)
{
	tsmv3 c;
	c.x = a.x + b.x;
	c.y = a.y + b.y;
	c.z = a.z + b.z;
	return c;
}

tsmv3 tsm_mul_v3(tsmv3 a, float b)
{
	tsmv3 c;
	c.x = a.x * b;
	c.y = a.y * b;
	c.z = a.z * b;
	return c;
}

tsmv3 tsm_norm_v3(tsmv3 a)
{
	float length = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
	float inv = 1.0f / length;
	return tsm_mul_v3(a, inv);
}

typedef struct
{
	float x;
	float y;
	float z;
	float w;
} tsmv4;

tsmv4 tsm_add_v4(tsmv4 a, tsmv4 b)
{
	tsmv4 c;
	c.x = a.x + b.x;
	c.y = a.y + b.y;
	c.z = a.z + b.z;
	c.w = a.w + b.w;
	return c;
}

tsmv4 tsm_mul_v4(tsmv4 a, float b)
{
	tsmv4 c;
	c.x = a.x * b;
	c.y = a.y * b;
	c.z = a.z * b;
	c.w = a.w * b;
	return c;
}

tsmv4 tsm_norm_v4(tsmv4 a)
{
	float length = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
	float inv = 1.0f / length;
	return tsm_mul_v4(a, inv);
}

int tsmVertCount(int subdivisions)
{
	int vert_count = 24;
	for (int i = 0; i < subdivisions; ++i) vert_count *= 4;
	return vert_count;
}

int tsmCalcBytes(int subdivisions, int component_count)
{
	int vert_count = tsmVertCount(subdivisions);
	int vector_size = sizeof(float) * component_count;
	return vert_count * 2 * vector_size;
}

int tsmBytesRequired3(int number_of_subdivisions)
{
	return tsmCalcBytes(number_of_subdivisions, 3);
}

void tsmSubdivide3(int* in_pointer, int* out_pointer, tsmv3* in, tsmv3* out)
{
	int ip = *in_pointer;
	int op = 0;
 
	for (int i = 0; i < ip; i += 3)
	{
		tsmv3 a = in[i];
		tsmv3 b = in[i + 1];
		tsmv3 c = in[i + 2];
 
		tsmv3 ab = tsm_mul_v3(tsm_add_v3(a, b), 0.5f);
		tsmv3 bc = tsm_mul_v3(tsm_add_v3(b, c), 0.5f);
		tsmv3 ca = tsm_mul_v3(tsm_add_v3(c, a), 0.5f);

		ab = tsm_norm_v3(ab);
		bc = tsm_norm_v3(bc);
		ca = tsm_norm_v3(ca);
 
		out[op++] = b;
		out[op++] = bc;
		out[op++] = ab;
 
		out[op++] = c;
		out[op++] = ca;
		out[op++] = bc;
 
		out[op++] = a;
		out[op++] = ab;
		out[op++] = ca;
 
		out[op++] = ab;
		out[op++] = bc;
		out[op++] = ca;
	}
 
	*out_pointer = op;
}

float* tsmGenerateSphereMeshVerts3(void* scratch_memory, int number_of_subdivisions, int* vert_count)
{
	if (!scratch_memory) return 0;
	int final_vert_count = tsmVertCount(number_of_subdivisions);
	tsmv3* in = (tsmv3*)scratch_memory;
	tsmv3* out = (tsmv3*)(((float*)scratch_memory) + final_vert_count * 3);
	int ip = 0;
	int op = 0;

	tsmv3 octohedron[] = {
		{ 1.0f, 0.0f, 0.0f},
		{ 0.0f,-1.0f, 0.0f},
		{-1.0f, 0.0f, 0.0f},
		{ 0.0f, 1.0f, 0.0f},
		{ 0.0f, 0.0f, 1.0f},
		{ 0.0f, 0.0f,-1.0f},
	};
 
	in[ip++] = octohedron[2 - 1];
	in[ip++] = octohedron[1 - 1];
	in[ip++] = octohedron[5 - 1];
 
	in[ip++] = octohedron[3 - 1];
	in[ip++] = octohedron[2 - 1];
	in[ip++] = octohedron[5 - 1];
 
	in[ip++] = octohedron[4 - 1];
	in[ip++] = octohedron[3 - 1];
	in[ip++] = octohedron[5 - 1];
 
	in[ip++] = octohedron[1 - 1];
	in[ip++] = octohedron[4 - 1];
	in[ip++] = octohedron[5 - 1];
 
	in[ip++] = octohedron[1 - 1];
	in[ip++] = octohedron[2 - 1];
	in[ip++] = octohedron[6 - 1];
 
	in[ip++] = octohedron[2 - 1];
	in[ip++] = octohedron[3 - 1];
	in[ip++] = octohedron[6 - 1];
 
	in[ip++] = octohedron[3 - 1];
	in[ip++] = octohedron[4 - 1];
	in[ip++] = octohedron[6 - 1];
 
	in[ip++] = octohedron[4 - 1];
	in[ip++] = octohedron[1 - 1];
	in[ip++] = octohedron[6 - 1];

	for (int i = 0; i < number_of_subdivisions; ++i)
	{
		tsmSubdivide3(&ip, &op, in, out);
		tsmv3* tv = in;
		in = out;
		out = tv;
		int tp = ip;
		ip = op;
		op = tp;
	}

	out = in;
	op = ip;
	if (final_vert_count != op) return 0;

	float* result = (float*)malloc(final_vert_count * 3 * sizeof(float));
	for (int i = 0; i < final_vert_count; ++i)
	{
		tsmv3 v = out[i];
		result[i * 3] = v.x;
		result[i * 3 + 1] = v.y;
		result[i * 3 + 2] = v.z;
	}

	if (vert_count) *vert_count = final_vert_count;
	return result;
}

int tsmBytesRequired4(int number_of_subdivisions)
{
	return tsmCalcBytes(number_of_subdivisions, 4);
}

void tsmSubdivide4(int* in_pointer, int* out_pointer, tsmv4* in, tsmv4* out)
{
	int ip = *in_pointer;
	int op = 0;
 
	for (int i = 0; i < ip; i += 3)
	{
		tsmv4 a = in[i];
		tsmv4 b = in[i + 1];
		tsmv4 c = in[i + 2];
 
		tsmv4 ab = tsm_mul_v4(tsm_add_v4(a, b), 0.5f);
		tsmv4 bc = tsm_mul_v4(tsm_add_v4(b, c), 0.5f);
		tsmv4 ca = tsm_mul_v4(tsm_add_v4(c, a), 0.5f);

		ab = tsm_norm_v4(ab);
		bc = tsm_norm_v4(bc);
		ca = tsm_norm_v4(ca);
 
		out[op++] = b;
		out[op++] = bc;
		out[op++] = ab;
 
		out[op++] = c;
		out[op++] = ca;
		out[op++] = bc;
 
		out[op++] = a;
		out[op++] = ab;
		out[op++] = ca;
 
		out[op++] = ab;
		out[op++] = bc;
		out[op++] = ca;
	}
 
	*out_pointer = op;
}

float* tsmGenerateSphereMeshVerts4(void* scratch_memory, int number_of_subdivisions, int* vert_count)
{
	if (!scratch_memory) return 0;
	int final_vert_count = tsmVertCount(number_of_subdivisions);
	tsmv4* in = (tsmv4*)scratch_memory;
	tsmv4* out = (tsmv4*)(((float*)scratch_memory) + final_vert_count * 4);
	int ip = 0;
	int op = 0;

	tsmv4 octohedron[] = {
		{ 1.0f, 0.0f, 0.0f, 0.0f},
		{ 0.0f,-1.0f, 0.0f, 0.0f},
		{-1.0f, 0.0f, 0.0f, 0.0f},
		{ 0.0f, 1.0f, 0.0f, 0.0f},
		{ 0.0f, 0.0f, 1.0f, 0.0f},
		{ 0.0f, 0.0f,-1.0f, 0.0f},
	};
 
	in[ip++] = octohedron[2 - 1];
	in[ip++] = octohedron[1 - 1];
	in[ip++] = octohedron[5 - 1];
 
	in[ip++] = octohedron[3 - 1];
	in[ip++] = octohedron[2 - 1];
	in[ip++] = octohedron[5 - 1];
 
	in[ip++] = octohedron[4 - 1];
	in[ip++] = octohedron[3 - 1];
	in[ip++] = octohedron[5 - 1];
 
	in[ip++] = octohedron[1 - 1];
	in[ip++] = octohedron[4 - 1];
	in[ip++] = octohedron[5 - 1];
 
	in[ip++] = octohedron[1 - 1];
	in[ip++] = octohedron[2 - 1];
	in[ip++] = octohedron[6 - 1];
 
	in[ip++] = octohedron[2 - 1];
	in[ip++] = octohedron[3 - 1];
	in[ip++] = octohedron[6 - 1];
 
	in[ip++] = octohedron[3 - 1];
	in[ip++] = octohedron[4 - 1];
	in[ip++] = octohedron[6 - 1];
 
	in[ip++] = octohedron[4 - 1];
	in[ip++] = octohedron[1 - 1];
	in[ip++] = octohedron[6 - 1];

	for (int i = 0; i < number_of_subdivisions; ++i)
	{
		tsmSubdivide4(&ip, &op, in, out);
		tsmv4* tv = in;
		in = out;
		out = tv;
		int tp = ip;
		ip = op;
		op = tp;
	}

	out = in;
	op = ip;
	if (final_vert_count != op) return 0;

	float* result = (float*)malloc(final_vert_count * 4 * sizeof(float));
	for (int i = 0; i < final_vert_count; ++i)
	{
		tsmv4 v = out[i];
		result[i * 4] = v.x;
		result[i * 4 + 1] = v.y;
		result[i * 4 + 2] = v.z;
	}

	if (vert_count) *vert_count = final_vert_count;
	return result;
}

#endif


/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2017 Randy Gaul http://www.randygaul.net
	This software is provided 'as-is', without any express or implied warranty.
	In no event will the authors be held liable for any damages arising from
	the use of this software.
	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:
	  1. The origin of this software must not be misrepresented; you must not
	     claim that you wrote the original software. If you use this software
	     in a product, an acknowledgment in the product documentation would be
	     appreciated but is not required.
	  2. Altered source versions must be plainly marked as such, and must not
	     be misrepresented as being the original software.
	  3. This notice may not be removed or altered from any source distribution.
	------------------------------------------------------------------------------
	ALTERNATIVE B - Public Domain (www.unlicense.org)
	This is free and unencumbered software released into the public domain.
	Anyone is free to copy, modify, publish, use, compile, sell, or distribute this 
	software, either in source code form or as a compiled binary, for any purpose, 
	commercial or non-commercial, and by any means.
	In jurisdictions that recognize copyright laws, the author or authors of this 
	software dedicate any and all copyright interest in the software to the public 
	domain. We make this dedication for the benefit of the public at large and to 
	the detriment of our heirs and successors. We intend this dedication to be an 
	overt act of relinquishment in perpetuity of all present and future rights to 
	this software under copyright law.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
	AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
	ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
	------------------------------------------------------------------------------
*/
