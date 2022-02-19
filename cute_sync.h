/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_sync.h - v1.01

	To create implementation (the function definitions)
		#define CUTE_SYNC_IMPLEMENTATION
		#define CUTE_SYNC_WINDOWS
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY

		Collection of practical syncronization primitives for Windows/Posix/SDL2.

		Here is a list of all supported primitives.

			* atomic integer/pointer
			* thread
			* mutex
			* condition variable
			* semaphore
			* read/write lock
			* thread pool

		Here are some slides I wrote for those interested in learning prequisite
		knowledge for utilizing this header:
		http://www.randygaul.net/2014/09/24/multi-threading-best-practices-for-gamedev/

		A good chunk of this code came from Mattias Gustavsson's thread.h header.
		It really is quite a good header, and worth considering!
		https://github.com/mattiasgustavsson/libs


	PLATFORMS

		The current supported platforms are Windows/Posix/SDL. Here are the macros for
		picking each implementation.

			* CUTE_SYNC_WINDOWS
			* CUTE_SYNC_POSIX
			* CUTE_SYNC_SDL


	REVISION HISTORY

		1.0  (05/31/2018) initial release
		1.01 (08/25/2019) Windows and pthreads port
*/

#if !defined(CUTE_SYNC_H)

typedef union cute_atomic_int_t cute_atomic_int_t;
typedef union cute_mutex_t cute_mutex_t;
typedef union cute_cv_t cute_cv_t;
typedef struct cute_semaphore_t cute_semaphore_t;
typedef struct cute_thread_t cute_thread_t;
typedef unsigned long long cute_thread_id_t;
typedef int (cute_thread_fn)(void *udata);

/**
 * Creates an unlocked mutex.
 */
cute_mutex_t cute_mutex_create();

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
cute_cv_t cute_cv_create();

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
cute_semaphore_t cute_semaphore_create(int initial_count);

/**
 * Automically increments the semaphore's value and then wakes a sleeping thread.
 * Returns 1 on success, zero otherwise.
 */
int cute_semaphore_post(cute_semaphore_t* semaphore);

/**
 * Non-blocking version of `cute_semaphore_wait`.
 * Returns 1 on success, zero otherwise.
 */
int cute_semaphore_try(cute_semaphore_t* semaphore);

/**
 * Suspends the calling thread's execution unless the semaphore's value is positive. Will
 * decrement the value atomically afterwards.
 * Returns 1 on success, zero otherwise.
 */
int cute_semaphore_wait(cute_semaphore_t* semaphore);
int cute_semaphore_value(cute_semaphore_t* semaphore);
void cute_semaphore_destroy(cute_semaphore_t* semaphore);

cute_thread_t* cute_thread_create(cute_thread_fn func, const char* name, void* udata);

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
 * Waits until the thread exits (unless it has already exited), and returns the thread's
 * return code. Unless the thread was detached, this function must be used, otherwise it
 * is considered a leak to leave an thread hanging around (even if it finished execution
 * and returned).
 */
int cute_thread_wait(cute_thread_t* thread);

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
 * Returns the size of the machine's RAM in megabytes.
 */
int cute_ram_size();

/**
 * Atomically adds `addend` at `atomic` and returns the old value at `atomic`.
 */
int cute_atomic_add(cute_atomic_int_t* atomic, int addend);

/**
 * Atomically sets `value` at `atomic` and returns the old value at `atomic`.
 */
int cute_atomic_set(cute_atomic_int_t* atomic, int value);

/**
 * Atomically fetches the value at `atomic`.
 */
int cute_atomic_get(cute_atomic_int_t* atomic);

/**
 * Atomically sets `atomic` to `value` if `expected` equals `atomic`.
 * Returns 1 of the value was set, 0 otherwise.
 */
int cute_atomic_cas(cute_atomic_int_t* atomic, int expected, int value);

/**
 * Atomically sets `value` at `atomic` and returns the old value at `atomic`.
 */
void* cute_atomic_ptr_set(void** atomic, void* value);

/**
 * Atomically fetches the value at `atomic`.
 */
void* cute_atomic_ptr_get(void** atomic);

/**
 * Atomically sets `atomic` to `value` if `expected` equals `atomic`.
 * Returns 1 of the value was set, 0 otherwise.
 */
int cute_atomic_ptr_cas(void** atomic, void* expected, void* value);

/**
 * A reader/writer mutual exclusion lock. Allows many simultaneous readers or a single writer.
 *
 * The number of readers is capped by `CUTE_RW_LOCK_MAX_READERS` (or in other words, a nearly indefinite
 * number). Exceeding `CUTE_RW_LOCK_MAX_READERS` simultaneous readers results in undefined behavior.
 */
typedef struct cute_rw_lock_t cute_rw_lock_t;
#define CUTE_RW_LOCK_MAX_READERS (1 << 30)

/**
 * Constructs an unlocked mutual exclusion read/write lock. The `rw` lock can safely sit
 * on the stack.
 */
cute_rw_lock_t cute_rw_lock_create();

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
 * Returns NULL on error. Will return NULL if `CUTE_SYNC_CACHELINE_SIZE` is less than the actual
 * cache line size on a given machine. `CUTE_SYNC_CACHELINE_SIZE` defaults to 128 bytes, and can
 * be overidden by defining CUTE_SYNC_CACHELINE_SIZE before including cute_sync.h
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

#ifndef CUTE_SYNC_TYPE_DEFINITIONS_H

union  cute_atomic_int_t { void* align; long i;                  };
union  cute_mutex_t      { void* align; char data[64];           };
union  cute_cv_t         { void* align; char data[64];           };
struct cute_semaphore_t  { void* id;    cute_atomic_int_t count; };

struct cute_rw_lock_t
{
	cute_mutex_t mutex;
	cute_semaphore_t write_sem;
	cute_semaphore_t read_sem;
	cute_atomic_int_t readers;
	cute_atomic_int_t readers_departing;
};

#define CUTE_SYNC_TYPE_DEFINITIONS_H
#endif

//--------------------------------------------------------------------------------------------------

#if defined(CUTE_SYNC_IMPLEMENTATION)
#if !defined(CUTE_SYNC_IMPLEMENTATION_ONCE)
#define CUTE_SYNC_IMPLEMENTATION_ONCE

#if defined(CUTE_SYNC_SDL)
#elif defined(CUTE_SYNC_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	// To use GetThreadId and other methods we must require Windows Vista minimum.
	#if _WIN32_WINNT < 0x0600
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0600 // requires Windows Vista minimum
		// 0x0400=Windows NT 4.0, 0x0500=Windows 2000, 0x0501=Windows XP, 0x0502=Windows Server 2003, 0x0600=Windows Vista,
		// 0x0601=Windows 7, 0x0602=Windows 8, 0x0603=Windows 8.1, 0x0A00=Windows 10
	#endif
	#include <Windows.h>
#elif defined(CUTE_SYNC_POSIX)
	#include <pthread.h>
	#include <semaphore.h>

	// Just platforms with unistd.h are supported for now.
	// So no FreeBSD, OS/2, or other weird platforms.
	#include <unistd.h> // sysconf

	#if defined(__APPLE__)
	#include <sys/sysctl.h> // sysctlbyname
	#endif
#else
	#error Please choose a base implementation between CUTE_SYNC_SDL, CUTE_SYNC_WINDOWS and CUTE_SYNC_POSIX.
#endif

#if !defined(CUTE_SYNC_ALLOC)
	#include <stdlib.h>
	#define CUTE_SYNC_ALLOC(size, ctx) malloc(size)
	#define CUTE_SYNC_FREE(ptr, ctx) free(ptr)
#endif

#if !defined(CUTE_SYNC_MEMCPY)
	#include <string.h>
	#define CUTE_SYNC_MEMCPY memcpy
#endif

#if !defined(CUTE_SYNC_YIELD)
	#ifdef CUTE_SYNC_WINDOWS
		#define WIN32_LEAN_AND_MEAN
		#include <Windows.h> // winnt
		#define CUTE_SYNC_YIELD YieldProcessor
	#elif defined(CUTE_SYNC_POSIX)
		#include <sched.h>
		#define CUTE_SYNC_YIELD sched_yield
	#else
		#define CUTE_SYNC_YIELD() // Not implemented by SDL.
	#endif
#endif

#if !defined(CUTE_SYNC_ASSERT)
	#include <assert.h>
	#define CUTE_SYNC_ASSERT assert
#endif

#if !defined(CUTE_SYNC_CACHELINE_SIZE)
	// Sized generously to try and avoid guessing "too low". Too small would incur serious overhead
	// inside of `cute_threadpool_t` as false sharing would run amok between pooled threads.
	#define CUTE_SYNC_CACHELINE_SIZE 128
#endif

// Atomics implementation.
// Use SDL2's implementation if available, otherwise WIN32 and GCC-like compilers are supported out-of-the-box.
#ifdef CUTE_SYNC_SDL

int cute_atomic_add(cute_atomic_int_t* atomic, int addend)
{
	return SDL_AtomicAdd((SDL_atomic_t*)atomic, addend);
}

int cute_atomic_set(cute_atomic_int_t* atomic, int value)
{
	return SDL_AtomicSet((SDL_atomic_t*)atomic, value);
}

int cute_atomic_get(cute_atomic_int_t* atomic)
{
	return SDL_AtomicGet((SDL_atomic_t*)atomic);
}

int cute_atomic_cas(cute_atomic_int_t* atomic, int expected, int value)
{
	return SDL_AtomicCAS((SDL_atomic_t*)atomic, expected, value);
}

void* cute_atomic_ptr_set(void** atomic, void* value)
{
	return SDL_AtomicSetPtr(atomic, value);
}

void* cute_atomic_ptr_get(void** atomic)
{
	return SDL_AtomicGetPtr(atomic);
}

int cute_atomic_ptr_cas(void** atomic, void* expected, void* value)
{
	return SDL_AtomicCASPtr(atomic, expected, value);
}

#elif defined(CUTE_SYNC_WINDOWS)

int cute_atomic_add(cute_atomic_int_t* atomic, int addend)
{
	return (int)_InterlockedExchangeAdd(&atomic->i, (LONG)addend);
}

int cute_atomic_set(cute_atomic_int_t* atomic, int value)
{
	return (int)_InterlockedExchange(&atomic->i, value);
}

int cute_atomic_get(cute_atomic_int_t* atomic)
{
	return (int)_InterlockedCompareExchange(&atomic->i, 0, 0);
}

int cute_atomic_cas(cute_atomic_int_t* atomic, int expected, int value)
{
	return (int)_InterlockedCompareExchange(&atomic->i, expected, value) == value;
}

void* cute_atomic_ptr_set(void** atomic, void* value)
{
	return _InterlockedExchangePointer(atomic, value);
}

void* cute_atomic_ptr_get(void** atomic)
{
	return _InterlockedCompareExchangePointer(atomic, NULL, NULL);
}

int cute_atomic_ptr_cas(void** atomic, void* expected, void* value)
{
	return _InterlockedCompareExchangePointer(atomic, expected, value) == value;
}

#elif defined(CUTE_SYNC_POSIX)

#if !(defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__))
#	error Unsupported platform found - no atomics implementation available for this compiler.
#	error The section only implements GCC atomics.
#endif

int cute_atomic_add(cute_atomic_int_t* atomic, int addend)
{
	return (int)__sync_fetch_and_add(&atomic->i, addend);
}

int cute_atomic_set(cute_atomic_int_t* atomic, int value)
{
	int result = (int)__sync_lock_test_and_set(&atomic->i, value);
	__sync_lock_release(&atomic->i);
	return result;
}

int cute_atomic_get(cute_atomic_int_t* atomic)
{
	return (int)__sync_fetch_and_add(&atomic->i, 0);
}

int cute_atomic_cas(cute_atomic_int_t* atomic, int expected, int value)
{
	return (int)__sync_val_compare_and_swap(&atomic->i, expected, value) == value;
}

void* cute_atomic_ptr_set(void** atomic, void* value)
{
	void* result = __sync_lock_test_and_set(atomic, value);
	__sync_lock_release(atomic);
	return result;
}

void* cute_atomic_ptr_get(void** atomic)
{
	return __sync_fetch_and_add(atomic, NULL);
}

int cute_atomic_ptr_cas(void** atomic, void* expected, void* value)
{
	return __sync_val_compare_and_swap(atomic, expected, value) == value;
}

#endif // End atomics implementation.

#if defined(CUTE_SYNC_SDL)

cute_mutex_t cute_mutex_create()
{
	cute_mutex_t mutex;
	mutex.align = SDL_CreateMutex();
	return mutex;
}

int cute_lock(cute_mutex_t* mutex)
{
	return !SDL_LockMutex((SDL_mutex*)mutex->align);
}

int cute_unlock(cute_mutex_t* mutex)
{
	return !SDL_UnlockMutex((SDL_mutex*)mutex->align);
}

int cute_trylock(cute_mutex_t* mutex)
{
	return !SDL_TryLockMutex((SDL_mutex*)mutex->align);
}

void cute_mutex_destroy(cute_mutex_t* mutex)
{
	SDL_DestroyMutex((SDL_mutex*)mutex->align);
}

cute_cv_t cute_cv_create()
{
	cute_cv_t cv;
	cv.align = SDL_CreateCond();
	return cv;
}

int cute_cv_wake_all(cute_cv_t* cv)
{
	return !SDL_CondBroadcast((SDL_cond*)cv->align);
}

int cute_cv_wake_one(cute_cv_t* cv)
{
	return !SDL_CondSignal((SDL_cond*)cv->align);
}

int cute_cv_wait(cute_cv_t* cv, cute_mutex_t* mutex)
{
	return !SDL_CondWait((SDL_cond*)cv, (SDL_mutex*)mutex->align);
}

void cute_cv_destroy(cute_cv_t* cv)
{
	SDL_DestroyCond((SDL_cond*)cv->align);
}

cute_semaphore_t cute_semaphore_create(int initial_count)
{
	cute_semaphore_t semaphore;
	semaphore.id = SDL_CreateSemaphore(initial_count);
	semaphore.count.i = initial_count;
	return semaphore;
}

int cute_semaphore_post(cute_semaphore_t* semaphore)
{
	return !SDL_SemPost((SDL_sem*)semaphore->id);
}

int cute_semaphore_try(cute_semaphore_t* semaphore)
{
	return !SDL_SemTryWait((SDL_sem*)semaphore->id);
}

int cute_semaphore_wait(cute_semaphore_t* semaphore)
{
	return !SDL_SemWait((SDL_sem*)semaphore->id);
}

int cute_semaphore_value(cute_semaphore_t* semaphore)
{
	return SDL_SemValue((SDL_sem*)semaphore->id);
}

void cute_semaphore_destroy(cute_semaphore_t* semaphore)
{
	SDL_DestroySemaphore((SDL_sem*)semaphore->id);
}

cute_thread_t* cute_thread_create(cute_thread_fn func, const char* name, void* udata)
{
	return (cute_thread_t*)SDL_CreateThread(func, name, udata);
}

void cute_thread_detach(cute_thread_t* thread)
{
	SDL_DetachThread((SDL_Thread*)thread);
}

cute_thread_id_t cute_thread_get_id(cute_thread_t* thread)
{
	return SDL_GetThreadID((SDL_Thread*)thread);
}

cute_thread_id_t cute_thread_id()
{
	return SDL_ThreadID();
}

int cute_thread_wait(cute_thread_t* thread)
{
	int ret;
	SDL_WaitThread((SDL_Thread*)thread, &ret);
	return ret;
}

int cute_core_count()
{
	return SDL_GetCPUCount();
}

int cute_cacheline_size()
{
	return SDL_GetCPUCacheLineSize();
}

int cute_ram_size()
{
	return SDL_GetSystemRAM();
}

#elif defined(CUTE_SYNC_WINDOWS)

cute_mutex_t cute_mutex_create()
{
	CUTE_SYNC_ASSERT(sizeof(CRITICAL_SECTION) <= sizeof(cute_mutex_t));
	cute_mutex_t mutex;
	InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)&mutex, 2000);
	return mutex;
}

int cute_lock(cute_mutex_t* mutex)
{
	EnterCriticalSection((CRITICAL_SECTION*)mutex);
	return 1;
}

int cute_unlock(cute_mutex_t* mutex)
{
	LeaveCriticalSection((CRITICAL_SECTION*)mutex);
	return 1;
}

int cute_trylock(cute_mutex_t* mutex)
{
	return !TryEnterCriticalSection((CRITICAL_SECTION*)mutex);
}

void cute_mutex_destroy(cute_mutex_t* mutex)
{
	DeleteCriticalSection((CRITICAL_SECTION*)mutex);
}

cute_cv_t cute_cv_create()
{
	CUTE_SYNC_ASSERT(sizeof(CONDITION_VARIABLE) <= sizeof(cute_cv_t));
	cute_cv_t cv;
	InitializeConditionVariable((CONDITION_VARIABLE*)&cv);
	return cv;
}

int cute_cv_wake_all(cute_cv_t* cv)
{
	WakeAllConditionVariable((CONDITION_VARIABLE*)cv);
	return 1;
}

int cute_cv_wake_one(cute_cv_t* cv)
{
	WakeConditionVariable((CONDITION_VARIABLE*)cv);
	return 1;
}

int cute_cv_wait(cute_cv_t* cv, cute_mutex_t* mutex)
{
	return !!SleepConditionVariableCS((CONDITION_VARIABLE*)cv, (CRITICAL_SECTION*)mutex, INFINITE);
}

void cute_cv_destroy(cute_cv_t* cv)
{
	// Nothing needed here on Windows... Weird!
	// https://stackoverflow.com/questions/28975958/why-does-windows-have-no-deleteconditionvariable-function-to-go-together-with
}

cute_semaphore_t cute_semaphore_create(int initial_count)
{
	cute_semaphore_t semaphore;
	semaphore.id = CreateSemaphoreA(NULL, (LONG)initial_count, LONG_MAX, NULL);
	semaphore.count.i = initial_count;
	return semaphore;
}

int cute_semaphore_post(cute_semaphore_t* semaphore)
{
	_InterlockedIncrement(&semaphore->count.i);
	if (ReleaseSemaphore(semaphore->id, 1, NULL) == FALSE) {
		_InterlockedDecrement(&semaphore->count.i);
		return 0;
	} else {
		return 1;
	}
}

static int s_wait(cute_semaphore_t* semaphore, DWORD milliseconds)
{
	if (WaitForSingleObjectEx(semaphore->id, milliseconds, FALSE) == WAIT_OBJECT_0) {
		return 1;
	} else {
		return 0;
	}
}

int cute_semaphore_try(cute_semaphore_t* semaphore)
{
	return s_wait(semaphore, 0);
}

int cute_semaphore_wait(cute_semaphore_t* semaphore)
{
	return s_wait(semaphore, INFINITE);
}

int cute_semaphore_value(cute_semaphore_t* semaphore)
{
	return cute_atomic_get(&semaphore->count);
}

void cute_semaphore_destroy(cute_semaphore_t* semaphore)
{
	CloseHandle((HANDLE)&semaphore->id);
}

cute_thread_t* cute_thread_create(cute_thread_fn fn, const char* name, void* udata)
{
	(void)name;
	DWORD unused;
	HANDLE id = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, udata, 0, &unused);
	return (cute_thread_t*)id;
}

void cute_thread_detach(cute_thread_t* thread)
{
	CloseHandle((HANDLE)thread);
}

cute_thread_id_t cute_thread_get_id(cute_thread_t* thread)
{
	return GetThreadId((HANDLE)thread);
}

cute_thread_id_t cute_thread_id()
{
	return GetCurrentThreadId();
}

int cute_thread_wait(cute_thread_t* thread)
{
	WaitForSingleObject((HANDLE)thread, INFINITE);
	CloseHandle((HANDLE)thread);
	return 1;
}

int cute_core_count()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return (int)info.dwNumberOfProcessors;
}

int cute_cacheline_size()
{
	DWORD buffer_size;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer[256];

	GetLogicalProcessorInformation(0, &buffer_size);
	DWORD buffer_count = buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
	if (buffer_count > 256) {
		// Just guess... Since this machine has more than 256 cores?
		// Supporting more than 256 cores would probably require a malloc here.
		return 128;
	}

	GetLogicalProcessorInformation(buffer, &buffer_size);

	for (DWORD i = 0; i < buffer_count; ++i) {
		if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1) {
			return (int)buffer[i].Cache.LineSize;
		}
	}

	// Just guess...
	return 128;
}

int cute_ram_size()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return (int)(status.ullTotalPhys / (1024 * 1024));
}

#elif defined(CUTE_SYNC_POSIX)

cute_mutex_t cute_mutex_create()
{
	CUTE_SYNC_ASSERT(sizeof(pthread_mutex_t) <= sizeof(cute_mutex_t));
	cute_mutex_t mutex;
	pthread_mutex_init((pthread_mutex_t*)&mutex, NULL);
	return mutex;
}

int cute_lock(cute_mutex_t* mutex)
{
	pthread_mutex_lock((pthread_mutex_t*)mutex);
	return 1;
}

int cute_unlock(cute_mutex_t* mutex)
{
	pthread_mutex_unlock((pthread_mutex_t*)mutex);
	return 1;
}

int cute_trylock(cute_mutex_t* mutex)
{
	return !pthread_mutex_trylock((pthread_mutex_t*)mutex);
}

void cute_mutex_destroy(cute_mutex_t* mutex)
{
	pthread_mutex_destroy((pthread_mutex_t*)mutex);
}

cute_cv_t cute_cv_create()
{
	CUTE_SYNC_ASSERT(sizeof(pthread_cond_t) <= sizeof(cute_cv_t));
	cute_cv_t cv;
	pthread_cond_init((pthread_cond_t*)&cv, NULL);
	return cv;
}

int cute_cv_wake_all(cute_cv_t* cv)
{
	pthread_cond_broadcast((pthread_cond_t*)cv);
	return 1;
}

int cute_cv_wake_one(cute_cv_t* cv)
{
	pthread_cond_signal((pthread_cond_t*)cv);
	return 1;
}

int cute_cv_wait(cute_cv_t* cv, cute_mutex_t* mutex)
{
	return !pthread_cond_wait((pthread_cond_t*)cv, (pthread_mutex_t*)mutex);
}

void cute_cv_destroy(cute_cv_t* cv)
{
	pthread_cond_destroy((pthread_cond_t*)cv);
}

#if !defined(__APPLE__)

cute_semaphore_t cute_semaphore_create(int initial_count)
{
	cute_semaphore_t semaphore;
	semaphore.id = CUTE_SYNC_ALLOC(sizeof(sem_t), NULL);
	sem_init((sem_t*)semaphore.id, 0, (unsigned)initial_count);
	semaphore.count.i = initial_count;
	return semaphore;
}

int cute_semaphore_post(cute_semaphore_t* semaphore)
{
	return !sem_post((sem_t*)semaphore->id);
}

int cute_semaphore_try(cute_semaphore_t* semaphore)
{
	return !sem_trywait((sem_t*)semaphore->id);
}

int cute_semaphore_wait(cute_semaphore_t* semaphore)
{
	return !sem_wait((sem_t*)semaphore->id);
}

int cute_semaphore_value(cute_semaphore_t* semaphore)
{
	int result = 0;
	sem_getvalue((sem_t*)semaphore->id, &result);
	return result;
}

void cute_semaphore_destroy(cute_semaphore_t* semaphore)
{
	sem_destroy((sem_t*)semaphore->id);
	CUTE_SYNC_FREE(semaphore->id, NULL);
}

#elif defined(__APPLE__)

// Because Apple sucks and deprecated posix semaphores we must make our own...

typedef struct cute_apple_sem_t
{
	int count;
	int waiting_count;
	cute_mutex_t lock;
	cute_cv_t cv;
} cute_apple_sem_t;

cute_semaphore_t cute_semaphore_create(int initial_count)
{
	cute_apple_sem_t* apple_sem = (cute_apple_sem_t*)CUTE_SYNC_ALLOC(sizeof(cute_apple_sem_t), NULL);
	apple_sem->count = initial_count;
	apple_sem->waiting_count = 0;
	apple_sem->lock = cute_mutex_create();
	apple_sem->cv = cute_cv_create();
	cute_semaphore_t semaphore;
	semaphore.id = (void*)apple_sem;
	semaphore.count.i = initial_count;
	return semaphore;
}

int cute_semaphore_post(cute_semaphore_t* semaphore)
{
	cute_apple_sem_t* apple_sem = (cute_apple_sem_t*)semaphore->id;
	cute_lock(&apple_sem->lock);
	if (apple_sem->waiting_count > 0) {
		cute_cv_wake_one(&apple_sem->cv);
	}
	apple_sem->count += 1;
	cute_unlock(&apple_sem->lock);
	return 1;
}

int cute_semaphore_try(cute_semaphore_t* semaphore)
{
	cute_apple_sem_t* apple_sem = (cute_apple_sem_t*)semaphore->id;
	int result = 0;
	cute_lock(&apple_sem->lock);
	if (apple_sem->count > 0) {
		apple_sem->count -= 1;
		result = 1;
	}
	cute_unlock(&apple_sem->lock);
	return result;
}

int cute_semaphore_wait(cute_semaphore_t* semaphore)
{
	cute_apple_sem_t* apple_sem = (cute_apple_sem_t*)semaphore->id;
	int result = 1;
	cute_lock(&apple_sem->lock);
	while (apple_sem->count == 0 && result) {
		result = cute_cv_wait(&apple_sem->cv, &apple_sem->lock);
	}
	apple_sem->waiting_count -= 1;
	if (result) {
		apple_sem->count -= 1;
	}
	cute_unlock(&apple_sem->lock);
	return result;
}

int cute_semaphore_value(cute_semaphore_t* semaphore)
{
	cute_apple_sem_t* apple_sem = (cute_apple_sem_t*)semaphore->id;
	int value;
	cute_lock(&apple_sem->lock);
	value = apple_sem->count;
	cute_unlock(&apple_sem->lock);
	return value;
}

void cute_semaphore_destroy(cute_semaphore_t* semaphore)
{
	cute_apple_sem_t* apple_sem = (cute_apple_sem_t*)semaphore->id;
	while (apple_sem->waiting_count > 0) {
		cute_cv_wake_all(&apple_sem->cv);
		CUTE_SYNC_YIELD();
	}
	cute_cv_destroy(&apple_sem->cv);
	cute_lock(&apple_sem->lock);
	cute_unlock(&apple_sem->lock);
	cute_mutex_destroy(&apple_sem->lock);
	CUTE_SYNC_FREE(apple_sem, NULL);
}

#endif

cute_thread_t* cute_thread_create(cute_thread_fn fn, const char* name, void* udata)
{
	pthread_t thread;
	pthread_create(&thread, NULL, (void* (*)(void*))fn, udata);
#if !defined(__APPLE__)
	if (name) pthread_setname_np(thread, name);
#else
	(void)name;
#endif
	return (cute_thread_t*)thread;
}

void cute_thread_detach(cute_thread_t* thread)
{
	pthread_detach((pthread_t)thread);
}

cute_thread_id_t cute_thread_get_id(cute_thread_t* thread)
{
	return (cute_thread_id_t)thread;
}

cute_thread_id_t cute_thread_id()
{
	return (cute_thread_id_t)pthread_self();
}

int cute_thread_wait(cute_thread_t* thread)
{
	pthread_join((pthread_t)thread, NULL);
	return 1;
}

int cute_core_count()
{
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
}

int cute_cacheline_size()
{
#if defined(__APPLE__)
	size_t sz;
	size_t szsz = sizeof(sz);
	sysctlbyname("hw.cachelinesize", &sz, &szsz, 0, 0);
	return (int)sz;
#else
	return (int)sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#endif
}

int cute_ram_size()
{
	return (int)(sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE) / (1024*1024));
}

#else

	#error Please choose a base implementation between CUTE_SYNC_SDL, CUTE_SYNC_WINDOWS and CUTE_SYNC_POSIX.

#endif

cute_rw_lock_t cute_rw_lock_create()
{
	cute_rw_lock_t rw;
	rw.mutex = cute_mutex_create();
	rw.write_sem = cute_semaphore_create(0);
	rw.read_sem = cute_semaphore_create(0);
	rw.readers.i = 0;
	rw.readers_departing.i = 0;
	return rw;
}

void cute_read_lock(cute_rw_lock_t* rw)
{
	// Wait on writers.
	// Negative means locked for writing, or there is a pending writer.
	if (cute_atomic_add(&rw->readers, 1) < 0) {
		cute_semaphore_wait(&rw->read_sem);
	}
}

void cute_read_unlock(cute_rw_lock_t* rw)
{
	// Write is pending.
	if (cute_atomic_add(&rw->readers, -1) < 0) {
		// The final departing reader notifies the pending writer.
		if (cute_atomic_add(&rw->readers_departing, -1) - 1 == 0) {
			cute_semaphore_post(&rw->write_sem);
		}
	}
}

void cute_write_lock(cute_rw_lock_t* rw)
{
	cute_lock(&rw->mutex);

	// Flip to negative to force new readers to wait. Record the number of active
	// readers at that moment, which need to depart to allow write access.
	int readers = cute_atomic_add(&rw->readers, -CUTE_RW_LOCK_MAX_READERS);

	// Wait for departing readers.
	if (cute_atomic_add(&rw->readers_departing, readers) + readers != 0) {
		cute_semaphore_wait(&rw->write_sem);
	}
}

void cute_write_unlock(cute_rw_lock_t* rw)
{
	// Flip to positive to allow new readers. Record the number of waiting readers
	// at that moment.
	int readers = cute_atomic_add(&rw->readers, CUTE_RW_LOCK_MAX_READERS) + CUTE_RW_LOCK_MAX_READERS;

	// Signal all waiting readers to wake.
	for (int i = 0; i < readers; ++i) {
		cute_semaphore_post(&rw->read_sem);
	}

	cute_unlock(&rw->mutex);
}

void cute_rw_lock_destroy(cute_rw_lock_t* rw)
{
	cute_mutex_destroy(&rw->mutex);
	cute_semaphore_destroy(&rw->write_sem);
	cute_semaphore_destroy(&rw->read_sem);
}

#define CUTE_SYNC_ALIGN_PTR(X, Y) ((((size_t)X) + ((Y) - 1)) & ~((Y) - 1))

static void* cute_malloc_aligned(size_t size, int alignment, void* mem_ctx)
{
	(void)mem_ctx;
	int is_power_of_2 = alignment && !(alignment & (alignment - 1));
	CUTE_SYNC_ASSERT(is_power_of_2);
	void* p = CUTE_SYNC_ALLOC(size + alignment, mem_ctx);
	if (!p) return 0;
	unsigned char offset = (unsigned char)((size_t)p & (alignment - 1));
	p = (void*)CUTE_SYNC_ALIGN_PTR(p + 1, alignment);
	*((char*)p - 1) = alignment - offset;
	return p;
}

static void cute_free_aligned(void* p, void* mem_ctx)
{
	(void)mem_ctx;
	if (!p) return;
	size_t alignment = (size_t)*((char*)p - 1) & 0xFF;
	CUTE_SYNC_FREE((char*)p - alignment, mem_ctx);
}

typedef struct cute_task_t
{
	void (*do_work)(void*);
	void* param;
} cute_task_t;

typedef struct cute_threadpool_t
{
	int task_capacity;
	int task_count;
	cute_task_t* tasks;
	cute_mutex_t task_mutex;

	int thread_count;
	cute_thread_t** threads;

	cute_atomic_int_t running;
	cute_mutex_t sem_mutex;
	cute_semaphore_t semaphore;
	void* mem_ctx;
} cute_threadpool_t;

int cute_try_pop_task_internal(cute_threadpool_t* pool, cute_task_t* task)
{
	cute_lock(&pool->task_mutex);

	if (pool->task_count) {
		*task = pool->tasks[--pool->task_count];
		cute_unlock(&pool->task_mutex);
		return 1;
	}

	cute_unlock(&pool->task_mutex);
	return 0;
}

int cute_worker_thread_internal(void* udata)
{
	cute_threadpool_t* pool = (cute_threadpool_t*)udata;
	while (cute_atomic_get(&pool->running)) {
		cute_task_t task;
		if (cute_try_pop_task_internal(pool, &task)) {
			task.do_work(task.param);
		}

		cute_semaphore_wait(&pool->semaphore);
	}
	return 0;
}

cute_threadpool_t* cute_threadpool_create(int thread_count, void* mem_ctx)
{
	if (CUTE_SYNC_CACHELINE_SIZE < cute_cacheline_size()) return 0;

	cute_threadpool_t* pool = (cute_threadpool_t*)CUTE_SYNC_ALLOC(sizeof(cute_threadpool_t), mem_ctx);
	pool->task_capacity = 64;
	pool->task_count = 0;
	pool->tasks = (cute_task_t*)cute_malloc_aligned(sizeof(cute_task_t) * pool->task_capacity, CUTE_SYNC_CACHELINE_SIZE, mem_ctx);
	pool->task_mutex = cute_mutex_create();
	pool->thread_count = thread_count;
	pool->threads = (cute_thread_t**)cute_malloc_aligned(sizeof(cute_thread_t*) * thread_count, CUTE_SYNC_CACHELINE_SIZE, mem_ctx);
	cute_atomic_set(&pool->running, 1);
	pool->sem_mutex = cute_mutex_create();
	pool->semaphore = cute_semaphore_create(0);
	pool->mem_ctx = mem_ctx;

	for (int i = 0; i < thread_count; ++i) {
		pool->threads[i] = cute_thread_create(cute_worker_thread_internal, 0, pool);
	}

	return pool;
}

void cute_threadpool_add_task(cute_threadpool_t* pool, void (*func)(void*), void* param)
{
	cute_lock(&pool->task_mutex);

	if (pool->task_count == pool->task_capacity) {
		int new_cap = pool->task_capacity * 2;
		cute_task_t* new_tasks = (cute_task_t*)cute_malloc_aligned(sizeof(cute_task_t) * new_cap, CUTE_SYNC_CACHELINE_SIZE, pool->mem_ctx);
		CUTE_SYNC_MEMCPY(new_tasks, pool->tasks, sizeof(cute_task_t) * pool->task_count);
		cute_free_aligned(pool->tasks, pool->mem_ctx);
		pool->task_capacity = new_cap;
		pool->tasks = new_tasks;
	}

	cute_task_t task;
	task.do_work = func;
	task.param = param;
	pool->tasks[pool->task_count++] = task;

	cute_unlock(&pool->task_mutex);
}

void cute_threadpool_kick_and_wait(cute_threadpool_t* pool)
{
	cute_threadpool_kick(pool);

	while (pool->task_count) {
		cute_task_t task;
		if (cute_try_pop_task_internal(pool, &task)) {
			cute_semaphore_try(&pool->semaphore);
			task.do_work(task.param);
		}
		CUTE_SYNC_YIELD();
	}
}

void cute_threadpool_kick(cute_threadpool_t* pool)
{
	if (pool->task_count) {
		int count = pool->task_count < pool->thread_count ? pool->task_count : pool->thread_count;
		for (int i = 0; i < count; ++i) {
			cute_semaphore_post(&pool->semaphore);
		}
	}
}

void cute_threadpool_destroy(cute_threadpool_t* pool)
{
	cute_atomic_set(&pool->running, 0);

	for (int i = 0; i < pool->thread_count; ++i) {
		cute_semaphore_post(&pool->semaphore);
	}

	for (int i = 0; i < pool->thread_count; ++i) {
		cute_thread_wait(pool->threads[i]);
	}

	cute_free_aligned(pool->tasks, pool->mem_ctx);
	cute_free_aligned(pool->threads, pool->mem_ctx);
	void* mem_ctx = pool->mem_ctx;
	(void)mem_ctx;
	CUTE_SYNC_FREE(pool, mem_ctx);
}

#endif // CUTE_SYNC_IMPLEMENTATION_ONCE
#endif // CUTE_SYNC_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2019 Randy Gaul http://www.randygaul.net
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
