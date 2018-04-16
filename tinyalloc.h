/*
	tinyalloc.h - v1.01
	
	To create implementation (the function definitions)
		#define TINYALLOC_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:
	This header contains a collection of allocators. None of the allocators are
	very fancy, and all have particular limitations making them useful only in
	specific scenarios. None of the allocators do any special alignment support.

	There are a few kinds:

		1. taStack          - stack based allocator
		2. taFrame          - frame based scratch allocator
		3. TINYALLOC_ALLOC/TINYALLOC_FREE - malloc/free leak checker

	And here are their descriptions:

		1. Given an initial chunk of memory, implements a stack based allocator inside
		the chunk. Each allocation is laid contiguously after the last. Deallocation must
		occur in *reverse* order to allocation. Mostly useful for graph traversals.

		2. Given an initial chunk of memory, acts like the stack allocator, but only
		implements a single deallocation function. Upon deallocation, the entire stack is
		cleared, typically by moving a single pointer. Mostly useful for loading level
		assets, or for quick "temporary scratch-space" in the middle of an algorithm.

		3. Thin wrapper around malloc/free for recording and reporting memory leaks. Call
		TINYALLOC_CHECK_FOR_LEAKS to find and printf any un-free'd memory. Define TINYALLOC_LEAK_CHECK
		to 0 in order to turn of leak checking and use raw malloc/free.

	Revision history:
		1.0  (11/01/2017) initial release
		1.01 (11/11/2017) added leak checker for malloc/free
*/

#if !defined(TINYALLOC_H)

typedef struct taStack taStack;
taStack* taStackCreate(void* memory_chunk, size_t size);
void* taStackAlloc(taStack* stack, size_t size);
int taStackFree(taStack* stack, void* memory);
size_t taStackBytesLeft(taStack* stack);

typedef struct taFrame taFrame;
taFrame* taFrameCreate(void* memory_chunk, size_t size);
void* taFrameAlloc(taFrame* frame, size_t size);
void taFrameFree(taFrame* frame);

// define these to your own user definition as necessary
#if !defined(TINYALLOC_ALLOC)
	#define TINYALLOC_ALLOC(size, ctx) taLeakCheckAlloc((size), (char*)__FILE__, __LINE__)
#endif

#if !defined(TINYALLOC_FREE)
	#define TINYALLOC_FREE(mem, ctx) taLeakCheckFree(mem)
#endif

#if !defined(TINYALLOC_CALLOC)
	#define TINYALLOC_CALLOC(count, elementSize, ctx) taLeakCheckCalloc(count, elementSize, (char*)__FILE__, __LINE__)
#endif

// 1 - use the leak checker
// 0 - use plain malloc/free
#define TINYALLOC_LEAK_CHECK 1
void* taLeakCheckAlloc(size_t size, char* file, int line);
void* taLeakCheckCalloc(size_t count, size_t elementSize, char* file, int line);
void taLeakCheckFree(void* mem);
int TINYALLOC_CHECK_FOR_LEAKS();
int TINYALLOC_BYTES_IN_USE();

// define these to your own user definition as necessary
#if !defined(TINYALLOC_MALLOC_FUNC)
	#define TINYALLOC_MALLOC_FUNC malloc
#endif

#if !defined(TINYALLOC_FREE_FUNC)
	#define TINYALLOC_FREE_FUNC free
#endif

#if !defined(TINYALLOC_CALLOC_FUNC)
	#define TINYALLOC_CALLOC_FUNC calloc
#endif

#define TINYALLOC_H
#endif

#ifdef TINYALLOC_IMPLEMENTATION
#ifndef TINYALLOC_IMPLEMENTATION_ONCE
#define TINYALLOC_IMPLEMENTATION_ONCE

struct taStack
{
	void* memory;
	size_t capacity;
	size_t bytes_left;
};

#define TINYALLOC_PTR_ADD(ptr, size) ((void*)(((char*)ptr) + (size)))
#define TINYALLOC_PTR_SUB(ptr, size) ((void*)(((char*)ptr) - (size)))

taStack* taStackCreate(void* memory_chunk, size_t size)
{
	taStack* stack = (taStack*)memory_chunk;
	if (size < sizeof(taStack)) return 0;
	*(size_t*)TINYALLOC_PTR_ADD(memory_chunk, sizeof(taStack)) = 0;
	stack->memory = TINYALLOC_PTR_ADD(memory_chunk, sizeof(taStack) + sizeof(size_t));
	stack->capacity = size - sizeof(taStack) - sizeof(size_t);
	stack->bytes_left = stack->capacity;
	return stack;
}

void* taStackAlloc(taStack* stack, size_t size)
{
	if (stack->bytes_left - sizeof(size_t) < size) return 0;
	void* user_mem = stack->memory;
	*(size_t*)TINYALLOC_PTR_ADD(user_mem, size) = size;
	stack->memory = TINYALLOC_PTR_ADD(user_mem, size + sizeof(size_t));
	stack->bytes_left -= sizeof(size_t) + size;
	return user_mem;
}

int taStackFree(taStack* stack, void* memory)
{
	if (!memory) return 0;
	size_t size = *(size_t*)TINYALLOC_PTR_SUB(stack->memory, sizeof(size_t));
	void* prev = TINYALLOC_PTR_SUB(stack->memory, size + sizeof(size_t));
	if (prev != memory) return 0;
	stack->memory = prev;
	stack->bytes_left += sizeof(size_t) + size;
	return 1;
}

size_t taStackBytesLeft(taStack* stack)
{
	return stack->bytes_left;
}

struct taFrame
{
	void* original;
	void* ptr;
	size_t capacity;
	size_t bytes_left;
};

taFrame* taFrameCreate(void* memory_chunk, size_t size)
{
	taFrame* frame = (taFrame*)memory_chunk;
	frame->original = TINYALLOC_PTR_ADD(memory_chunk, sizeof(taFrame));
	frame->ptr = frame->original;
	frame->capacity = frame->bytes_left = size - sizeof(taFrame);
	return frame;
}

void* taFrameAlloc(taFrame* frame, size_t size)
{
	if (frame->bytes_left < size) return 0;
	void* user_mem = frame->ptr;
	frame->ptr = TINYALLOC_PTR_ADD(frame->ptr, size);
	frame->bytes_left -= size;
	return user_mem;
}

void taFrameFree(taFrame* frame)
{
	frame->ptr = frame->original;
	frame->bytes_left = frame->capacity;
}

#include <stdlib.h> // malloc, free

#if TINYALLOC_LEAK_CHECK
#include <stdio.h>

typedef struct taAllocInfo taAllocInfo;
struct taAllocInfo
{
	char* file;
	size_t size;
	int line;

	struct taAllocInfo* next;
	struct taAllocInfo* prev;
};

static taAllocInfo* taAllocHead()
{
	static taAllocInfo info;
	static int init;

	if (!init)
	{
		info.next = &info;
		info.prev = &info;
		init = 1;
	}

	return &info;
}

void* taLeakCheckAlloc(size_t size, char* file, int line)
{
	taAllocInfo* mem = (taAllocInfo*)TINYALLOC_MALLOC_FUNC(sizeof(taAllocInfo) + size);

	if (!mem) return 0;

	mem->file = file;
	mem->line = line;
	mem->size = size;
	taAllocInfo* head = taAllocHead();
	mem->prev = head;
	mem->next = head->next;
	head->next->prev = mem;
	head->next = mem;

	return mem + 1;
}

#if !defined(TINYALLOC_MEMSET)
	#include <string.h> // memset
	#define TINYALLOC_MEMSET memset
#endif

void* taLeakCheckCalloc(size_t count, size_t elementSize, char* file, int line)
{
	size_t size = count * elementSize;
	void* mem = taLeakCheckAlloc(size, file, line);
	TINYALLOC_MEMSET(mem, 0, size);
	return mem;
}

void taLeakCheckFree(void* mem)
{
	if (!mem) return;

	taAllocInfo* info = (taAllocInfo*)mem - 1;
	info->prev->next = info->next;
	info->next->prev = info->prev;

	TINYALLOC_FREE_FUNC(info);
}

int TINYALLOC_CHECK_FOR_LEAKS()
{
	taAllocInfo* head = taAllocHead();
	taAllocInfo* next = head->next;
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

int TINYALLOC_BYTES_IN_USE()
{
	taAllocInfo* head = taAllocHead();
	taAllocInfo* next = head->next;
	int bytes = 0;

	while (next != head)
	{
		bytes += (int)next->size;
		next = next->next;
	}

	return bytes;
}
#else
#if !defined(TINYALLOC_UNUSED)
	#if defined(_MSC_VER)
		#define TINYALLOC_UNUSED(x) (void)x
	#else
		#define TINYALLOC_UNUSED(x) (void)(sizeof(x))
	#endif
#endif

inline void* taLeakCheckAlloc(size_t size, char* file, int line)
{
	TINYALLOC_UNUSED(file);
	TINYALLOC_UNUSED(line);
	return TINYALLOC_MALLOC_FUNC(size);
}

void* taLeakCheckCalloc(size_t count, size_t elementSize, char* file, int line)
{
	TINYALLOC_UNUSED(file);
	TINYALLOC_UNUSED(line);
	return TINYALLOC_CALLOC_FUNC(count, size);
}

inline void taLeakCheckFree(void* mem)
{
	return TINYALLOC_FREE_FUNC(mem);
}

inline int TINYALLOC_CHECK_FOR_LEAKS() { return 0; }
inline int TINYALLOC_BYTES_IN_USE() { return 0; }
#endif // TINYALLOC_LEAK_CHECK

#endif // TINYALLOC_IMPLEMENTATION_ONCE
#endif // TINYALLOC_IMPLEMENTATION

/*
	This is free and unencumbered software released into the public domain.
	Our intent is that anyone is free to copy and use this software,
	for any purpose, in any form, and by any means.
	The authors dedicate any and all copyright interest in the software
	to the public domain, at their own expense for the betterment of mankind.
	The software is provided "as is", without any kind of warranty, including
	any implied warranty. If it breaks, you get to keep both pieces.
*/
