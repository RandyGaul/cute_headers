/*
	tinyalloc.h - v1.00

	SUMMARY:
	This header contains a collection of allocators. None of the allocators are
	very fancy, and all have particular limitations making them useful only in
	specific scenarios. None of the allocators do any special alignment support.

	There are a few kinds:

		1. taStack - stack based allocator
		2. taFrame - frame based scratch allocator

	And here are their descriptions:

		1. Given an initial chunk of memory, implements a stack based allocator inside
		the chunk. Each allocation is laid contiguously after the last. Deallocation must
		occur in *reverse* order to allocation. Mostly useful for graph traversals.

		2. Given an initial chunk of memory, acts like the stack allocator, but only
		implements a single deallocation function. Upon deallocation, the entire stack is
		cleared, typically by moving a single pointer. Mostly useful for loading level
		assets, or for quick "temporary scratch-space" in the middle of an algorithm.
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

#endif
