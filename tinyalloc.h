/*
	tinyalloc.h - v1.01

	SUMMARY:
	This header contains a collection of allocators. None of the allocators are
	very fancy, and all have particular limitations making them useful only in
	specific scenarios. None of the allocators do any special alignment support.

	There are a few kinds:

		1. taStack          - stack based allocator
		2. taFrame          - frame based scratch allocator
		3. TA_ALLOC/TA_FREE - malloc/free leak checker

	And here are their descriptions:

		1. Given an initial chunk of memory, implements a stack based allocator inside
		the chunk. Each allocation is laid contiguously after the last. Deallocation must
		occur in *reverse* order to allocation. Mostly useful for graph traversals.

		2. Given an initial chunk of memory, acts like the stack allocator, but only
		implements a single deallocation function. Upon deallocation, the entire stack is
		cleared, typically by moving a single pointer. Mostly useful for loading level
		assets, or for quick "temporary scratch-space" in the middle of an algorithm.

		3. Thin wrapper around malloc/free for recording and reporting memory leaks. Call
		TA_CHECK_FOR_LEAKS to find and printf any un-free'd memory. Define TA_LEAK_CHECK
		to 0 in order to turn of leak checking and use raw malloc/free.

	Revision history:
		1.0  (11/01/2017) initial release
		1.01 (11/11/2017) added leak checker for malloc/free
*/

/*
	To create implementation (the function definitions)
		#define TINYALLOC_IMPL
	in *one* C/CPP file (translation unit) that includes this file
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

// 1 - use the leak checker
// 0 - use plain malloc/free
#define TA_LEAK_CHECK 1
#define TA_ALLOC(size) taLeakCheckAlloc((size), (char*)__FILE__, __LINE__)
#define TA_FREE(mem) taLeakCheckFree(mem)
void* taLeakCheckAlloc(size_t size, char* file, int line);
void taLeakCheckFree(void* mem);
int TA_CHECK_FOR_LEAKS();

// define these to your own user definition as necessary
#if !defined(TA_MALLOC_FUNC)
	#define TA_MALLOC_NEED_HEADER
	#define TA_MALLOC_FUNC(size, ...) malloc(size)
#endif

#if !defined(TA_FREE_FUNC)
	#if !defined(TA_MALLOC_NEED_HEADER)
		#define TA_MALLOC_NEED_HEADER
	#endif
	#define TA_FREE_FUNC(mem, ...) free(free)
#endif

#define TINYALLOC_H
#endif

#if defined(TINYALLOC_IMPL)

struct taStack
{
	void* memory;
	size_t capacity;
	size_t bytes_left;
};

#define TA_PTR_ADD(ptr, size) ((void*)(((char*)ptr) + (size)))
#define TA_PTR_SUB(ptr, size) ((void*)(((char*)ptr) - (size)))

taStack* taStackCreate(void* memory_chunk, size_t size)
{
	taStack* stack = (taStack*)memory_chunk;
	if (size < sizeof(taStack)) return 0;
	*(size_t*)TA_PTR_ADD(memory_chunk, sizeof(taStack)) = 0;
	stack->memory = TA_PTR_ADD(memory_chunk, sizeof(taStack) + sizeof(size_t));
	stack->capacity = size - sizeof(taStack) - sizeof(size_t);
	stack->bytes_left = stack->capacity;
	return stack;
}

void* taStackAlloc(taStack* stack, size_t size)
{
	if (stack->bytes_left - sizeof(size_t) < size) return 0;
	void* user_mem = stack->memory;
	*(size_t*)TA_PTR_ADD(user_mem, size) = size;
	stack->memory = TA_PTR_ADD(user_mem, size + sizeof(size_t));
	stack->bytes_left -= sizeof(size_t) + size;
	return user_mem;
}

int taStackFree(taStack* stack, void* memory)
{
	if (!memory) return 0;
	size_t size = *(size_t*)TA_PTR_SUB(stack->memory, sizeof(size_t));
	void* prev = TA_PTR_SUB(stack->memory, size + sizeof(size_t));
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
	frame->original = TA_PTR_ADD(memory_chunk, sizeof(taFrame));
	frame->ptr = frame->original;
	frame->capacity = frame->bytes_left = size - sizeof(taFrame);
	return frame;
}

void* taFrameAlloc(taFrame* frame, size_t size)
{
	if (frame->bytes_left < size) return 0;
	void* user_mem = frame->ptr;
	frame->ptr = TA_PTR_ADD(frame->ptr, size);
	frame->bytes_left -= size;
	return user_mem;
}

void taFrameFree(taFrame* frame)
{
	frame->ptr = frame->original;
	frame->bytes_left = frame->capacity;
}

#if defined(TA_MALLOC_NEED_HEADER)
#include <stdlib.h>	// malloc, free
#endif

#if TA_LEAK_CHECK
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
	taAllocInfo* mem = (taAllocInfo*)TA_MALLOC_FUNC(sizeof(taAllocInfo) + size, file, line);

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

void taLeakCheckFree(void* mem)
{
	if (!mem) return;

	taAllocInfo* info = (taAllocInfo*)mem - 1;
	info->prev->next = info->next;
	info->next->prev = info->prev;

	TA_FREE_FUNC(info);
}

int TA_CHECK_FOR_LEAKS()
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
#else
inline void* taLeakCheckAlloc(size_t size, char* file, int line)
{
	(void)file;
	(void)line;
	return malloc(size);
}

inline void taLeakCheckFree(void* mem)
{
	return free(mem);
}

inline int TA_CHECK_FOR_LEAKS() {}
#endif // TA_LEAK_CHECK

#endif // TINYALLOC_IMPL

/*
	This is free and unencumbered software released into the public domain.
	Our intent is that anyone is free to copy and use this software,
	for any purpose, in any form, and by any means.
	The authors dedicate any and all copyright interest in the software
	to the public domain, at their own expense for the betterment of mankind.
	The software is provided "as is", without any kind of warranty, including
	any implied warranty. If it breaks, you get to keep both pieces.
*/
