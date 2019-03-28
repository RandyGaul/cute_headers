/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_serialize.h - v1.03

	To create implementation (the function definitions)
		#define CUTE_SERIALIZE_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY

		This header implements a binary data serializer for file and in-memory io.
		The main purpose is to use polymorphism to write a single serialization
		routine that can do read/write/measure operations, to either files or to
		in-memory buffers.

		The second purpose is to compute the minimum bits required for serializing
		any integer given a min/max range. The difference between max - min gives
		the number of unique values valid for the integer in question, and can be
		used to directly compute the minimum number of bits required. In this way
		integral values can be a-priori packed, instead of padding the serialized
		io with unnecessary bits for a full uint32/uint64 size.

	IO

		Stands for input/output. The underlying io can be in-memory, or on-disk.
		A `serialize_t` is initialized for either memory operations in RAM, or disk
		operations with file io.

	READ

		Read will grab bits from the underlying io.

	WRITE

		Write will place bits onto the underlying io.

	MEASURE

		Measure will count how many bits a series of READ/WRITE operations will
		require, and does not perform any actual io. It is strictly for high-performance
		measuring. This is typically useful to see if a particular io has enough
		empty space to store a serialized bit pattern -- which isn't often immediately
		obvious, as most of the time the underlying bit patterns take less space than
		the original values in RAM.

	QUICK EXAMPLE

		Say we have this struct:

			struct enemy_t
			{
				const char* name;
				enemy_enum type;
				int stat_count;
				int stats[5];
				float time_elapsed;
				vec3 dir;
			};

		Here's an example corresponding serialization routine:

			#define MY_SERIALIZE_CHECK(...) do { if ((__VA_ARGS__) != SERIALIZE_SUCCESS) return false; } while (0)

			bool serialize_enemy(serialize_t* io, enemy_t* enemy)
			{
				MY_SERIALIZE_CHECK(serialize_cstr(io, enemy->name));
				MY_SERIALIZE_CHECK(serialize_uint32(io, &enemy->type, 0, ENEMY_TYPE_ENUM_LAST));
				MY_SERIALIZE_CHECK(serialize_uint32(io, &enemy->stat_count, 0, 5));
				for (int i = 0; i < enemy->stat_count; ++i)
					MY_SERIALIZE_CHECK(serialize_uint32(io, enemy->stats + i, 0, INT_MAX));
				MY_SERIALIZE_CHECK(serialize_float(io, &enemy->time_elapsed));
				MY_SERIALIZE_CHECK(my_serialize_unit_vec3(io, &enemy->dir));
				return true;
			}

		The `serialize_enemy` can be called with different initializations of `io`. If `io` had been initialized
		with the `serialize_buffer_create`, then the routine will perform in-memory io. If it had been setup with the
		`serialize_file_create`, then file io will be performed (via `SERIALIZE_FREAD` and `SERIALIZE_FWRITE`).
		Additionally, the `io` can be setup to do read/write/measure operations depending on the `type` flag used
		during construction. The routine `serialize_enemy` need not be written more than once, and performs
		polymorphism depending on the type of `io` provided.

		Be sure to call `serialize_flush(io);` whenever serialization is completed, and you want to "finish" up all of
		your io. For example, before calling `fclose`, or before sending a buffer across the net.

	CUSTOMIZATION

		The following macros can be defined to override the default implementations:

			SERIALIZE_UINT64
			SERIALIZE_UINT32
			SERIALIZE_INT64
			SERIALIZE_INT32
			SERIALIZE_ALLOC
			SERIALIZE_FREE
			SERIALIZE_ASSERT
			SERIALIZE_FREAD
			SERIALIZE_FWRITE
			SERIALIZE_HOST_TO_IO_UINT32
			SERIALIZE_HOST_TO_IO_UINT64
			SERIALIZE_IO_TO_HOST_UINT32
			SERIALIZE_IO_TO_HOST_UINT64

	BYTE ORDERING ENDIANNESS

		The macros `SERIALIZE_HOST_TO_IO_UINT32`, `SERIALIZE_HOST_TO_IO_UINT64`, `SERIALIZE_IO_TO_HOST_UINT32`,
		`SERIALIZE_IO_TO_HOST_UINT64` don't do anything by default. Define and override them to implement byte
		swapping/endianness swapping for your platform. They can be used to swap endianness of all serialized
		bytes. These functions are used internally inside this header's implementation. There is no need to use
		these directly when using cute_serialize.h.

		Here is a typical example:

			#define SERIALIZE_HOST_TO_IO_UINT32 htonl
			#define SERIALIZE_HOST_TO_IO_UINT64 htonll
			#define SERIALIZE_IO_TO_HOST_UINT32 ntohl
			#define SERIALIZE_IO_TO_HOST_UINT64 ntohll
			#define CUTE_SERIALIZE_IMPLEMENTATION
			#include <cute_serialize.h>

	DEPENDENCIES

		The implementation depends on <winnt.h> on windows (for `_BitScanReverse64`).


	Revision history:
		1.00 (10/22/2018) initial release
		1.01 (02/05/2019) added many convenience functions, and fixed bug in serialize cstr
		1.02 (03/20/2019) added SERIALIZE_FREAD and SERIALIZE_FWRITE, and disabled compiling unit tests by default
		1.03 (03/27/2019) bugfixe edge-cases in measure bytes, cstr, and flush
*/

#if !defined(CUTE_SERIALIZE_H)

#if !defined(SERIALIZE_UINT64)
	#define SERIALIZE_UINT64 unsigned long long
#endif

#if !defined(SERIALIZE_UINT32)
	#define SERIALIZE_UINT32 unsigned
#endif

#if !defined(SERIALIZE_INT64)
	#define SERIALIZE_INT64 long long
#endif

#if !defined(SERIALIZE_INT32)
	#define SERIALIZE_INT32 int
#endif

#define SERIALIZE_SUCCESS (0)
#define SERIALIZE_FAILURE (-1)

typedef struct serialize_t serialize_t;

/**
 * Each serialize object can be of three different types: read, write and measure. The point of choosing a
 * type is to let the user write their serialization routines once, and utilize polymorphism to read/write/measure,
 * thus reducing code duplication.
 */
typedef enum
{
	/**
	 * Reads bits out of the source io, and stores them in user variables.
	 */
	SERIALIZE_READ,
	
	/**
	 * Writes bits to the source io.
	 */
	SERIALIZE_WRITE,
	
	/**
	 * Measures how many bits would have been written/read to io, but doesn't perform any actual io. Bytes "written"
	 * can be retrieved via `serialize_serialized_bytes` at any time.
	 */
	SERIALIZE_MEASURE,
} serialize_type_t;

/**
 * Creates a serialize object using an in-memory buffer as the underlying io. `buffer` is expected to be a valid and opened `FILE*`.
 * Make sure to open the file with the binary flag, like `fopen(path, "wb") or `fopen(path, "rb")`.
 */
serialize_t* serialize_buffer_create(serialize_type_t type, void* buffer, int size, void* user_allocator_ctx_can_be_null);

/**
 * Creates a serialize object using a file (FILE*) as the underlying io.
 * IMPORTANT NOTE:
 *   If you do not define `SERIALIZE_FREAD` and `SERIALIZE_FWRITE` yourself, this header will assume you want
 *   to use `fread` and `fwrite` respectively, which means you must pass in a `FILE*` to `serialize_file_create`
 *   as the `file` parameter.
 */
serialize_t* serialize_file_create(serialize_type_t type, void* file, void* user_allocator_ctx_can_be_null);

/**
 * Cleans up all resources for `io`. Does *not* close the file.
 */
void serialize_destroy(serialize_t* io);

/**
 * Serializes a number of bits to the underlying io. `num_bits` can not be higher than 32. To serialize
 * signed integers, simply perform a typecast to `SERIALIZE_UINT32` before calling.
 */
int serialize_bits(serialize_t* io, SERIALIZE_UINT32* bits, unsigned num_bits);

/**
 * Serializes a 32-bit unsigned integer to the underlying io. `min` and `max` are value ranges for this
 * particular integer. To serialize a full 4-bytes, supply `0` and `UINT_MAX` to the `min` and `max`
 * parameters respectively, or optionally use `serialize_uint32_full`.
 */
int serialize_uint32(serialize_t* io, SERIALIZE_UINT32* val, SERIALIZE_UINT32 min, SERIALIZE_UINT32 max);
#define serialize_uint32_full(io, val) serialize_uint32(io, val, 0, UINT_MAX)

/**
 * Serializes a 64-bit unsigned integer to the underlying io. `min` and `max` are value ranges for this
 * particular integer. To serialize a full 8-bytes, supply `0` and `ULLONG_MAX` to the `min` and `max`
 * parameters respectively, or optionally use `serialize_uint64_full`.
 */
int serialize_uint64(serialize_t* io, SERIALIZE_UINT64* val, SERIALIZE_UINT64 min, SERIALIZE_UINT64 max);
#define serialize_uint64_full(io, val) serialize_uint64(io, val, 0, ULLONG_MAX)

/**
 * Serializes a 32-bit signed integer to the underlying io. `min` and `max` are value ranges for this
 * particular integer. To serialize a full 4-bytes, supply `0` and `INT_MAX` to the `min` and `max`
 * parameters respectively, or optionally use `serialize_int32_full`.
 */
int serialize_int32(serialize_t* io, SERIALIZE_INT32* val, SERIALIZE_INT32 min, SERIALIZE_INT32 max);
#define serialize_int32_full(io, val) serialize_int32(io, val, 0, INT_MAX)

/**
 * Serializes a 64-bit signed integer to the underlying io. `min` and `max` are value ranges for this
 * particular integer. To serialize a full 8-bytes, supply `0` and `LLONG_MAX` to the `min` and `max`
 * parameters respectively, or optionally use `serialize_int64_full`.
 */
int serialize_int64(serialize_t* io, SERIALIZE_INT64* val, SERIALIZE_INT64 min, SERIALIZE_INT64 max);
#define serialize_int64_full(io, val) serialize_int64(io, val, 0, LLONG_MAX)

/**
 * Serializes a 32-bit float to the underlying io as a raw 4-byte value.
 */
int serialize_float(serialize_t* io, float* val);

/**
 * Serializes a 64-bit float to the underlying io as a raw 8-byte value.
 */
int serialize_double(serialize_t* io, double* val);

/**
 * Serializes the buffer as a series of bytes to the underlying io.
 */
int serialize_buffer(serialize_t* io, void* buffer, int size);

/**
 * Serializes a series of bytes to the underlying io. Does not serialize the length or the null
 * terminator. It is recommended to serialize the length of your byte buffer before calling this
 * function, or to serialize a `0` sentinel byte afterwards.
 */
int serialize_bytes(serialize_t* io, unsigned char* bytes, int len);

/**
 * Serializes a series of four bytes. Upon reading, if the bytes read do not match the bytes
 * in `fourcc`, will return `SERIALIZE_FAILURE`.
 */
int serialize_fourcc(serialize_t* io, const char fourcc[4]);

/**
 * Returns the amount of io performed in bytes since the creation, or resetting, of `io`.
 */
int serialize_serialized_bytes(serialize_t* io);

/**
 * Commits any buffered bits to the underlying io. Bits are padded with 0 bits up to the nearest
 * byte boundary. This function should *always* be called once serialization is finished, at least
 * once, but typically only once when all io is finished. For example, call this function just before
 * calling `fclose` during file io.
 */
int serialize_flush(serialize_t* io);

/**
 * Resets the serialize object's state, ready to start writing to beginning of the buffer. The main
 * purpose is to re-use the `io` object without performing allocation/deallocation via `create` and
 * `destroy` functions.
 */
void serialize_reset_buffer(serialize_t* io, serialize_type_t type, void* buffer, int size);

/**
 * Resets the serialize object's state, ready to start measuring. The main purpose is to re-use the
 * `io` object without performing allocation/deallocation via `create` and `destroy` functions.
 */
void serialize_reset_measure(serialize_t* io);

/**
 * Returns the internal pointer to the buffer handed to `io` upon creation via `serialize_buffer_create`.
 */
void* serialize_get_buffer(serialize_t* io);

/**
 * Returns the internal file handle handed to `io` upon creation via `serialize_file_create`.
 */
void* serialize_get_file(serialize_t* io);

/**
 * Returns the type of the io operations (read/write/measure).
 */
serialize_type_t serialize_get_type(serialize_t* io);

#ifdef SERIALIZE_UNIT_TESTS
int serialize_do_unit_tests();
int serialize_do_file_unit_test(const char* path);
#endif

#define CUTE_SERIALIZE_H
#endif

#if defined(CUTE_SERIALIZE_IMPLEMENTATION)
#if !defined(CUTE_SERIALIZE_IMPLEMENTATION_ONCE)
#define CUTE_SERIALIZE_IMPLEMENTATION_ONCE

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#if !defined(SERIALIZE_ALLOC)
	#include <stdlib.h>
	#define SERIALIZE_ALLOC(size, ctx) malloc(size)
	#define SERIALIZE_FREE(mem, ctx) free(mem)
#endif

#if !defined(SERIALIZE_ASSERT)
	#include <assert.h>
	#define SERIALIZE_ASSERT assert
#endif

#if !defined(SERIALIZE_FREAD)
	#include <stdio.h>
	#define SERIALIZE_FREAD(buffer, element_size, element_count, stream) fread((FILE*)buffer, element_size, element_count, stream)
	#define SERIALIZE_FWRITE(buffer, element_size, element_count, stream) fwrite((FILE*)buffer, element_size, element_count, stream)
#endif

#if !defined(SERIALIZE_HOST_TO_IO_UINT32)
	#define SERIALIZE_HOST_TO_IO_UINT32(x) x
#endif

#if !defined(SERIALIZE_HOST_TO_IO_UINT64)
	#define SERIALIZE_HOST_TO_IO_UINT64(x) x
#endif

#if !defined(SERIALIZE_IO_TO_HOST_UINT32)
	#define SERIALIZE_IO_TO_HOST_UINT32(x) x
#endif

#if !defined(SERIALIZE_IO_TO_HOST_UINT64)
	#define SERIALIZE_IO_TO_HOST_UINT64(x) x
#endif

struct serialize_t
{
	serialize_type_t type;
	void* file;
	unsigned char* buffer;
	unsigned char* end;
	int size;

	int measure_bytes;
	int bit_count;
	SERIALIZE_UINT64 bits;
	void* ctx;
};

serialize_t* serialize_buffer_create(serialize_type_t type, void* buffer, int size, void* user_allocator_ctx_can_be_null)
{
	serialize_t* io = (serialize_t*)SERIALIZE_ALLOC(sizeof(serialize_t), user_allocator_ctx_can_be_null);
	io->type = type;
	io->file = 0;
	io->buffer = (unsigned char*)buffer;
	io->end = io->buffer + size;
	io->size = size;
	io->bits = 0;
	io->measure_bytes = 0;
	io->bit_count = 0;
	io->ctx = user_allocator_ctx_can_be_null;
	return io;
}

serialize_t* serialize_file_create(serialize_type_t type, void* file, void* user_allocator_ctx_can_be_null)
{
	serialize_t* io = (serialize_t*)SERIALIZE_ALLOC(sizeof(serialize_t), user_allocator_ctx_can_be_null);
	io->type = type;
	io->file = (FILE*)file;
	io->buffer = 0;
	io->end = 0;
	io->size = 0;
	io->bits = 0;
	io->measure_bytes = 0;
	io->bit_count = 0;
	io->ctx = user_allocator_ctx_can_be_null;
	return io;
}

void serialize_destroy(serialize_t* io)
{
	SERIALIZE_FREE(io, io->ctx);
}

int serialize_bits(serialize_t* io, SERIALIZE_UINT32* bits, unsigned num_bits)
{
	SERIALIZE_ASSERT(num_bits > 0);
	SERIALIZE_ASSERT(num_bits <= 32);
	SERIALIZE_ASSERT(io->bit_count >= 0);
	SERIALIZE_ASSERT(io->bit_count <= 64);

	if (io->file) {
		switch (io->type)
		{
		case SERIALIZE_WRITE:
			io->bits |= ((SERIALIZE_UINT64)*bits << io->bit_count);
			io->bit_count += num_bits;
			SERIALIZE_ASSERT(io->bit_count <= 64);
			if (io->bit_count >= 32) {
				SERIALIZE_UINT64 bits = SERIALIZE_HOST_TO_IO_UINT64(io->bits);
				int bytes_written = (int)SERIALIZE_FWRITE(&bits, 1, 4, io->file);
				if (bytes_written != 4) {
					return SERIALIZE_FAILURE;
				}
				io->measure_bytes += 4;
				io->bits >>= 32ULL;
				io->bit_count -= 32;
			}
			SERIALIZE_ASSERT(io->bit_count >= 0);
			SERIALIZE_ASSERT(io->bit_count <= 32);
			break;

		case SERIALIZE_READ:
			if (io->bit_count < 32) {
				SERIALIZE_UINT32 a = 0;
				int bytes_read = (int)SERIALIZE_FREAD(&a, 1, 4, io->file);
				a = SERIALIZE_IO_TO_HOST_UINT32(a);
				io->bits |= (SERIALIZE_UINT64)a << io->bit_count;
				io->bit_count += bytes_read * 8;
				io->measure_bytes += bytes_read;
				SERIALIZE_ASSERT(io->bit_count <= 64);
				if (io->bit_count < (int)num_bits) {
					return SERIALIZE_FAILURE;
				}
			}

			*bits = io->bits & ((1ULL << (SERIALIZE_UINT64)num_bits) - 1ULL);
			io->bits >>= num_bits;
			io->bit_count -= num_bits;
			break;

		case SERIALIZE_MEASURE:
			io->bit_count += num_bits;
			if (io->bit_count > 8) {
				io->measure_bytes += io->bit_count / 8;
				io->bit_count %= 8;
			}
			break;
		}
	} else {
		switch (io->type)
		{
		case SERIALIZE_WRITE:
			io->bits |= (*bits << io->bit_count);
			io->bit_count += num_bits;
			SERIALIZE_ASSERT(io->bit_count <= 64);
			while (io->bit_count >= 8) {
				SERIALIZE_ASSERT(io->buffer < io->end);
				*io->buffer++ = SERIALIZE_HOST_TO_IO_UINT64(io->bits & 0xFF);
				io->bits >>= 8;
				io->bit_count -= 8;
				io->measure_bytes++;
			}
			break;

		case SERIALIZE_READ:
		{
			int bits_to_read = num_bits;
			while (io->bit_count < (int)num_bits) {
				SERIALIZE_ASSERT(io->bit_count <= 64);
				SERIALIZE_ASSERT(io->buffer < io->end);
				SERIALIZE_UINT64 bits = (SERIALIZE_UINT64)(*io->buffer++);
				io->bits |= SERIALIZE_IO_TO_HOST_UINT64(bits) << io->bit_count;
				io->bit_count += 8;
				bits_to_read -= 8;
				io->measure_bytes++;
			}

			*bits = io->bits & ((1ULL << (SERIALIZE_UINT64)num_bits) - 1ULL);
			io->bits >>= num_bits;
			io->bit_count -= num_bits;
		}	break;

		case SERIALIZE_MEASURE:
			io->bit_count += num_bits;
			if (io->bit_count > 8) {
				io->measure_bytes += io->bit_count / 8;
				io->bit_count %= 8;
			}
			break;
		}
	}

	return SERIALIZE_SUCCESS;
}

#if !defined(SERIALIZE_LOG2)
	#if defined(_MSC_VER)

		#include <winnt.h>

		static inline SERIALIZE_UINT64 s_log2(SERIALIZE_UINT64 x)
		{
			unsigned long index = 0;
			_BitScanReverse64(&index, x);
			return index;
		}

		#define	SERIALIZE_LOG2 s_log2

	#elif defined(__GNUC__) || defined(__clang__)

		#define	SERIALIZE_LOG2(x) (64 - __builtin_clzll(x) - 1)

	#else

		static inline SERIALIZE_UINT64 s_log2(SERIALIZE_UINT64 x)
		{
			SERIALIZE_UINT64 i = 0;
			while (x >>=1) ++i;
			return i;
		}

		#define	SERIALIZE_LOG2 s_log2

	#endif
#endif

static inline SERIALIZE_UINT64 s_bits_required(SERIALIZE_UINT64 min, SERIALIZE_UINT64 max)
{
	return (min == max) ? 0 : SERIALIZE_LOG2(max - min) + 1;
}

int serialize_uint32(serialize_t* io, SERIALIZE_UINT32* val, SERIALIZE_UINT32 min, SERIALIZE_UINT32 max)
{
	SERIALIZE_ASSERT(min <= max);
	if (io->type == SERIALIZE_WRITE || io->type == SERIALIZE_MEASURE) {
		SERIALIZE_ASSERT(*val >= min);
		SERIALIZE_ASSERT(*val <= max);
	}
	SERIALIZE_UINT32 bits_required = (SERIALIZE_UINT32)s_bits_required(min, max);
	SERIALIZE_UINT32 offseted_val = *val - min;
	int ret = serialize_bits(io, &offseted_val, bits_required);
	if (io->type == SERIALIZE_READ) {
		*val = offseted_val + min;
	}
	return ret;
}

int serialize_uint64(serialize_t* io, SERIALIZE_UINT64* val, SERIALIZE_UINT64 min, SERIALIZE_UINT64 max)
{
	SERIALIZE_ASSERT(min <= max);
	SERIALIZE_UINT64 bits_required = s_bits_required(min, max);
	if (bits_required > 32) {
		SERIALIZE_UINT32 a = (SERIALIZE_UINT32)*val;
		SERIALIZE_UINT32 b = (SERIALIZE_UINT32)(*val >> 32);
		if (serialize_bits(io, &a, 32) == SERIALIZE_FAILURE) return SERIALIZE_FAILURE;
		if (serialize_bits(io, &b, (unsigned)(bits_required - 32ULL)) == SERIALIZE_FAILURE) return SERIALIZE_FAILURE;
		if (io->type == SERIALIZE_READ) {
			*val = ((SERIALIZE_UINT64)a) | (SERIALIZE_UINT64)b << 32;
		}
	} else {
		SERIALIZE_UINT32 a;
		if (io->type == SERIALIZE_WRITE || io->type == SERIALIZE_MEASURE) {
			a = (SERIALIZE_UINT32)*val;
		}
		if (serialize_uint32(io, &a, (SERIALIZE_UINT32)min, (SERIALIZE_UINT32)max) == SERIALIZE_FAILURE) return SERIALIZE_FAILURE;
		if (io->type == SERIALIZE_READ) {
			*val = a;
		}
	}
	return SERIALIZE_SUCCESS;
}

int serialize_int32(serialize_t* io, SERIALIZE_INT32* val, SERIALIZE_INT32 min, SERIALIZE_INT32 max)
{
	return serialize_uint32(io, (SERIALIZE_UINT32*)val, min, max);
}

int serialize_int64(serialize_t* io, SERIALIZE_INT64* val, SERIALIZE_INT64 min, SERIALIZE_INT64 max)
{
	return serialize_uint64(io, (SERIALIZE_UINT64*)val, min, max);
}

typedef union
{
	SERIALIZE_UINT64 ival;
	float fval;
	double dval;
} serialize_union_t;

int serialize_float(serialize_t* io, float* val)
{
	serialize_union_t u;
	u.fval = *val;
	SERIALIZE_UINT32 ival = (SERIALIZE_UINT32)u.ival;
	int ret = serialize_bits(io, &ival, sizeof(float) * 8);
	if (io->type == SERIALIZE_READ) {
		u.ival = ival;
		*val = u.fval;
	}
	return ret;
}

int serialize_double(serialize_t* io, double* val)
{
	serialize_union_t u;
	u.dval = *val;
	int ret = serialize_uint64(io, &u.ival, 0, ~0ULL);
	if (io->type == SERIALIZE_READ) {
		*val = u.dval;
	}
	return ret;
}

int serialize_buffer(serialize_t* io, void* buffer, int size)
{
	char* buf = (char*)buffer;
	for (int i = 0; i < size; ++i)
	{
		SERIALIZE_UINT32 a = (SERIALIZE_UINT32)buf[i];
		if (serialize_bits(io, &a, 8) != SERIALIZE_SUCCESS) return SERIALIZE_FAILURE;
	}
	return SERIALIZE_SUCCESS;
}

int serialize_bytes(serialize_t* io, unsigned char* bytes, int len)
{
	while (len--)
	{
		unsigned char c = *bytes;
		if (io->type == SERIALIZE_WRITE) {
			++bytes;
		}
		SERIALIZE_UINT32 a = (SERIALIZE_UINT32)c;
		if (serialize_bits(io, &a, 8) != SERIALIZE_SUCCESS) return SERIALIZE_FAILURE;
		if (io->type == SERIALIZE_READ) {
			*bytes++ = (unsigned char)a;
		}
	}
	return SERIALIZE_SUCCESS;
}

int serialize_fourcc(serialize_t* io, const char fourcc[4])
{
	for (int i = 0; i < 4; ++i)
	{
		unsigned x = fourcc[i];
		if (serialize_bits(io, &x, 8) != SERIALIZE_SUCCESS) return SERIALIZE_FAILURE;
		if (serialize_get_type(io) == SERIALIZE_READ) {
			if (fourcc[i] != x) return SERIALIZE_FAILURE;
		}
	}
	return SERIALIZE_SUCCESS;
}

int serialize_serialized_bytes(serialize_t* io)
{
	if (io->type == SERIALIZE_WRITE) {
		return io->measure_bytes + (io->bit_count + 7) / 8;
	} else {
		return io->measure_bytes;
	}
}

int serialize_flush(serialize_t* io)
{
	if (io->type == SERIALIZE_WRITE) {
		if (io->file) {
			if (io->bit_count) {
				int bytes_to_write = (io->bit_count + 7) / 8;
				int bytes_written = (int)SERIALIZE_FWRITE(&io->bits, 1, bytes_to_write, io->file);
				io->bit_count = 0;
				io->bits = 0;
				if (bytes_to_write != bytes_written) {
					return SERIALIZE_FAILURE;
				}
				io->measure_bytes += bytes_written;
			}
		} else {
			while (io->buffer < io->end && io->bit_count) {
				*io->buffer++ = (unsigned char)io->bits;
				io->bits >>= 8;
				if (io->bit_count >= 8) io->bit_count -= 8;
				else io->bit_count = 0;
				io->measure_bytes++;
			}
		}
	} else {
		io->bits = 0;
		io->bit_count = 0;
	}
	return SERIALIZE_SUCCESS;
}

void serialize_reset_buffer(serialize_t* io, serialize_type_t type, void* buffer, int size)
{
	SERIALIZE_ASSERT(io->file == NULL);
	io->type = type;
	io->buffer = (unsigned char*)buffer;
	io->end = io->buffer + size;
	io->size = size;
	io->measure_bytes = 0;
	io->bits = 0;
	io->bit_count = 0;
}

void serialize_reset_measure(serialize_t* io)
{
	SERIALIZE_ASSERT(io->buffer == NULL);
	SERIALIZE_ASSERT(io->file == NULL);
	io->measure_bytes = 0;
	io->bit_count = 0;
}

void* serialize_get_buffer(serialize_t* io)
{
	SERIALIZE_ASSERT(io->file == NULL);
	return io->buffer;
}

void* serialize_get_file(serialize_t* io)
{
	SERIALIZE_ASSERT(io->buffer == NULL);
	return io->file;
}

serialize_type_t serialize_get_type(serialize_t* io)
{
	return io->type;
}

#ifdef SERIALIZE_UNIT_TESTS
#define SERIALIZE_UNIT_TEST_ASSERT(x) do { if (!(x)) { errors = SERIALIZE_FAILURE; goto end; } } while (0)

int serialize_do_unit_tests()
{
	int errors = SERIALIZE_SUCCESS;
	char mem[1024];
	void* buf = mem;
	serialize_t* write_io = serialize_buffer_create(SERIALIZE_WRITE, buf, 1024, NULL);
	serialize_t* read_io = serialize_buffer_create(SERIALIZE_READ, buf, 1024, NULL);

	unsigned a = 3;
	unsigned b = 0;
	serialize_bits(write_io, &a, 2);
	serialize_flush(write_io);
	serialize_bits(read_io, &b, 2);
	SERIALIZE_UNIT_TEST_ASSERT(a == b);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	a = ~0;
	b = 0;
	serialize_bits(write_io, &a, 32);
	serialize_flush(write_io);
	serialize_bits(read_io, &b, 32);
	SERIALIZE_UNIT_TEST_ASSERT(a == b);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	a = 1;
	for (int i = 0; i < 10; ++i)
		serialize_bits(write_io, &a, 2);
	serialize_flush(write_io);
	for (int i = 0; i < 10; ++i)
	{
		b = 0;
		serialize_bits(read_io, &b, 2);
		SERIALIZE_UNIT_TEST_ASSERT(a == b);
	}
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	a = 2;
	for (int i = 0; i < 10; ++i)
		serialize_bits(write_io, &a, 3);
	serialize_flush(write_io);
	for (int i = 0; i < 10; ++i)
	{
		b = 0;
		serialize_bits(read_io, &b, 3);
		SERIALIZE_UNIT_TEST_ASSERT(a == b);
	}
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	a = 17;
	b = 0;
	serialize_uint32(write_io, &a, 0, 255);
	serialize_flush(write_io);
	serialize_uint32(read_io, &b, 0, 255);
	SERIALIZE_UNIT_TEST_ASSERT(a == b);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	a = 1025;
	b = 0;
	serialize_uint32(write_io, &a, 1000, 1500);
	serialize_flush(write_io);
	serialize_uint32(read_io, &b, 1000, 1500);
	SERIALIZE_UNIT_TEST_ASSERT(a == b);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	srand(0);
	for (int i = 0; i < 10; ++i)
	{
		int lo = (SERIALIZE_UINT32)rand();
		int hi = (SERIALIZE_UINT32)rand();
		if (lo > hi) {
			int t = lo;
			lo = hi;
			hi = t;
		} else if (lo == hi) {
			lo = 0;
			hi = 1;
		}
		a = ((SERIALIZE_UINT32)rand() % (hi - lo + 1)) + lo;
		serialize_uint32(write_io, &a, lo, hi);
	}
	serialize_flush(write_io);
	srand(0);
	for (int i = 0; i < 10; ++i)
	{
		int lo = (SERIALIZE_UINT32)rand();
		int hi = (SERIALIZE_UINT32)rand();
		if (lo > hi) {
			int t = lo;
			lo = hi;
			hi = t;
		} else if (lo == hi) {
			lo = 0;
			hi = 1;
		}
		a = ((SERIALIZE_UINT32)rand() % (hi - lo + 1)) + lo;
		b = 0;
		serialize_uint32(read_io, &b, lo, hi);
		SERIALIZE_UNIT_TEST_ASSERT(a == b);
	}
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	SERIALIZE_UINT64 c = 17;
	SERIALIZE_UINT64 d;
	serialize_uint64(write_io, &c, 0, 17);
	serialize_flush(write_io);
	serialize_uint64(read_io, &d, 0, 17);
	SERIALIZE_UNIT_TEST_ASSERT(c == d);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	c = 0xFFFFFFFFFFFFFFFFULL;
	d;
	serialize_uint64(write_io, &c, 0, ~0ULL);
	serialize_flush(write_io);
	serialize_uint64(read_io, &d, 0, ~0ULL);
	SERIALIZE_UNIT_TEST_ASSERT(c == d);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	c = 0x00000000FFFFFFFFULL;
	d;
	serialize_uint64(write_io, &c, 0, ~0UL);
	serialize_flush(write_io);
	serialize_uint64(read_io, &d, 0, ~0UL);
	SERIALIZE_UNIT_TEST_ASSERT(c == d);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	c = 0xFFFFFFFF00000000ULL;
	d;
	serialize_uint64(write_io, &c, 0, ~0ULL);
	serialize_flush(write_io);
	serialize_uint64(read_io, &d, 0, ~0ULL);
	SERIALIZE_UNIT_TEST_ASSERT(c == d);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	c = 0x0000FFFFFFFF0000ULL;
	d;
	serialize_uint64(write_io, &c, 0, ~0ULL);
	serialize_flush(write_io);
	serialize_uint64(read_io, &d, 0, ~0ULL);
	SERIALIZE_UNIT_TEST_ASSERT(c == d);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	float e = 1.23f;
	float f;
	serialize_float(write_io, &e);
	serialize_flush(write_io);
	serialize_float(read_io, &f);
	SERIALIZE_UNIT_TEST_ASSERT(e == f);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	double g = 1013.1293881;
	double h;
	serialize_double(write_io, &g);
	serialize_flush(write_io);
	serialize_double(read_io, &h);
	SERIALIZE_UNIT_TEST_ASSERT(g == h);
	serialize_reset_buffer(write_io, SERIALIZE_WRITE, buf, 1024);
	serialize_reset_buffer(read_io, SERIALIZE_READ, buf, 1024);

	serialize_t* measure_io = serialize_buffer_create(SERIALIZE_MEASURE, NULL, 0, NULL);
	serialize_bits(measure_io, NULL, 13);
	serialize_bits(measure_io, NULL, 5);
	serialize_bits(measure_io, NULL, 19);
	serialize_bits(measure_io, NULL, 24);
	serialize_bits(measure_io, NULL, 12);
	serialize_bits(measure_io, NULL, 27);
	serialize_bits(measure_io, NULL, 31);
	int bytes = serialize_serialized_bytes(measure_io);
	SERIALIZE_UNIT_TEST_ASSERT(bytes == 17);

end:
	serialize_destroy(write_io);
	serialize_destroy(read_io);

	return errors;
}

int serialize_do_file_unit_test(const char* path)
{
	int errors = SERIALIZE_SUCCESS;
	FILE* fp = fopen(path, "wb");
	serialize_t* write_io = serialize_file_create(SERIALIZE_WRITE, fp, NULL);
	unsigned a = 5;
	unsigned b;
	for (int i = 0; i < 10; ++i)
		serialize_bits(write_io, &a, 3);
	serialize_flush(write_io);
	fclose(fp);
	fp = NULL;
	serialize_destroy(write_io);
	write_io = NULL;

	fp = fopen(path, "rb");
	serialize_t* read_io = serialize_file_create(SERIALIZE_READ, fp, NULL);
	for (int i = 0; i < 10; ++i)
	{
		unsigned b = 0;
		serialize_bits(read_io, &b, 3);
		SERIALIZE_UNIT_TEST_ASSERT(a == b);
	}
	fclose(fp);
	fp = NULL;
	serialize_destroy(read_io);
	read_io = NULL;

	fp = fopen(path, "wb");
	write_io = serialize_file_create(SERIALIZE_WRITE, fp, NULL);
	for (int i = 0; i < 26; ++i)
	{
		unsigned letter = 'a' + i;
		serialize_uint32(write_io, &letter, 0, 255);
	}
	serialize_flush(write_io);
	serialize_destroy(write_io);
	write_io = NULL;
	fclose(fp);
	fp = NULL;

	fp = fopen(path, "rb");
	read_io = serialize_file_create(SERIALIZE_READ, fp, NULL);
	for (int i = 0; i < 26; ++i)
	{
		unsigned letter_expected = 'a' + i;
		unsigned letter = 0;
		serialize_uint32(read_io, &letter, 0, 255);
		SERIALIZE_UNIT_TEST_ASSERT(letter == letter_expected);
	}
	serialize_destroy(read_io);
	read_io = NULL;
	fclose(fp);
	fp = NULL;

	fp = fopen(path, "wb");
	write_io = serialize_file_create(SERIALIZE_WRITE, fp, NULL);
	srand(0);
	for (int i = 0; i < 10; ++i)
	{
		int lo = (SERIALIZE_UINT32)rand();
		int hi = (SERIALIZE_UINT32)rand();
		if (lo > hi) {
			int t = lo;
			lo = hi;
			hi = t;
		} else if (lo == hi) {
			lo = 0;
			hi = 1;
		}
		a = ((SERIALIZE_UINT32)rand() % (hi - lo + 1)) + lo;
		serialize_uint32(write_io, &a, lo, hi);
	}
	serialize_flush(write_io);
	serialize_destroy(write_io);
	write_io = NULL;
	fclose(fp);
	fp = NULL;
	fp = fopen(path, "rb");
	read_io = serialize_file_create(SERIALIZE_READ, fp, NULL);
	srand(0);
	for (int i = 0; i < 10; ++i)
	{
		int lo = (SERIALIZE_UINT32)rand();
		int hi = (SERIALIZE_UINT32)rand();
		if (lo > hi) {
			int t = lo;
			lo = hi;
			hi = t;
		} else if (lo == hi) {
			lo = 0;
			hi = 1;
		}
		a = ((SERIALIZE_UINT32)rand() % (hi - lo + 1)) + lo;
		b = 0;
		serialize_uint32(read_io, &b, lo, hi);
		SERIALIZE_UNIT_TEST_ASSERT(a == b);
	}
	fclose(fp);
	fp = NULL;
	serialize_destroy(read_io);
	read_io = NULL;

end:
	if (fp) fclose(fp);
	if (write_io) serialize_destroy(write_io);
	if (read_io) serialize_destroy(read_io);

	return errors;
}

#endif // SERIALIZE_NO_UNIT_TESTS

#endif // CUTE_SERIALIZE_IMPLEMENTATION_ONCE
#endif // CUTE_SERIALIZE_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2018 Randy Gaul http://www.randygaul.net
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
