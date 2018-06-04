/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_sync.h - v1.00

	To create implementation (the function definitions)
		#define CUTE_SYNC_IMPLEMENTATION
		#define CUTE_SYNC_SDL
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY

		Collection of practical syncronization primitives. They are currently all
		implemented with the SDL2 API, though cute_sync has been written so it can
		be implemented by other backends, such as pthreads, or Windows primitives.

		Here are some slides I wrote for those interested in learning prequisite
		knowledge for utilizing this header:
		http://www.randygaul.net/2014/09/24/multi-threading-best-practices-for-gamedev/

	Revision history:
		1.0  (05/31/2018) initial release
*/

#if !defined(CUTE_SYNC_H)

#if defined(CUTE_SYNC_SDL)
typedef SDL_mutex          cute_mutex_t;
typedef SDL_cond           cute_cv_t;
typedef SDL_sem            cute_sem_t;
typedef SDL_Thread         cute_thread_t;
typedef SDL_threadID       cute_thread_id_t;
typedef SDL_ThreadFunction cute_thread_func_t;
#endif

/**
 * Creates an unlocked mutex.
 */
cute_mutex_t* cute_mutex_create();

/**
 * Returns 1 on success, zero otherwise.
 */
int cute_lock(cute_mutex_t* mutex);

/**
 * Returns 1 on success, zero otherwise.
 */
int cute_unlock(cute_mutex_t* mutex);

/**
 * Attempts to lock the mutex without blocking. Returns one if lock was acquired,
 * otherwise returns zero.
 */
int cute_trylock(cute_mutex_t* mutex);
void cute_mutex_destroy(cute_mutex_t* mutex);

/**
 * Constructs a condition variable, used to sleep or wake threads.
 */
cute_cv_t* cute_cv_create();

/**
 * Signals all sleeping threads to wake that are waiting on the condition variable.
 * Returns 1 on success, zero otherwise.
 */
int cute_cv_wake_all(cute_cv_t* cv);

/**
 * Signals a single thread to wake that are waiting on the condition variable.
 * Returns 1 on success, zero otherwise.
 */
int cute_cv_wake_one(cute_cv_t* cv);

/**
 * Places a thread to wait on the condition variable.
 * Returns 1 on success, zero otherwise.
 */
int cute_cv_wait(cute_cv_t* cv, cute_mutex_t* mutex);
void cute_cv_destroy(cute_cv_t* cv);

/**
 * Creates a semaphore with an initial internal value of `initial_count`.
 * Returns NULL on failure.
 */
cute_sem_t* cute_sem_create(unsigned initial_count);

/**
 * Automically increments the semaphore's value and then wakes a sleeping thread.
 * Returns 1 on success, zero otherwise.
 */
int cute_sem_post(cute_sem_t* semaphore);

/**
 * Non-blocking version of `cute_sem_wait`.
 * Returns 1 on success, zero otherwise.
 */
int cute_sem_try(cute_sem_t* semaphore);

/**
 * Suspends the calling thread's execution unless the semaphore's value is positive. Will
 * decrement the value atomically afterwards.
 * Returns 1 on success, zero otherwise.
 */
int cute_sem_wait(cute_sem_t* semaphore);
int cute_sem_value(cute_sem_t* semaphore);
void cute_sem_destroy(cute_sem_t* semaphore);

cute_thread_t* cute_thread_create(cute_thread_func_t func, const char* name, void* udata);

/**
 * An optimization, meaning the thread will never have `cute_thread_wait` called on it.
 * Useful for certain long-lived threads.
 * It is invalid to call `cute_thread_wait` on a detached thread.
 * It is invalid to call `cute_thread_wait` on a thread more than once.
 * Please see this link for a longer description: https://wiki.libsdl.org/SDL_DetachThread
 */
void cute_thread_detach(cute_thread_t* thread);
cute_thread_id_t cute_thread_get_id(cute_thread_t* thread);
cute_thread_id_t cute_thread_id();

/**
 * Returns the number of CPU cores on the machine. Can be affected my machine dependent technology,
 * such as Intel's hyperthreading.
 */
int cute_core_count();

/**
 * Returns the size of CPU's L1's cache line size in bytes.
 */
int cute_cacheline_size();

/**
 * Waits until the thread exits (unless it has already exited), and returns the thread's
 * return code. Unless the thread was detached, this function must be used, otherwise it
 * is considered a leak to leave an thread hanging around (even if it finished execution
 * and returned).
 */
int cute_thread_wait(cute_thread_t* thread);

/**
 * Atomically adds `addend` at `address` and returns the old value at `address`.
 */
int cute_atomic_add(int* address, int addend);

/**
 * Atomically sets `value` at `address` and returns the old value at `address`.
 */
int cute_atomic_set(int* address, int value);

/**
 * Atomically fetches the value at `address`.
 */
int cute_atomic_get(int* address);

/**
 * Atomically sets `address` to `value` if compare equals `address`.
 * Returns 1 of the value was set, 0 otherwise.
 */
int cute_atomic_cas(int* address, int compare, int value);

/**
 * Atomically sets `value` at `address` and returns the old value at `address`.
 */
void* cute_atomic_ptr_set(void** address, void* value);

/**
 * Atomically fetches the value at `address`.
 */
void* cute_atomic_ptr_get(void** address);

/**
 * Atomically sets `address` to `value` if compare equals `address`.
 * Returns 1 of the value was set, 0 otherwise.
 */
int cute_atomic_ptr_cas(void** address, void* compare, void* value);

#define CUTE_RW_LOCK_MAX_READERS (1 << 30)

/**
 * A reader/writer mutual exclusion lock. Allows many simultaneous readers or a single writer.
 *
 * The number of readers is capped by `CUTE_RW_LOCK_MAX_READERS` (or in other words, a nearly indefinite
 * number). Exceeding `CUTE_RW_LOCK_MAX_READERS` simultaneous readers results in undefined behavior.
 */
typedef struct cute_rw_lock_t
{
	cute_mutex_t* mutex;
	cute_sem_t* write_sem;
	cute_sem_t* read_sem;
	int readers;
	int readers_departing;
} cute_rw_lock_t;

/**
 * Constructs an unlocked mutual exclusion read/write lock. The `rw` lock can safely sit
 * on the stack.
 */
void cute_rw_lock_create(cute_rw_lock_t* rw);

/**
 * Locks for reading. Many simultaneous readers are allowed.
 */
void cute_read_lock(cute_rw_lock_t* rw);

/**
 * Undoes a single call to `cute_read_lock`.
 */
void cute_read_unlock(cute_rw_lock_t* rw);

/**
 * Locks for writing. When locked for writing, only one writer can be present, and no readers.
 *
 * Will wait for active readers to call `cute_read_unlock`, or for active writers to call
 * `cute_write_unlock`.
 */
void cute_write_lock(cute_rw_lock_t* rw);

/**
 * Undoes a single call to `cute_write_lock`.
 */
void cute_write_unlock(cute_rw_lock_t* rw);

/**
 * Destroys the internal semaphores, and mutex.
 */
void cute_rw_lock_destroy(cute_rw_lock_t* rw);

typedef struct cute_threadpool_t cute_threadpool_t;

/**
 * Constructs a threadpool containing `thread_count`, useful for implementing job/task systems.
 * `mem_ctx` can be NULL, and is used for custom allocation purposes.
 *
 * Returns NULL on error. Will return NULL if `CUTE_THREAD_CACHELINE_SIZE` is less than the actual
 * cache line size on a given machine. `CUTE_THREAD_CACHELINE_SIZE` defaults to 128 bytes, and can
 * be overidden by defining CUTE_THREAD_CACHELINE_SIZE before including cute_sync.h
 *
 * Makes a modest attempt at memory aligning to avoid false sharing, as an optimization.
 */
cute_threadpool_t* cute_threadpool_create(int thread_count, void* mem_ctx);

/**
 * Atomically adds a single task to the internal task stack (FIFO order). The task is represented
 * as a function pointer `func`, which does work. The `param` is passed to the `func` when the
 * task is started.
 */
void cute_threadpool_add_task(cute_threadpool_t* pool, void (*func)(void*), void* param);

/**
 * Wakes internal threads to perform tasks, and waits for all tasks to complete before returning.
 * The calling thread will help perform available tasks while waiting.
 */
void cute_threadpool_kick_and_wait(cute_threadpool_t* pool);

/**
 * Wakes internal threads to perform tasks and immediately returns.
 */
void cute_threadpool_kick(cute_threadpool_t* pool);

/**
 * Cleans up all resources created from `cute_threadpool_create`.
 */
void cute_threadpool_destroy(cute_threadpool_t* pool);

#define CUTE_SYNC_H
#endif

//--------------------------------------------------------------------------------------------------

#if defined(CUTE_SYNC_IMPLEMENTATION)
#if !defined(CUTE_SYNC_IMPLEMENTATION_ONCE)
#define CUTE_SYNC_IMPLEMENTATION_ONCE

#if defined(CUTE_SYNC_SDL)
cute_mutex_t* cute_mutex_create()
{
	return SDL_CreateMutex();
}

int cute_lock(cute_mutex_t* mutex)
{
	return !SDL_LockMutex(mutex);
}

int cute_unlock(cute_mutex_t* mutex)
{
	return !SDL_UnlockMutex(mutex);
}

int cute_trylock(cute_mutex_t* mutex)
{
	return !SDL_TryLockMutex(mutex);
}

void cute_mutex_destroy(cute_mutex_t* mutex)
{
	SDL_DestroyMutex(mutex);
}

cute_cv_t* cute_cv_create()
{
	return SDL_CreateCond();
}

int cute_cv_wake_all(cute_cv_t* cv)
{
	return !SDL_CondBroadcast(cv);
}

int cute_cv_wake_one(cute_cv_t* cv)
{
	return !SDL_CondSignal(cv);
}

int cute_cv_wait(cute_cv_t* cv, cute_mutex_t* mutex)
{
	return !SDL_CondWait(cv, mutex);
}

void cute_cv_destroy(cute_cv_t* cv)
{
	SDL_DestroyCond(cv);
}

cute_sem_t* cute_sem_create(unsigned initial_count)
{
	return SDL_CreateSemaphore(initial_count);
}

int cute_sem_post(cute_sem_t* semaphore)
{
	return !SDL_SemPost(semaphore);
}

int cute_sem_try(cute_sem_t* semaphore)
{
	return !SDL_SemTryWait(semaphore);
}

int cute_sem_wait(cute_sem_t* semaphore)
{
	return !SDL_SemWait(semaphore);
}

int cute_sem_value(cute_sem_t* semaphore)
{
	return SDL_SemValue(semaphore);
}

void cute_sem_destroy(cute_sem_t* semaphore)
{
	SDL_DestroySemaphore(semaphore);
}

cute_thread_t* cute_thread_create(cute_thread_func_t func, const char* name, void* udata)
{
	return SDL_CreateThread(func, name, udata);
}

void cute_thread_detach(cute_thread_t* thread)
{
	SDL_DetachThread(thread);
}

cute_thread_id_t cute_thread_get_id(cute_thread_t* thread)
{
	return SDL_GetThreadID(thread);
}

cute_thread_id_t cute_thread_id()
{
	return SDL_ThreadID();
}

int cute_core_count()
{
	return SDL_GetCPUCount();
}

int cute_cacheline_size()
{
	return SDL_GetCPUCacheLineSize();
}
#endif

// Make sure a valid implementation exists.
#if !defined(CUTE_SYNC_SDL)
	#error An implementation of cute_sync primitives was not found. Did you forget to define CUTE_SYNC_SDL?
#endif

int cute_thread_wait(cute_thread_t* thread)
{
	int ret;
	SDL_WaitThread(thread, &ret);
	return ret;
}

int cute_atomic_add(int* address, int addend)
{
	return SDL_AtomicAdd((SDL_atomic_t*)address, addend);
}

int cute_atomic_set(int* address, int value)
{
	return SDL_AtomicSet((SDL_atomic_t*)address, value);
}

int cute_atomic_get(int* address)
{
	return SDL_AtomicGet((SDL_atomic_t*)address);
}

int cute_atomic_cas(int* address, int compare, int value)
{
	return SDL_AtomicCAS((SDL_atomic_t*)address, compare, value);
}

void* cute_atomic_ptr_set(void** address, void* value)
{
	return SDL_AtomicSetPtr(address, value);
}

void* cute_atomic_ptr_get(void** address)
{
	return SDL_AtomicGetPtr(address);
}

int cute_atomic_ptr_cas(void** address, void* compare, void* value)
{
	return SDL_AtomicCASPtr(address, compare, value);
}

void cute_rw_lock_create(cute_rw_lock_t* rw)
{
	rw->mutex = cute_mutex_create();
	rw->write_sem = cute_sem_create(0);
	rw->read_sem = cute_sem_create(0);
	rw->readers = 0;
	rw->readers_departing = 0;
}

void cute_read_lock(cute_rw_lock_t* rw)
{
	// Wait on writers.
	// Negative means locked for writing, or there is a pending writer.
	if (cute_atomic_add(&rw->readers, 1) < 0)
	{
		cute_sem_wait(rw->read_sem);
	}
}

void cute_read_unlock(cute_rw_lock_t* rw)
{
	// Write is pending.
	if (cute_atomic_add(&rw->readers, -1) < 0)
	{
		// The final departing reader notifies the pending writer.
		if (cute_atomic_add(&rw->readers_departing, -1) - 1 == 0)
		{
			cute_sem_post(rw->write_sem);
		}
	}
}

void cute_write_lock(cute_rw_lock_t* rw)
{
	cute_lock(rw->mutex);

	// Flip to negative to force new readers to wait. Record the number of active
	// readers at that moment, which need to depart to allow write access.
	int readers = cute_atomic_add(&rw->readers, -CUTE_RW_LOCK_MAX_READERS);

	// Wait for departing readers.
	if (cute_atomic_add(&rw->readers_departing, readers) + readers != 0)
	{
		cute_sem_wait(rw->write_sem);
	}
}

void cute_write_unlock(cute_rw_lock_t* rw)
{
	// Flip to positive to allow new readers. Record the number of waiting readers
	// at that moment.
	int readers = cute_atomic_add(&rw->readers, CUTE_RW_LOCK_MAX_READERS) + CUTE_RW_LOCK_MAX_READERS;

	// Signal all waiting readers to wake.
	for (int i = 0; i < readers; ++i)
	{
		cute_sem_post(rw->read_sem);
	}

	cute_unlock(rw->mutex);
}

void cute_rw_lock_destroy(cute_rw_lock_t* rw)
{
	cute_mutex_destroy(rw->mutex);
	cute_sem_destroy(rw->write_sem);
	cute_sem_destroy(rw->read_sem);
}

#if !defined(CUTE_THREAD_ALLOC)
	#include <stdlib.h>
	#define CUTE_THREAD_ALLOC(size, ctx) malloc(size)
	#define CUTE_THREAD_FREE(ptr, ctx) free(ptr)
#endif

#if !defined(CUTE_THREAD_MEMCPY)
	#include <string.h>
	#define CUTE_THREAD_MEMCPY memcpy
#endif

#if !defined(CUTE_THREAD_ALIGN)
	#ifdef _MSC_VER
		#define CUTE_THREAD_ALIGN(X) __declspec(align(X))
	#else
		#define CUTE_THREAD_ALIGN(X) __attribute__((aligned(X)))
	#endif
	// Add your own platform here.
#endif

#if !defined(CUTE_THREAD_YIELD)
	#ifdef _MSC_VER
		#include <winnt.h>
		#define CUTE_THREAD_YIELD YieldProcessor
	#else
		#include <sched.h>
		#define CUTE_THREAD_YIELD sched_yield
	#endif
#endif

#if !defined(CUTE_THREAD_CACHELINE_SIZE)
	// Sized generously to try and avoid guessing "too low". Too small would incur serious overhead
	// inside of `cute_threadpool_t` as false sharing would run amok between pooled threads.
	#define CUTE_THREAD_CACHELINE_SIZE 128
#endif

#define CUTE_THREAD_ALIGN_PTR(X, Y) ((((size_t)X) + ((Y) - 1)) & ~((Y) - 1))

static void* cute_malloc_aligned(size_t size, int alignment, void* mem_ctx)
{
	void* p = CUTE_THREAD_ALLOC(size + alignment, mem_ctx);
	if (!p) return 0;
	unsigned char offset = (size_t)p & (alignment - 1);
	p = (void*)CUTE_THREAD_ALIGN_PTR(p + 1, alignment);
	*((char*)p - 1) = alignment - offset;
	return p;
}

static void cute_free_aligned(void* p, void* mem_ctx)
{
	if (!p) return;
	size_t alignment = (size_t)*((char*)p - 1) & 0xFF;
	CUTE_THREAD_FREE((char*)p - alignment, mem_ctx);
}

typedef struct cute_task_t
{
	void (*do_work)(void*);
	void* param;
} cute_task_t;

typedef struct CUTE_THREAD_ALIGN(CUTE_THREAD_CACHELINE_SIZE) cute_aligned_thread_t
{
	cute_thread_t* thread;
} cute_aligned_thread_t;

struct cute_threadpool_t
{
	int task_capacity;
	int task_count;
	cute_task_t* tasks;
	cute_mutex_t* task_mutex;

	int thread_count;
	cute_aligned_thread_t* threads;

	int running;
	cute_mutex_t* sem_mutex;
	cute_sem_t* semaphore;
	void* mem_ctx;
};

int cute_try_pop_task_internal(cute_threadpool_t* pool, cute_task_t* task)
{
	cute_lock(pool->task_mutex);

	if (pool->task_count)
	{
		*task = pool->tasks[--pool->task_count];
		cute_unlock(pool->task_mutex);
		return 1;
	}

	cute_unlock(pool->task_mutex);
	return 0;
}

int cute_worker_thread_internal(void* udata)
{
	cute_threadpool_t* pool = (cute_threadpool_t*)udata;
	while (pool->running)
	{
		cute_task_t task;
		if (cute_try_pop_task_internal(pool, &task))
		{
			task.do_work(task.param);
		}

		cute_sem_wait(pool->semaphore);
	}
	return 0;
}

cute_threadpool_t* cute_threadpool_create(int thread_count, void* mem_ctx)
{
	if (CUTE_THREAD_CACHELINE_SIZE < cute_cacheline_size()) return 0;

	cute_threadpool_t* pool = (cute_threadpool_t*)CUTE_THREAD_ALLOC(sizeof(cute_threadpool_t), mem_ctx);
	pool->task_capacity = 64;
	pool->task_count = 0;
	pool->tasks = (cute_task_t*)cute_malloc_aligned(sizeof(cute_task_t) * pool->task_capacity, CUTE_THREAD_CACHELINE_SIZE, mem_ctx);
	pool->task_mutex = cute_mutex_create();
	pool->thread_count = thread_count;
	pool->threads = (cute_aligned_thread_t*)cute_malloc_aligned(sizeof(cute_aligned_thread_t) * thread_count, CUTE_THREAD_CACHELINE_SIZE, mem_ctx);
	pool->running = 1;
	pool->sem_mutex = cute_mutex_create();
	pool->semaphore = cute_sem_create(0);
	pool->mem_ctx = mem_ctx;

	for (int i = 0; i < thread_count; ++i)
	{
		pool->threads[i].thread = cute_thread_create(cute_worker_thread_internal, 0, pool);
	}

	return pool;
}

void cute_threadpool_add_task(cute_threadpool_t* pool, void (*func)(void*), void* param)
{
	cute_lock(pool->task_mutex);

	if (pool->task_count == pool->task_capacity)
	{
		int new_cap = pool->task_capacity * 2;
		cute_task_t* new_tasks = (cute_task_t*)cute_malloc_aligned(sizeof(cute_task_t) * new_cap, CUTE_THREAD_CACHELINE_SIZE, pool->mem_ctx);
		CUTE_THREAD_MEMCPY(new_tasks, pool->tasks, sizeof(cute_task_t) * pool->task_count);
		cute_free_aligned(pool->tasks, pool->mem_ctx);
		pool->task_capacity = new_cap;
		pool->tasks = new_tasks;
	}

	cute_task_t task;
	task.do_work = func;
	task.param = param;
	pool->tasks[pool->task_count++] = task;

	cute_unlock(pool->task_mutex);
}

void cute_threadpool_kick_and_wait(cute_threadpool_t* pool)
{
	cute_threadpool_kick(pool);

	while (pool->task_count)
	{
		cute_task_t task;
		if (cute_try_pop_task_internal(pool, &task))
		{
			cute_sem_try(pool->semaphore);
			task.do_work(task.param);
		}
		CUTE_THREAD_YIELD();
	}
}

void cute_threadpool_kick(cute_threadpool_t* pool)
{
	if (pool->task_count)
	{
		int count = pool->task_count < pool->thread_count ? pool->task_count : pool->thread_count;
		for (int i = 0; i < count; ++i)
		{
			cute_sem_post(pool->semaphore);
		}
	}
}

void cute_threadpool_destroy(cute_threadpool_t* pool)
{
	pool->running = 0;

	for (int i = 0; i < pool->thread_count; ++i)
	{
		cute_sem_post(pool->semaphore);
	}

	for (int i = 0; i < pool->thread_count; ++i)
	{
		cute_thread_wait(pool->threads[i].thread);
	}

	cute_free_aligned(pool->tasks, pool->mem_ctx);
	cute_free_aligned(pool->threads, pool->mem_ctx);
	void* mem_ctx = pool->mem_ctx;
	CUTE_THREAD_FREE(pool, mem_ctx);
}

#endif // CUTE_SYNC_IMPLEMENTATION_ONCE
#endif // CUTE_SYNC_IMPLEMENTATION

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
