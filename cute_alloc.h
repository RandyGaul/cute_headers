/*
	cute_alloc.h - v1.01

	To create implementation (the function definitions)
		#define CUTE_ALLOC_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:
	This header contains a collection of allocators. None of the allocators are
	very fancy, and all have particular limitations making them useful only in
	specific scenarios. None of the allocators do any special alignment support.

	There are a few kinds:

		1. ca_stack_t          - stack based allocator
		2. ca_frame_t          - frame based scratch allocator
		3. CUTE_ALLOC_ALLOC/CUTE_ALLOC_FREE - malloc/free leak checker

	And here are their descriptions:

		1. Given an initial chunk of memory, implements a stack based allocator inside
		the chunk. Each allocation is laid contiguously after the last. Deallocation must
		occur in *reverse* order to allocation. Mostly useful for graph traversals.

		2. Given an initial chunk of memory, acts like the stack allocator, but only
		implements a single deallocation function. Upon deallocation, the entire stack is
		cleared, typically by moving a single pointer. Mostly useful for loading level
		assets, or for quick "temporary scratch-space" in the middle of an algorithm.

		3. Thin wrapper around malloc/free for recording and reporting memory leaks. Call
		CUTE_ALLOC_CHECK_FOR_LEAKS to find and printf any un-free'd memory. Define CUTE_ALLOC_LEAK_CHECK
		to 0 in order to turn of leak checking and use raw malloc/free.

	Revision history:
		1.0  (11/01/2017) initial release
		1.01 (11/11/2017) added leak checker for malloc/free
*/

#if !defined(CUTE_ALLOC_H)

typedef struct ca_stack_t ca_stack_t;
ca_stack_t* ca_stack_create(void* memory_chunk, size_t size);
void* ca_stack_alloc(ca_stack_t* stack, size_t size);
int ca_stack_free(ca_stack_t* stack, void* memory);
size_t ca_stack_bytes_left(ca_stack_t* stack);

typedef struct ca_frame_t ca_frame_t;
ca_frame_t* ca_frame_create(void* memory_chunk, size_t size);
void* ca_frame_alloc(ca_frame_t* frame, size_t size);
void ca_frame_free(ca_frame_t* frame);

typedef struct ca_heap_t ca_heap_t;
ca_heap_t* ca_heap_create(void* memory_chunk, size_t size);
void* ca_heap_alloc(ca_heap_t* frame, size_t size);
void ca_heap_free(ca_heap_t* frame);

// define these to your own user definition as necessary
#if !defined(CUTE_ALLOC_ALLOC)
	#define CUTE_ALLOC_ALLOC(size, ctx) ca_leak_check_alloc((size), (char*)__FILE__, __LINE__)
#endif

#if !defined(CUTE_ALLOC_FREE)
	#define CUTE_ALLOC_FREE(mem, ctx) ca_leak_check_free(mem)
#endif

#if !defined(CUTE_ALLOC_CALLOC)
	#define CUTE_ALLOC_CALLOC(count, element_size, ctx) ca_leak_check_calloc(count, element_size, (char*)__FILE__, __LINE__)
#endif

// 1 - use the leak checker
// 0 - use plain malloc/free
#define CUTE_ALLOC_LEAK_CHECK 1
void* ca_leak_check_alloc(size_t size, const char* file, int line);
void* ca_leak_check_calloc(size_t count, size_t element_size, const char* file, int line);
void ca_leak_check_free(void* mem);
int CUTE_ALLOC_CHECK_FOR_LEAKS();
int CUTE_ALLOC_BYTES_IN_USE();

// define these to your own user definition as necessary
#if !defined(CUTE_ALLOC_MALLOC_FUNC)
	#include <stdlib.h>
	#define CUTE_ALLOC_MALLOC_FUNC malloc
#endif

#if !defined(CUTE_ALLOC_FREE_FUNC)
	#include <stdlib.h>
	#define CUTE_ALLOC_FREE_FUNC free
#endif

#if !defined(CUTE_ALLOC_CALLOC_FUNC)
	#include <stdlib.h>
	#define CUTE_ALLOC_CALLOC_FUNC calloc
#endif

#define CUTE_ALLOC_H
#endif

#ifdef CUTE_ALLOC_IMPLEMENTATION
#ifndef CUTE_ALLOC_IMPLEMENTATION_ONCE
#define CUTE_ALLOC_IMPLEMENTATION_ONCE

struct ca_stack_t
{
	void* memory;
	size_t capacity;
	size_t bytes_left;
};

#define CUTE_ALLOC_PTR_ADD(ptr, size) ((void*)(((char*)ptr) + (size)))
#define CUTE_ALLOC_PTR_SUB(ptr, size) ((void*)(((char*)ptr) - (size)))

ca_stack_t* ca_stack_create(void* memory_chunk, size_t size)
{
	ca_stack_t* stack = (ca_stack_t*)memory_chunk;
	if (size < sizeof(ca_stack_t)) return 0;
	*(size_t*)CUTE_ALLOC_PTR_ADD(memory_chunk, sizeof(ca_stack_t)) = 0;
	stack->memory = CUTE_ALLOC_PTR_ADD(memory_chunk, sizeof(ca_stack_t) + sizeof(size_t));
	stack->capacity = size - sizeof(ca_stack_t) - sizeof(size_t);
	stack->bytes_left = stack->capacity;
	return stack;
}

void* ca_stack_alloc(ca_stack_t* stack, size_t size)
{
	if (stack->bytes_left - sizeof(size_t) < size) return 0;
	void* user_mem = stack->memory;
	*(size_t*)CUTE_ALLOC_PTR_ADD(user_mem, size) = size;
	stack->memory = CUTE_ALLOC_PTR_ADD(user_mem, size + sizeof(size_t));
	stack->bytes_left -= sizeof(size_t) + size;
	return user_mem;
}

int ca_stack_free(ca_stack_t* stack, void* memory)
{
	if (!memory) return 0;
	size_t size = *(size_t*)CUTE_ALLOC_PTR_SUB(stack->memory, sizeof(size_t));
	void* prev = CUTE_ALLOC_PTR_SUB(stack->memory, size + sizeof(size_t));
	if (prev != memory) return 0;
	stack->memory = prev;
	stack->bytes_left += sizeof(size_t) + size;
	return 1;
}

size_t ca_stack_bytes_left(ca_stack_t* stack)
{
	return stack->bytes_left;
}

struct ca_frame_t
{
	void* original;
	void* ptr;
	size_t capacity;
	size_t bytes_left;
};

ca_frame_t* ca_frame_create(void* memory_chunk, size_t size)
{
	ca_frame_t* frame = (ca_frame_t*)memory_chunk;
	frame->original = CUTE_ALLOC_PTR_ADD(memory_chunk, sizeof(ca_frame_t));
	frame->ptr = frame->original;
	frame->capacity = frame->bytes_left = size - sizeof(ca_frame_t);
	return frame;
}

void* ca_frame_alloc(ca_frame_t* frame, size_t size)
{
	if (frame->bytes_left < size) return 0;
	void* user_mem = frame->ptr;
	frame->ptr = CUTE_ALLOC_PTR_ADD(frame->ptr, size);
	frame->bytes_left -= size;
	return user_mem;
}

void ca_frame_free(ca_frame_t* frame)
{
	frame->ptr = frame->original;
	frame->bytes_left = frame->capacity;
}

typedef struct ca_heap_header_t
{
	struct ca_heap_header_t* next;
	struct ca_heap_header_t* prev;
	size_t size;
} ca_heap_header_t;

#if CUTE_ALLOC_LEAK_CHECK
#include <stdio.h>

typedef struct ca_alloc_info_t ca_alloc_info_t;
struct ca_alloc_info_t
{
	const char* file;
	size_t size;
	int line;

	struct ca_alloc_info_t* next;
	struct ca_alloc_info_t* prev;
};

static ca_alloc_info_t* ca_alloc_head()
{
	static ca_alloc_info_t info;
	static int init;

	if (!init)
	{
		info.next = &info;
		info.prev = &info;
		init = 1;
	}

	return &info;
}

void* ca_leak_check_alloc(size_t size, const char* file, int line)
{
	ca_alloc_info_t* mem = (ca_alloc_info_t*)CUTE_ALLOC_MALLOC_FUNC(sizeof(ca_alloc_info_t) + size);

	if (!mem) return 0;

	mem->file = file;
	mem->line = line;
	mem->size = size;
	ca_alloc_info_t* head = ca_alloc_head();
	mem->prev = head;
	mem->next = head->next;
	head->next->prev = mem;
	head->next = mem;

	return mem + 1;
}

#if !defined(CUTE_ALLOC_MEMSET)
	#include <string.h> // memset
	#define CUTE_ALLOC_MEMSET memset
#endif

void* ca_leak_check_calloc(size_t count, size_t element_size, const char* file, int line)
{
	size_t size = count * element_size;
	void* mem = ca_leak_check_alloc(size, file, line);
	CUTE_ALLOC_MEMSET(mem, 0, size);
	return mem;
}

void ca_leak_check_free(void* mem)
{
	if (!mem) return;

	ca_alloc_info_t* info = (ca_alloc_info_t*)mem - 1;
	info->prev->next = info->next;
	info->next->prev = info->prev;

	CUTE_ALLOC_FREE_FUNC(info);
}

int CUTE_ALLOC_CHECK_FOR_LEAKS()
{
	ca_alloc_info_t* head = ca_alloc_head();
	ca_alloc_info_t* next = head->next;
	int leaks = 0;

	while (next != head)
	{
		printf("LEAKED %llu bytes from file \"%s\" at line %d from address %p.\n", next->size, next->file, next->line, next + 1);
		next = next->next;
		leaks = 1;
	}

	if (leaks) printf("WARNING: Memory leaks detected (see above).\n");
	else printf("SUCCESS: No memory leaks detected.\n");
	return leaks;
}

int CUTE_ALLOC_BYTES_IN_USE()
{
	ca_alloc_info_t* head = ca_alloc_head();
	ca_alloc_info_t* next = head->next;
	int bytes = 0;

	while (next != head)
	{
		bytes += (int)next->size;
		next = next->next;
	}

	return bytes;
}
#else
#if !defined(CUTE_ALLOC_UNUSED)
	#if defined(_MSC_VER)
		#define CUTE_ALLOC_UNUSED(x) (void)x
	#else
		#define CUTE_ALLOC_UNUSED(x) (void)(sizeof(x))
	#endif
#endif

inline void* ca_leak_check_alloc(size_t size, char* file, int line)
{
	CUTE_ALLOC_UNUSED(file);
	CUTE_ALLOC_UNUSED(line);
	return CUTE_ALLOC_MALLOC_FUNC(size);
}

void* ca_leak_check_calloc(size_t count, size_t element_size, char* file, int line)
{
	CUTE_ALLOC_UNUSED(file);
	CUTE_ALLOC_UNUSED(line);
	return CUTE_ALLOC_CALLOC_FUNC(count, size);
}

inline void ca_leak_check_free(void* mem)
{
	return CUTE_ALLOC_FREE_FUNC(mem);
}

inline int CUTE_ALLOC_CHECK_FOR_LEAKS() { return 0; }
inline int CUTE_ALLOC_BYTES_IN_USE() { return 0; }
#endif // CUTE_ALLOC_LEAK_CHECK

#endif // CUTE_ALLOC_IMPLEMENTATION_ONCE
#endif // CUTE_ALLOC_IMPLEMENTATION

/*
	This is free and unencumbered software released into the public domain.
	Our intent is that anyone is free to copy and use this software,
	for any purpose, in any form, and by any means.
	The authors dedicate any and all copyright interest in the software
	to the public domain, at their own expense for the betterment of mankind.
	The software is provided "as is", without any kind of warranty, including
	any implied warranty. If it breaks, you get to keep both pieces.
*/
