/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_sound_pitch_plugin.h

	To create implementation (the function definitions)
		#define CUTE_SOUND_PITCH_PLUGIN_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY

		Implements a cute sound plugin for performing real-time pitch shifting
		without time-stretching. It uses the fast fourier transform along with
		some other tricks, but ultimately is quite an expensive operation. Use
		with care.

		Please note this plugin is for *real-time* pitch shifting, meaning the
		quality of the pitch shifting will not sound nearly as good as an off-
		line preprocessor like Audacity's pitch shifter. If you want really high
		quality pitch shifting, it's best to preprocess your audio offline and
		simply save different samples at different pitches to disk, and then play
		them at run-time. This pitch shifter is just for small variations or
		other use-cases requiring a cheap-and-quick pitch adjustment.

	EXAMPLE

		https://github.com/RandyGaul/cute_headers/tree/master/examples_cute_sound/pitch
*/

#ifndef CUTE_SOUND_H
#	error Please include cute_sound.h before including cute_sound_pitch_plugin.h.
#endif

#ifndef CUTE_SOUND_PITCH_PLUGIN_H
#define CUTE_SOUND_PITCH_PLUGIN_H

cs_plugin_interface_t csp_get_pitch_plugin();

// Change pitch (not duration) of sound. pitch = 0.5f for one octave lower, pitch = 2.0f for one octave higher.
// pitch at 1.0f applies no change. pitch settings farther away from 1.0f create more distortion and lower
// the output sample quality. pitch can be adjusted in real-time for doppler effects and the like. Going beyond
// 0.5f and 2.0f may require some tweaking the pitch shifting parameters, and is not recommended.
void csp_set_pitch(cs_playing_sound_t* sound, float pitch, cs_plugin_id_t id);

typedef struct csp_filter_t csp_filter_t;

#endif CUTE_SOUND_PITCH_PLUGIN_H

#ifdef CUTE_SOUND_PITCH_PLUGIN_IMPLEMENTATION
#ifndef CUTE_SOUND_PITCH_PLUGIN_IMPLEMENTATION_ONCE
#define CUTE_SOUND_PITCH_PLUGIN_IMPLEMENTATION_ONCE

#ifndef CUTE_SOUND_IMPLEMENTATION
#	error Please make sure to place the implementation of cute sound pitch plugin just after the cute sound implementation.
// Here is an example.
// 
// #define CUTE_SOUND_IMPLEMENTATION
// #include <cute_sound.h>
// 
// #define CUTE_SOUND_PITCH_PLUGIN_IMPLEMENTATION
// #include <cute_sound_pitch_plugin.h>
#endif

// TODO:
// Make these run-time configurable. Specifically frame size, max lenght, and quality.
#define CUTE_SOUND_MAX_FRAME_LENGTH 4096
#define CUTE_SOUND_PITCH_FRAME_SIZE 512
#define CUTE_SOUND_PITCH_QUALITY 4
#define CUTE_SOUND_STEPSIZE (CUTE_SOUND_PITCH_FRAME_SIZE / CUTE_SOUND_PITCH_QUALITY)
#define CUTE_SOUND_OVERLAP (CUTE_SOUND_PITCH_FRAME_SIZE - CUTE_SOUND_STEPSIZE)
#define CUTE_SOUND_EXPECTED_FREQUENCY (2.0f * 3.14159265359f * (float)CUTE_SOUND_STEPSIZE / (float)CUTE_SOUND_PITCH_FRAME_SIZE)

// TODO:
// Use a memory pool for these things. For now they are just cs_malloc16'd/cs_free16'd
// Not high priority to use a pool, since pitch shifting is already really expensive,
// and cost of malloc is dwarfed. But would be a nice-to-have for potential memory
// fragmentation issues.
typedef struct csp_filter_t
{
	float pitch_shifted_output_samples[CUTE_SOUND_MAX_FRAME_LENGTH];
	float in_FIFO[CUTE_SOUND_STEPSIZE + CUTE_SOUND_PITCH_FRAME_SIZE];
	float out_FIFO[CUTE_SOUND_STEPSIZE + CUTE_SOUND_PITCH_FRAME_SIZE];
	float fft_data[2 * CUTE_SOUND_PITCH_FRAME_SIZE];
	float previous_phase[CUTE_SOUND_PITCH_FRAME_SIZE / 2 + 4];
	float sum_phase[CUTE_SOUND_PITCH_FRAME_SIZE / 2 + 4];
	float window_accumulator[CUTE_SOUND_STEPSIZE + CUTE_SOUND_PITCH_FRAME_SIZE];
	float freq[CUTE_SOUND_PITCH_FRAME_SIZE];
	float mag[CUTE_SOUND_PITCH_FRAME_SIZE];
	float pitch_shift_workspace[CUTE_SOUND_PITCH_FRAME_SIZE];
	int index;
	float pitch;
} csp_filter_t;

typedef struct csp_data_t
{
	int channel_count;
	csp_filter_t* filters[2];
} csp_data_t;

// TODO:
// Try this optimization out (2N POINT REAL FFT USING AN N POINT COMPLEX FFT)
// http://www.fftguru.com/fftguru.com.tutorial2.pdf

#include <math.h>

static uint32_t s_rev32(uint32_t x)
{
	uint32_t a = ((x & 0xAAAAAAAA) >> 1) | ((x & 0x55555555) << 1);
	uint32_t b = ((a & 0xCCCCCCCC) >> 2) | ((a & 0x33333333) << 2);
	uint32_t c = ((b & 0xF0F0F0F0) >> 4) | ((b & 0x0F0F0F0F) << 4);
	uint32_t d = ((c & 0xFF00FF00) >> 8) | ((c & 0x00FF00FF) << 8);
	return (d >> 16) | (d << 16);
}

static uint32_t s_popcount(uint32_t x)
{
	uint32_t a = x - ((x >> 1) & 0x55555555);
	uint32_t b = (((a >> 2) & 0x33333333) + (a & 0x33333333));
	uint32_t c = (((b >> 4) + b) & 0x0F0F0F0F);
	uint32_t d = c + (c >> 8);
	uint32_t e = d + (d >> 16);
	uint32_t f = e & 0x0000003F;
	return f;
}

static uint32_t s_log2(uint32_t x)
{
	uint32_t a = x | (x >> 1);
	uint32_t b = a | (a >> 2);
	uint32_t c = b | (b >> 4);
	uint32_t d = c | (c >> 8);
	uint32_t e = d | (d >> 16);
	uint32_t f = e >> 1;
	return s_popcount(f);
}

// x contains real inputs
// y contains imaginary inputs
// count must be a power of 2
// sign must be 1.0 (forward transform) or -1.0f (inverse transform)
static void s_fft(float* x, float* y, int count, float sign)
{
	int exponent = (int)s_log2((uint32_t)count);

	// bit reversal stage
	// swap all elements with their bit reversed index within the
	// lowest level of the Cooley-Tukey recursion tree
	for (int i = 1; i < count - 1; i++)
	{
		uint32_t j = s_rev32((uint32_t)i);
		j >>= (32 - exponent);
		if (i < (int)j)
		{
			float tx = x[i];
			float ty = y[i];
			x[i] = x[j];
			y[i] = y[j];
			x[j] = tx;
			y[j] = ty;
		}
	}

	// for each recursive iteration
	for (int iter = 0, L = 1; iter < exponent; ++iter)
	{
		int Ls = L;
		L <<= 1;
		float ur = 1.0f; // cos(pi / 2)
		float ui = 0;    // sin(pi / 2)
		float arg = 3.14159265359f / (float)Ls;
		float wr = cosf(arg);
		float wi = -sign * sinf(arg);

		// rows in DFT submatrix
		for (int j = 0; j < Ls; ++j)
		{
			// do butterflies upon DFT row elements
			for (int i = j; i < count; i += L)
			{
				int index = i + Ls;
				float x_index = x[index];
				float y_index = y[index];
				float x_i = x[i];
				float y_i = y[i];

				float tr = ur * x_index - ui * y_index;
				float ti = ur * y_index + ui * x_index;
				float x_low = x_i - tr;
				float x_high = x_i + tr;
				float y_low = y_i - ti;
				float y_high = y_i + ti;

				x[index] = x_low;
				y[index] = y_low;
				x[i] = x_high;
				y[i] = y_high;
			}

			// Rotate u1 and u2 via Givens rotations (2d planar rotation).
			// This keeps cos/sin calls in the outermost loop.
			// Floating point error is scaled proportionally to Ls.
			float t = ur * wr - ui * wi;
			ui = ur * wi + ui * wr;
			ur = t;
		}
	}

	// scale factor for forward transform
	if (sign > 0)
	{
		float inv_count = 1.0f / (float)count;
		for (int i = 0; i < count; i++)
		{
			x[i] *= inv_count;
			y[i] *= inv_count;
		}
	}
}

#ifdef _MSC_VER

#define CUTE_SOUND_ALIGN16_0 __declspec(align(16))
#define CUTE_SOUND_ALIGN16_1

#else

#define CUTE_SOUND_ALIGN16_0
#define CUTE_SOUND_ALIGN16_1 __attribute__((aligned(16)))

#endif

// SSE2 trig funcs from https://github.com/to-miz/sse_mathfun_extension/
#define _PS_CONST(Name, Val) \
	CUTE_SOUND_ALIGN16_0 float _ps_##Name[4] CUTE_SOUND_ALIGN16_1 = { Val, Val, Val, Val }

#define _PS_CONST_TYPE(Name, Type, Val) \
	CUTE_SOUND_ALIGN16_0 Type _ps_##Name[4] CUTE_SOUND_ALIGN16_1 = { Val, Val, Val, Val }

#define _PI32_CONST(Name, Val) \
	CUTE_SOUND_ALIGN16_0 int _pi32_##Name[4] CUTE_SOUND_ALIGN16_1 = { Val, Val, Val, Val }

_PS_CONST_TYPE(sign_mask, int, (int)0x80000000);
_PS_CONST_TYPE(inv_sign_mask, int, (int)~0x80000000);

_PS_CONST(atanrange_hi, 2.414213562373095f);
_PS_CONST(atanrange_lo, 0.4142135623730950f);
_PS_CONST(cephes_PIO2F, 1.5707963267948966192f);
_PS_CONST(cephes_PIO4F, 0.7853981633974483096f);
_PS_CONST(1, 1.0f);
_PS_CONST(0p5, 0.5f);
_PS_CONST(0, 0);
_PS_CONST(sincof_p0, -1.9515295891E-4f);
_PS_CONST(sincof_p1, 8.3321608736E-3f);
_PS_CONST(sincof_p2, -1.6666654611E-1f);
_PS_CONST(atancof_p0, 8.05374449538e-2f);
_PS_CONST(atancof_p1, 1.38776856032E-1f);
_PS_CONST(atancof_p2, 1.99777106478E-1f);
_PS_CONST(atancof_p3, 3.33329491539E-1f);
_PS_CONST(cephes_PIF, 3.141592653589793238f);
_PS_CONST(cephes_2PIF, 2.0f * 3.141592653589793238f);
_PS_CONST(cephes_FOPI, 1.27323954473516f); // 4 / M_PI
_PS_CONST(minus_cephes_DP1, -0.78515625f);
_PS_CONST(minus_cephes_DP2, -2.4187564849853515625e-4f);
_PS_CONST(minus_cephes_DP3, -3.77489497744594108e-8f);
_PS_CONST(coscof_p0, 2.443315711809948E-005f);
_PS_CONST(coscof_p1, -1.388731625493765E-003f);
_PS_CONST(coscof_p2, 4.166664568298827E-002f);
_PS_CONST(frame_size, (float)CUTE_SOUND_PITCH_FRAME_SIZE);

_PI32_CONST(1, 1);
_PI32_CONST(inv1, ~1);
_PI32_CONST(2, 2);
_PI32_CONST(4, 4);

#if 0 /* temporary comment it out, remove "unused functions" warning */
static __m128 _mm_atan_ps(__m128 x)
{
	__m128 sign_bit, y;

	sign_bit = x;
	/* take the absolute value */
	x = _mm_and_ps(x, *(__m128*)_ps_inv_sign_mask);
	/* extract the sign bit (upper one) */
	sign_bit = _mm_and_ps(sign_bit, *(__m128*)_ps_sign_mask);

	/* range reduction, init x and y depending on range */
		/* x > 2.414213562373095 */
	__m128 cmp0 = _mm_cmpgt_ps(x, *(__m128*)_ps_atanrange_hi);
	/* x > 0.4142135623730950 */
	__m128 cmp1 = _mm_cmpgt_ps(x, *(__m128*)_ps_atanrange_lo);

	/* x > 0.4142135623730950 && !(x > 2.414213562373095) */
	__m128 cmp2 = _mm_andnot_ps(cmp0, cmp1);

	/* -(1.0/x) */
	__m128 y0 = _mm_and_ps(cmp0, *(__m128*)_ps_cephes_PIO2F);
	__m128 x0 = _mm_div_ps(*(__m128*)_ps_1, x);
	x0 = _mm_xor_ps(x0, *(__m128*)_ps_sign_mask);

	__m128 y1 = _mm_and_ps(cmp2, *(__m128*)_ps_cephes_PIO4F);
	/* (x-1.0)/(x+1.0) */
	__m128 x1_o = _mm_sub_ps(x, *(__m128*)_ps_1);
	__m128 x1_u = _mm_add_ps(x, *(__m128*)_ps_1);
	__m128 x1 = _mm_div_ps(x1_o, x1_u);

	__m128 x2 = _mm_and_ps(cmp2, x1);
	x0 = _mm_and_ps(cmp0, x0);
	x2 = _mm_or_ps(x2, x0);
	cmp1 = _mm_or_ps(cmp0, cmp2);
	x2 = _mm_and_ps(cmp1, x2);
	x = _mm_andnot_ps(cmp1, x);
	x = _mm_or_ps(x2, x);

	y = _mm_or_ps(y0, y1);

	__m128 zz = _mm_mul_ps(x, x);
	__m128 acc = *(__m128*)_ps_atancof_p0;
	acc = _mm_mul_ps(acc, zz);
	acc = _mm_sub_ps(acc, *(__m128*)_ps_atancof_p1);
	acc = _mm_mul_ps(acc, zz);
	acc = _mm_add_ps(acc, *(__m128*)_ps_atancof_p2);
	acc = _mm_mul_ps(acc, zz);
	acc = _mm_sub_ps(acc, *(__m128*)_ps_atancof_p3);
	acc = _mm_mul_ps(acc, zz);
	acc = _mm_mul_ps(acc, x);
	acc = _mm_add_ps(acc, x);
	y = _mm_add_ps(y, acc);

	/* update the sign */
	y = _mm_xor_ps(y, sign_bit);

	return y;
}

static __m128 _mm_atan2_ps(__m128 y, __m128 x)
{
	__m128 x_eq_0 = _mm_cmpeq_ps(x, *(__m128*)_ps_0);
	__m128 x_gt_0 = _mm_cmpgt_ps(x, *(__m128*)_ps_0);
	__m128 x_le_0 = _mm_cmple_ps(x, *(__m128*)_ps_0);
	__m128 y_eq_0 = _mm_cmpeq_ps(y, *(__m128*)_ps_0);
	__m128 x_lt_0 = _mm_cmplt_ps(x, *(__m128*)_ps_0);
	__m128 y_lt_0 = _mm_cmplt_ps(y, *(__m128*)_ps_0);

	__m128 zero_mask = _mm_and_ps(x_eq_0, y_eq_0);
	__m128 zero_mask_other_case = _mm_and_ps(y_eq_0, x_gt_0);
	zero_mask = _mm_or_ps(zero_mask, zero_mask_other_case);

	__m128 pio2_mask = _mm_andnot_ps(y_eq_0, x_eq_0);
	__m128 pio2_mask_sign = _mm_and_ps(y_lt_0, *(__m128*)_ps_sign_mask);
	__m128 pio2_result = *(__m128*)_ps_cephes_PIO2F;
	pio2_result = _mm_xor_ps(pio2_result, pio2_mask_sign);
	pio2_result = _mm_and_ps(pio2_mask, pio2_result);

	__m128 pi_mask = _mm_and_ps(y_eq_0, x_le_0);
	__m128 pi = *(__m128*)_ps_cephes_PIF;
	__m128 pi_result = _mm_and_ps(pi_mask, pi);

	__m128 swap_sign_mask_offset = _mm_and_ps(x_lt_0, y_lt_0);
	swap_sign_mask_offset = _mm_and_ps(swap_sign_mask_offset, *(__m128*)_ps_sign_mask);

	__m128 offset0 = _mm_setzero_ps();
	__m128 offset1 = *(__m128*)_ps_cephes_PIF;
	offset1 = _mm_xor_ps(offset1, swap_sign_mask_offset);

	__m128 offset = _mm_andnot_ps(x_lt_0, offset0);
	offset = _mm_and_ps(x_lt_0, offset1);

	__m128 arg = _mm_div_ps(y, x);
	__m128 atan_result = _mm_atan_ps(arg);
	atan_result = _mm_add_ps(atan_result, offset);

	/* select between zero_result, pio2_result and atan_result */

	__m128 result = _mm_andnot_ps(zero_mask, pio2_result);
	atan_result = _mm_andnot_ps(pio2_mask, atan_result);
	atan_result = _mm_andnot_ps(pio2_mask, atan_result);
	result = _mm_or_ps(result, atan_result);
	result = _mm_or_ps(result, pi_result);

	return result;
}
#endif

static void _mm_sincos_ps(__m128 x, __m128 *s, __m128 *c)
{
	__m128 xmm1, xmm2, xmm3 = _mm_setzero_ps(), sign_bit_sin, y;
	__m128i emm0, emm2, emm4;
	sign_bit_sin = x;
	/* take the absolute value */
	x = _mm_and_ps(x, *(__m128*)_ps_inv_sign_mask);
	/* extract the sign bit (upper one) */
	sign_bit_sin = _mm_and_ps(sign_bit_sin, *(__m128*)_ps_sign_mask);

	/* scale by 4/Pi */
	y = _mm_mul_ps(x, *(__m128*)_ps_cephes_FOPI);

	/* store the integer part of y in emm2 */
	emm2 = _mm_cvttps_epi32(y);

	/* j=(j+1) & (~1) (see the cephes sources) */
	emm2 = _mm_add_epi32(emm2, *(__m128i*)_pi32_1);
	emm2 = _mm_and_si128(emm2, *(__m128i*)_pi32_inv1);
	y = _mm_cvtepi32_ps(emm2);

	emm4 = emm2;

	/* get the swap sign flag for the sine */
	emm0 = _mm_and_si128(emm2, *(__m128i*)_pi32_4);
	emm0 = _mm_slli_epi32(emm0, 29);
	__m128 swap_sign_bit_sin = _mm_castsi128_ps(emm0);

	/* get the polynom selection mask for the sine*/
	emm2 = _mm_and_si128(emm2, *(__m128i*)_pi32_2);
	emm2 = _mm_cmpeq_epi32(emm2, _mm_setzero_si128());
	__m128 poly_mask = _mm_castsi128_ps(emm2);

	/* The magic pass: "Extended precision modular arithmetic"
		x = ((x - y * DP1) - y * DP2) - y * DP3; */
	xmm1 = *(__m128*)_ps_minus_cephes_DP1;
	xmm2 = *(__m128*)_ps_minus_cephes_DP2;
	xmm3 = *(__m128*)_ps_minus_cephes_DP3;
	xmm1 = _mm_mul_ps(y, xmm1);
	xmm2 = _mm_mul_ps(y, xmm2);
	xmm3 = _mm_mul_ps(y, xmm3);
	x = _mm_add_ps(x, xmm1);
	x = _mm_add_ps(x, xmm2);
	x = _mm_add_ps(x, xmm3);

	emm4 = _mm_sub_epi32(emm4, *(__m128i*)_pi32_2);
	emm4 = _mm_andnot_si128(emm4, *(__m128i*)_pi32_4);
	emm4 = _mm_slli_epi32(emm4, 29);
	__m128 sign_bit_cos = _mm_castsi128_ps(emm4);

	sign_bit_sin = _mm_xor_ps(sign_bit_sin, swap_sign_bit_sin);


	/* Evaluate the first polynom  (0 <= x <= Pi/4) */
	__m128 z = _mm_mul_ps(x, x);
	y = *(__m128*)_ps_coscof_p0;

	y = _mm_mul_ps(y, z);
	y = _mm_add_ps(y, *(__m128*)_ps_coscof_p1);
	y = _mm_mul_ps(y, z);
	y = _mm_add_ps(y, *(__m128*)_ps_coscof_p2);
	y = _mm_mul_ps(y, z);
	y = _mm_mul_ps(y, z);
	__m128 tmp = _mm_mul_ps(z, *(__m128*)_ps_0p5);
	y = _mm_sub_ps(y, tmp);
	y = _mm_add_ps(y, *(__m128*)_ps_1);

	/* Evaluate the second polynom  (Pi/4 <= x <= 0) */

	__m128 y2 = *(__m128*)_ps_sincof_p0;
	y2 = _mm_mul_ps(y2, z);
	y2 = _mm_add_ps(y2, *(__m128*)_ps_sincof_p1);
	y2 = _mm_mul_ps(y2, z);
	y2 = _mm_add_ps(y2, *(__m128*)_ps_sincof_p2);
	y2 = _mm_mul_ps(y2, z);
	y2 = _mm_mul_ps(y2, x);
	y2 = _mm_add_ps(y2, x);

	/* select the correct result from the two polynoms */
	xmm3 = poly_mask;
	__m128 ysin2 = _mm_and_ps(xmm3, y2);
	__m128 ysin1 = _mm_andnot_ps(xmm3, y);
	y2 = _mm_sub_ps(y2, ysin2);
	y = _mm_sub_ps(y, ysin1);

	xmm1 = _mm_add_ps(ysin1, ysin2);
	xmm2 = _mm_add_ps(y, y2);

	/* update the sign */
	*s = _mm_xor_ps(xmm1, sign_bit_sin);
	*c = _mm_xor_ps(xmm2, sign_bit_cos);
}

static __m128i s_select_si(__m128i a, __m128i b, __m128i mask)
{
	return _mm_xor_si128(a, _mm_and_si128(mask, _mm_xor_si128(b, a)));
}

static __m128 s_vonhann4(int i)
{
	__m128 k4 = _mm_set_ps((float)(i * 4 + 3), (float)(i * 4 + 2), (float)(i * 4 + 1), (float)(i * 4));
	k4 = _mm_mul_ps(*(__m128*)_ps_cephes_2PIF, k4);
	k4 = _mm_div_ps(k4, *(__m128*)_ps_frame_size);

	// Seems like _mm_cos_ps and _mm_sincos_ps was causing some audio popping...
	// I'm not really skilled enough to fix it, but feel free to try: http://gruntthepeon.free.fr/ssemath/sse_mathfun.h
	// My guess is some large negative or positive values were causing some
	// precision trouble. In this case manually calling 4 cosines is not
	// really a big deal, since this function is not a bottleneck.

#if 0
	__m128 c = _mm_cos_ps(k4);
#elif 0
	__m128 s, c;
	_mm_sincos_ps(k4, &s, &c);
#else
	__m128 c = k4;
	float* cf = (float*)&c;
	cf[0] = cosf(cf[0]);
	cf[1] = cosf(cf[1]);
	cf[2] = cosf(cf[2]);
	cf[3] = cosf(cf[3]);
#endif

	__m128 von_hann = _mm_add_ps(_mm_mul_ps(_mm_set_ps1(-0.5f), c), _mm_set_ps1(0.5f));
	return von_hann;
}

static float s_atan2f(float x, float y)
{
	float signx = x > 0 ? 1.0f : -1.0f;
	if (x == 0) return 0;
	if (y == 0) return signx * 3.14159265f / 2.0f;
	return atan2f(x, y);
}

// Analysis and synthesis steps learned from Bernsee's wonderful blog post:
// http://blogs.zynaptiq.com/bernsee/pitch-shifting-using-the-ft/
static int s_pitch_shift(int num_samples_to_process, float sample_rate, const float* indata, csp_filter_t* pitch_filter)
{
	// This can happen if pitch shifting becomes slower than real-time.
	// If this does happen it will sound ugly. The only thing to do is
	// to either optimize the pitch shifter, tweak the tuning parameters,
	// or simply pitch shift less audio all at once.
	if (num_samples_to_process > CUTE_SOUND_MAX_FRAME_LENGTH) {
		return 0;
	}

	// make sure compiler didn't do anything weird with the member
	// offsets of csp_filter_t. All arrays must be 16 byte aligned
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->pitch_shifted_output_samples) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->fft_data) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->in_FIFO) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->out_FIFO) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->fft_data) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->previous_phase) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->sum_phase) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->window_accumulator) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->freq) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->mag) & 15));
	CUTE_SOUND_ASSERT(!((size_t)&(((csp_filter_t*)0)->pitch_shift_workspace) & 15));

	csp_filter_t* pf = pitch_filter;
	float pitch = pf->pitch;

	float freqPerBin = sample_rate / (float)CUTE_SOUND_PITCH_FRAME_SIZE;
	__m128 freq_per_bin = _mm_set_ps1(sample_rate / (float)CUTE_SOUND_PITCH_FRAME_SIZE);
	__m128 pi = *(__m128*)_ps_cephes_PIF;
	__m128 two_pi = *(__m128*)_ps_cephes_2PIF;
	__m128 pitch_quality = _mm_set_ps1((float)CUTE_SOUND_PITCH_QUALITY);
	float* out_samples = pf->pitch_shifted_output_samples;
	if (pf->index == 0) pf->index = CUTE_SOUND_OVERLAP;

	while (num_samples_to_process)
	{
		int copy_count = CUTE_SOUND_PITCH_FRAME_SIZE - pf->index;
		if (num_samples_to_process < copy_count) copy_count = num_samples_to_process;

		memcpy(pf->in_FIFO + pf->index, indata, sizeof(float) * copy_count);
		memcpy(out_samples, pf->out_FIFO + pf->index - CUTE_SOUND_OVERLAP, sizeof(float) * copy_count);

		int start_index = pf->index;
		int offset = start_index & 3;
		start_index += 4 - offset;

		for (int i = 0; i < offset; ++i)
			pf->in_FIFO[pf->index + i] /= 32768.0f;

		int extra = copy_count & 3;
		copy_count = copy_count / 4 - extra;
		__m128* in_FIFO = (__m128*)(pf->in_FIFO + pf->index + offset);
		CUTE_SOUND_ASSERT(!((size_t)in_FIFO & 15));
		__m128 int16_max = _mm_set_ps1(32768.0f);

		for (int i = 0; i < copy_count; ++i)
		{
			__m128 val = in_FIFO[i];
			__m128 div = _mm_div_ps(val, int16_max);
			in_FIFO[i] = div;
		}

		for (int i = 0, copy_count4 = copy_count * 4; i < extra; ++i)
		{
			int index = copy_count4 + i;
			pf->in_FIFO[pf->index + index] /= 32768.0f;
		}

		CUTE_SOUND_ASSERT(!((size_t)out_samples & 15));
		__m128* out_samples4 = (__m128*)out_samples;
		for (int i = 0; i < copy_count; ++i)
		{
			__m128 val = out_samples4[i];
			__m128 mul = _mm_mul_ps(val, int16_max);
			out_samples4[i] = mul;
		}

		for (int i = 0, copy_count4 = copy_count * 4; i < extra; ++i)
		{
			int index = copy_count4 + i;
			out_samples[index] *= 32768.0f;
		}

		copy_count = copy_count * 4 + extra;
		num_samples_to_process -= copy_count;
		pf->index += copy_count;
		indata += copy_count;
		out_samples += copy_count;

		if (pf->index >= CUTE_SOUND_PITCH_FRAME_SIZE)
		{
			pf->index = CUTE_SOUND_OVERLAP;
			{
				__m128* fft_data = (__m128*)pf->fft_data;
				__m128* in_FIFO = (__m128*)pf->in_FIFO;

				for (int k = 0; k < CUTE_SOUND_PITCH_FRAME_SIZE / 4; k++)
				{
					__m128 von_hann = s_vonhann4(k);
					__m128 sample = in_FIFO[k];
					__m128 windowed_sample = _mm_mul_ps(sample, von_hann);
					fft_data[k] = windowed_sample;
				}
			}

			memset(pf->fft_data + CUTE_SOUND_PITCH_FRAME_SIZE, 0, CUTE_SOUND_PITCH_FRAME_SIZE * sizeof(float));
			s_fft(pf->fft_data, pf->fft_data + CUTE_SOUND_PITCH_FRAME_SIZE, CUTE_SOUND_PITCH_FRAME_SIZE, 1.0f);

			{
				__m128* fft_data = (__m128*)pf->fft_data;
				__m128* previous_phase = (__m128*)pf->previous_phase;
				__m128* magnitudes = (__m128*)pf->mag;
				__m128* frequencies = (__m128*)pf->freq;
				int simd_count = (CUTE_SOUND_PITCH_FRAME_SIZE / 2) / 4;

				for (int k = 0; k <= simd_count; k++)
				{
					__m128 real = fft_data[k];
					__m128 imag = fft_data[(CUTE_SOUND_PITCH_FRAME_SIZE / 4) + k];
					__m128 overlap_phase = _mm_set_ps((float)(k * 4 + 3) * CUTE_SOUND_EXPECTED_FREQUENCY, (float)(k * 4 + 2) * CUTE_SOUND_EXPECTED_FREQUENCY, (float)(k * 4 + 1) * CUTE_SOUND_EXPECTED_FREQUENCY, (float)(k * 4) * CUTE_SOUND_EXPECTED_FREQUENCY);
					__m128 k4 = _mm_set_ps((float)(k * 4 + 3), (float)(k * 4 + 2), (float)(k * 4 + 1), (float)(k * 4));

					__m128 mag = _mm_mul_ps(_mm_set_ps1(2.0f), _mm_sqrt_ps(_mm_add_ps(_mm_mul_ps(real, real), _mm_mul_ps(imag, imag))));
#if 0
					__m128 phase = _mm_atan2_ps(imag, real);
#else
					__m128 phase; // = _mm_atan2_ps(imag, real);
					float *phasef = (float*)&phase;
					float *realf = (float*)&real;
					float *imagf = (float*)&imag;
					for (int i = 0; i < 4; i++) phasef[i] = s_atan2f(imagf[i], realf[i]);
#endif
					__m128 phase_dif = _mm_sub_ps(phase, previous_phase[k]);

					previous_phase[k] = phase;
					phase_dif = _mm_sub_ps(phase_dif, overlap_phase);

					// map delta phase into +/- pi interval
					__m128i qpd = _mm_cvttps_epi32(_mm_div_ps(phase_dif, pi));
					__m128i zero = _mm_setzero_si128();
					__m128i ltzero_mask = _mm_cmplt_epi32(qpd, zero);
					__m128i ones_bit = _mm_and_si128(qpd, _mm_set1_epi32(1));
					__m128i neg_qpd = _mm_sub_epi32(qpd, ones_bit);
					__m128i pos_qpd = _mm_add_epi32(qpd, ones_bit);
					qpd = s_select_si(pos_qpd, neg_qpd, ltzero_mask);
					__m128 pi_range_offset = _mm_mul_ps(pi, _mm_cvtepi32_ps(qpd));
					phase_dif = _mm_sub_ps(phase_dif, pi_range_offset);

					__m128 deviation = _mm_div_ps(_mm_mul_ps(_mm_set_ps1((float)CUTE_SOUND_PITCH_QUALITY), phase_dif), two_pi);
					__m128 true_freq_estimated = _mm_add_ps(_mm_mul_ps(k4, freq_per_bin), _mm_mul_ps(deviation, freq_per_bin));

					magnitudes[k] = mag;
					frequencies[k] = true_freq_estimated;
				}
			}

			// actual pitch shifting work
			// shift frequencies into workspace
			memset(pf->pitch_shift_workspace, 0, (CUTE_SOUND_PITCH_FRAME_SIZE / 2) * sizeof(float));
			for (int k = 0; k <= CUTE_SOUND_PITCH_FRAME_SIZE / 2; k++)
			{
				int index = (int)(k * pitch);
				if (index <= CUTE_SOUND_PITCH_FRAME_SIZE / 2)
					pf->pitch_shift_workspace[index] = pf->freq[k] * pitch;
			}

			// swap buffers around to reuse old pf->preq buffer as the new workspace
			float* frequencies = pf->pitch_shift_workspace;
			float* pitch_shift_workspace = pf->freq;
			float* magnitudes = pf->mag;

			// shift magnitudes into workspace
			memset(pitch_shift_workspace, 0, CUTE_SOUND_PITCH_FRAME_SIZE * sizeof(float));
			for (int k = 0; k <= CUTE_SOUND_PITCH_FRAME_SIZE / 2; k++)
			{
				int index = (int)(k * pitch);
				if (index <= CUTE_SOUND_PITCH_FRAME_SIZE / 2)
					pitch_shift_workspace[index] += magnitudes[k];
			}

			// track where the shifted magnitudes are
			magnitudes = pitch_shift_workspace;

			{
				__m128* magnitudes4 = (__m128*)magnitudes;
				__m128* frequencies4 = (__m128*)frequencies;
				__m128* fft_data = (__m128*)pf->fft_data;
				__m128* sum_phase = (__m128*)pf->sum_phase;
				int simd_count = (CUTE_SOUND_PITCH_FRAME_SIZE / 2) / 4;

				for (int k = 0; k <= simd_count; k++)
				{
					__m128 mag = magnitudes4[k];
					__m128 freq = frequencies4[k];
					__m128 freq_per_bin_k = _mm_set_ps((float)(k * 4 + 3) * freqPerBin, (float)(k * 4 + 2) * freqPerBin, (float)(k * 4 + 1) * freqPerBin, (float)(k * 4) * freqPerBin);

					freq = _mm_sub_ps(freq, freq_per_bin_k);
					freq = _mm_div_ps(freq, freq_per_bin);

					freq = _mm_mul_ps(two_pi, freq);
					freq = _mm_div_ps(freq, pitch_quality);

					__m128 overlap_phase = _mm_set_ps((float)(k * 4 + 3) * CUTE_SOUND_EXPECTED_FREQUENCY, (float)(k * 4 + 2) * CUTE_SOUND_EXPECTED_FREQUENCY, (float)(k * 4 + 1) * CUTE_SOUND_EXPECTED_FREQUENCY, (float)(k * 4) * CUTE_SOUND_EXPECTED_FREQUENCY);
					freq = _mm_add_ps(freq, overlap_phase);

					__m128 phase = sum_phase[k];
					phase = _mm_add_ps(phase, freq);
					sum_phase[k] = phase;

					__m128 c, s;
					_mm_sincos_ps(phase, &s, &c);
					__m128 real = _mm_mul_ps(mag, c);
					__m128 imag = _mm_mul_ps(mag, s);

					fft_data[k] = real;
					fft_data[(CUTE_SOUND_PITCH_FRAME_SIZE / 4) + k] = imag;
				}
			}

			for (int k = CUTE_SOUND_PITCH_FRAME_SIZE + 2; k < 2 * CUTE_SOUND_PITCH_FRAME_SIZE - 2; ++k)
				pf->fft_data[k] = 0;

			s_fft(pf->fft_data, pf->fft_data + CUTE_SOUND_PITCH_FRAME_SIZE, CUTE_SOUND_PITCH_FRAME_SIZE, -1);

			{
				__m128* fft_data = (__m128*)pf->fft_data;
				__m128* window_accumulator = (__m128*)pf->window_accumulator;

				for (int k = 0; k < CUTE_SOUND_PITCH_FRAME_SIZE / 4; ++k)
				{
					__m128 von_hann = s_vonhann4(k);
					__m128 fft_data_segment = fft_data[k];
					__m128 accumulator_segment = window_accumulator[k];
					__m128 divisor = _mm_div_ps(pitch_quality, _mm_set_ps1(8.0f));
					fft_data_segment = _mm_mul_ps(von_hann, fft_data_segment);
					fft_data_segment = _mm_div_ps(fft_data_segment, divisor);
					accumulator_segment = _mm_add_ps(accumulator_segment, fft_data_segment);
					window_accumulator[k] = accumulator_segment;
				}
			}

			memcpy(pf->out_FIFO, pf->window_accumulator, CUTE_SOUND_STEPSIZE * sizeof(float));
			memmove(pf->window_accumulator, pf->window_accumulator + CUTE_SOUND_STEPSIZE, CUTE_SOUND_PITCH_FRAME_SIZE * sizeof(float));
			memmove(pf->in_FIFO, pf->in_FIFO + CUTE_SOUND_STEPSIZE, CUTE_SOUND_OVERLAP * sizeof(float));
		}
	}

	return 1;
}

static void s_on_make_playing_sound(cs_context_t* cs_ctx, void* plugin_instance, void** playing_sound_udata, const cs_playing_sound_t* sound)
{
	(void)cs_ctx;
	(void)plugin_instance;
	(void)playing_sound_udata;
	(void)sound;
	// Don't construct `csp_data_t` here, instead only do it on demand when `csp_set_pitch` is called.
}

static void s_on_free_playing_sound(cs_context_t* cs_ctx, void* plugin_instance, void* playing_sound_udata, const cs_playing_sound_t* sound)
{
	(void)cs_ctx;
	(void)plugin_instance;
	(void)sound;
	if (playing_sound_udata)
	{
		csp_data_t* pitch_data = (csp_data_t*)playing_sound_udata;
		for (int i = 0; i < pitch_data->channel_count; ++i)
		{
			csp_filter_t* filter = pitch_data->filters[i];
			cs_free16(filter, NULL);
		}
		CUTE_SOUND_FREE(pitch_data, NULL);
	}
}

static void s_on_mix_fn(cs_context_t* cs_ctx, void* plugin_instance, int channel_index, const float* samples_in, int sample_count, float** samples_out, void* playing_sound_udata, const cs_playing_sound_t* sound)
{
	CUTE_SOUND_ASSERT(channel_index >= 0);
	CUTE_SOUND_ASSERT(channel_index <= 1);
	(void)plugin_instance;
	(void)sound;
	if (!playing_sound_udata) return;
	csp_data_t* pitch_data = (csp_data_t*)playing_sound_udata;
	csp_filter_t* filter = pitch_data->filters[channel_index];
	int success = s_pitch_shift(sample_count, (float)cs_ctx->Hz, samples_in, filter);
	if (success) *samples_out = filter->pitch_shifted_output_samples;
	else *samples_out = (float*)samples_in;
}

void csp_set_pitch(cs_playing_sound_t* sound, float pitch, cs_plugin_id_t id)
{
	CUTE_SOUND_ASSERT(id >= 0 && id < CUTE_SOUND_PLUGINS_MAX);
	if (pitch == 1.0f) return;
	csp_data_t* pitch_data = (csp_data_t*)sound->plugin_udata[id];

	if (!pitch_data)
	{
		pitch_data = (csp_data_t*)CUTE_SOUND_ALLOC(sizeof(csp_data_t), NULL);
		memset(pitch_data, 0, sizeof(csp_data_t));
		pitch_data->channel_count = sound->loaded_sound->channel_count;
		sound->plugin_udata[id] = pitch_data;

		for (int i = 0; i < pitch_data->channel_count; ++i)
		{
			csp_filter_t* filter = (csp_filter_t*)cs_malloc16(sizeof(csp_filter_t), NULL);
			memset(filter, 0, sizeof(csp_filter_t));
			filter->pitch = pitch;
			pitch_data->filters[i] = filter;
		}
	}

	for (int i = 0; i < pitch_data->channel_count; ++i)
	{
		csp_filter_t* filter = pitch_data->filters[i];
		filter->pitch = pitch;
	}
}

cs_plugin_interface_t csp_get_pitch_plugin()
{
	cs_plugin_interface_t plugin;
	plugin.plugin_instance = NULL;
	plugin.on_make_playing_sound_fn = s_on_make_playing_sound;
	plugin.on_free_playing_sound_fn = s_on_free_playing_sound;
	plugin.on_mix_fn = s_on_mix_fn;
	return plugin;
}

#endif // CUTE_SOUND_PITCH_PLUGIN_IMPLEMENTATION_ONCE
#endif // CUTE_SOUND_PITCH_PLUGIN_IMPLEMENTATION
