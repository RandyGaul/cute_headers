/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_coroutine.h - v1.01

	SUMMARY

		This header implements a C-style coroutine API used to implement state machines
		or "script" out code in a very linear and readable way. The main purpose of
		this kind of coroutine is to be able to pause a function's execution at any moment,
		and then resume from that point later.

		Idea comes from: https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html

	USE CASE EXAMPLE

		Here is a quick example: Say you are implementing a dialogue system for a
		game displaying lots of text. You want to write out some text to the screen, and
		then pause for a short time, and then continue on to the next text until finished.
		The "typical" way to implement this kind of thing would be to use some kind of
		state machine and lots of little timers. This can get really cumbersome very fast.
		As an alternative, a coroutine header like this one can look really simple, and
		be very easy to write. Here's the dialogue (no code yet):

			Bob     Yo alice. I heard you like mudkips.
			Alice   No Bob. Not me. Who told you such a thing?
			Bob     Alice please, don't lie to me. We've known each other a long time.
			Alice   We have grown apart. I barely know myself.
			Bob     OK.
			Alice   Good bye Bob. I wish you the best.
			Bob     But do you like mudkips?
			Alice   <has left>
			Bob     Well, I like mudkips :)

		And here is a quick implementation as a coroutine. It looks like this when run:
			https://twitter.com/RandyPGaul/status/1058920774973571079
		And here's the implementation:

			#include <stdio.h>
			#include <stdlib.h>
			#include <cute_coroutine.h>

			#ifdef _MSC_VER
				#include <windows.h>
			#else
				#include <unistd.h>
			#endif

			void print(coroutine_t* co, float dt, const char* character_name, const char* string)
			{
				static int index; // Not thread safe, but can easily be changed to a ptr as input param.
				int milliseconds = rand() % 10 + 20;
				char c = 0;

				COROUTINE_START(co);

				index = 0;
				printf("%-8s", character_name);
				COROUTINE_WAIT(co, 750, dt);

				COROUTINE_CASE(co, print_char);
				if (string[index]) {
					c = string[index++];
					printf("%c", c);
					if (c == '.' || c == ',' || c == '?') COROUTINE_WAIT(co, 250, dt);
					else COROUTINE_WAIT(co, milliseconds, dt);
					goto print_char;
				}

				COROUTINE_END(co);
			}

			int do_coroutine(coroutine_t* co, float dt)
			{
				int keep_going = 1;
				int milliseconds = 1000;

				COROUTINE_START(co);
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Bob", "Yo Alice. I heard you like mudkips.\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Alice", "No Bob. Not me. Who told you such a thing?\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Bob", "Alice please, don't lie to me. We've known each other a long time.\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Alice", "We have grown apart. I barely know myself.\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Bob", "OK.\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Alice", "Good bye Bob. I wish you the best.\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Bob", "But do you like mudkips?\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Alice", "<has left>\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				COROUTINE_CALL(co, print(co, dt, "Bob", "Well, I like mudkips :)\n"));
				COROUTINE_WAIT(co, milliseconds, dt);
				keep_going = 0;
				COROUTINE_END(co);

				return keep_going;
			}

			int main()
			{
				coroutine_t co;
				coroutine_init(&co);

				while (do_coroutine(&co, 1))
				{
					// Sleep for a second.
					#ifdef _MSC_VER
						Sleep(1);
					#else
						usleep(1000);
					#endif
					}

				system("pause");

				return 0;
			}

	SEQUENCE POINTS

		The docs for each function mention "sequence points". A sequence point is any line in
		the coroutine that is recorded, and will be jumped to whenever the coroutine has exited
		and entered again later. The idea is the sequence points store the code line number
		inside the coroutine's state, for later resuming.

	LOCAL VARIABLES

		Some special care needs to be taken in order to not screw up local variables when using
		a coroutine. Since the coroutine is implemented as a big jump table with switches/gotos,
		local variable initialization can easily be skipped. The idea is that initialization of
		local variables can only happen after the last sequence point, but not before (since the
		coroutine is resumed at the last recorded sequence point).

		If a real local variable is needed, it is best to store your local variable externally
		somewhere, and then handle it inside the coroutine via pointer. This way scoping and
		initialization of your local variables are handled externally to the coroutine, and can
		remain unnaffected by the coroutine jumping around.

		However, with some thought and familiarity, most of the instances where a local variable
		is needed can be easily coaxed into a comfortable form (like in the above example code).

		In version 1.01 explicit support for local variables was added via `coroutine_local_var`.
		Check out the examples in the doc comments around the function definition.

	RUNNING ON WINDOWS

		There's a problem with MSCV. Since MSCV does some nonstandard stuff with their
		`Edit and Continue` when generating debug information. The compiler switch
		causing problems is `ZI`; change it to `Zi` to fix the issue. This is under
		Properties >> C/C++ >> All Options >> Debug Information Format.

	Revision history:
		1.0  (11/03/2018) initial release
		1.01 (01/26/2020) added support for local variables
*/

#ifndef CUTE_COROUTINE_H

#define COROUTINE_MAX_DEPTH 8
#define COROUTINE_CASE_OFFSET (1024 * 1024)
#define COROUTINE_STACK_SIZE (512)

#ifndef COROUTINE_ASSERT
	#include <assert.h>
	#define COROUTINE_ASSERT assert
#endif

/**
 * All the state necessary to run a coroutine. You own the instance of this coroutine
 * completely (so allocate it on the stack, or heap, as you wish). Each coroutine is
 * 100% independent from one another, as no static memory is ever used.
 */
typedef struct
{
	float elapsed;
	int flag;
	int index;
	int line[COROUTINE_MAX_DEPTH];
	int stack_pointer;
	char stack[COROUTINE_STACK_SIZE];
} coroutine_t;

/**
 * Clears the entire coroutine (including all sub-coroutines) to their initial state.
 * Must be called before the first time a coroutine is started, and can be used to
 * completely reset a coroutine. Since coroutines are simply cleared to 0 here,
 * static/file scope coroutines don't need to use this function.
 *
 * Since this function simply sets everything to 0, it's totally fine to set up a
 * static coroutine knowing it's in a valid initial state.
 *
 * `static coroutine_t s_co;`
 */
static inline void coroutine_init(coroutine_t* co)
{
	co->elapsed = 0;
	co->flag = 0;
	co->index = 0;
	for (int i = 0; i < COROUTINE_MAX_DEPTH; ++i) co->line[i] = 0;
	co->stack_pointer = 0;
	/* No reason to clear the stack. */
}

/**
 * Pops off `size` bytes from `co`'s fixed-size buffer for use as a local variable.
 * Since local variables behave a little differently in the presence of a coroutine
 * it's useful to be able to have a little working memory associated with the life-
 * time of the coroutine itself.
 *
 * Here's an example.
 *
 *     void do_work(coroutine_t* co)
 *     {
 *         int* a = (int*)coroutine_local_var(co, sizeof(int));
 *         int* b = (int*)coroutine_local_var(co, sizeof(int));
 *
 *         COROUTINE_START(co);
 *         // Setup initial values here. This code runs just once. These persist.
 *         *a = 3;
 *         *b = 7;
 *
 *         // ...
 *
 *         COROUTINE_END(co);
 *     }
 *
 * One annoying thing about the above example is constantly typing `*a` with the de-
 * reference operator. Not only is the typing annoying, but it also does incur a real
 * performance hit, especially once pointer aliasing is considered. This problem
 * persists even when using references in C++ with the templated version of
 * `coroutine_local_var`. The only workaround is to use a pattern of reading from
 * pointers onto the actual CRT stack, and finally pushing the values back out to the
 * coroutine stack.
 *
 *     void do_work(coroutine_t* co)
 *     {
 *         // Setup locals onto the CRT (C runtime) stack.
 *         int* a_ptr = (int*)coroutine_local_var(co, sizeof(int));
 *         int* b_ptr = (int*)coroutine_local_var(co, sizeof(int));
 *         int a = *a_ptr;
 *         int b = *b_ptr;
 *
 *         COROUTINE_START(co);
 *         // ...
 *         COROUTINE_END(co);
 *
 *         // Push locals back onto the coroutine's stack.
 *         *a_ptr = a;
 *         *b_ptr = b;
 *     }
 */
static inline void* coroutine_local_var(coroutine_t* co, int size)
{
	COROUTINE_ASSERT(co->stack_pointer + size < COROUTINE_STACK_SIZE);
	void* ptr = co->stack + co->stack_pointer;
	co->stack_pointer += (int)size;
	return ptr;
}

#ifdef __cplusplus
/**
 * Helper function to setup "local variables" as references.
 *
 * Here's an example.
 *
 *     void do_work(coroutine_t* co)
 *     {
 *         int& a = coroutine_local_var<int>(co);
 *         int& b = coroutine_local_var<int>(co);
 *
 *         COROUTINE_START(co);
 *         // Setup initial values here. This code runs just once. These persist.
 *         a = 3;
 *         b = 7;
 *
 *         // ...
 *
 *         COROUTINE_END(co);
 *     }
 */
template <typename T>
static inline T& coroutine_local_var(coroutine_t* co)
{
	return *(T*)coroutine_local_var(co, sizeof(T));
}
#endif

/**
 * Begins the coroutine. Code written between here and the next sequence point will be run
 * only once, making for a good "single time init" spot to write code.
 */
#define COROUTINE_START(co)          do { co->flag = 0; switch (co->line[co->index]) { default: /* default is 0, and will run just once. */

/**
 * Sets a named sequence point. At any moment, a goto can be used to jump to `name`. Jumping to `name`
 * will set a sequence point at this location in the coroutine.
 */
#define COROUTINE_CASE(co, name)     case __LINE__: name: co->line[co->index] = __LINE__;

/**
 * Sets a timed sequence point. The coroutine will idle here until a certain time has elapsed.
 * This is similar to calling `COROUTINE_YIELD`, except will continue to yield until `time1`
 * has elapsed. `dt` should be a float, represents delta-time.
 */
#define COROUTINE_WAIT(co, time, dt) do { case __LINE__: co->line[co->index] = __LINE__; co->elapsed += dt; do { if (co->elapsed < time) { co->flag = 1; goto __co_end; } else { co->elapsed = 0; } } while (0); } while (0)

/**
 * Jumps out of the coroutine, but does not set a sequence point. Returns exection to the top
 * level `COROUTINE_END` call.
 */
#define COROUTINE_EXIT(co)           do { co->flag = 1; goto __co_end; } while (0)

/**
 * Jumps out of the coroutine, and sets a sequence point. The next time the coroutine
 * is entered, this sequence point will be resumed. Returns execution to the top level
 * `COROUTINE_END`.
 */
#define COROUTINE_YIELD(co)          do { co->line[co->index] = __LINE__; COROUTINE_EXIT(co); case __LINE__:; } while (0)

/**
 * Used as a wrapper to enter into a subroutine from within a coroutine. Will set a
 * sequence point before/after the call. Supports coroutines within the called function.
 * If the underlying coroutine called `COROUTINE_YIELD` or `COROUTINE_EXIT`, the next
 * time this coroutine is entered, the sequence point used to *enter* the subroutine
 * will be resumed (and the subroutine will be entered again). If the underlying
 * coroutine did not call `COROUTINE_YIELD` or `COROUTINE_EXIT`, it is assumed the
 * underlying coroutine completed, and the sequence point *after* the subroutine will
 * be resumed, this skipping the subroutine completely. Up to a maximum of
 * `COROUTINE_DEPTH_MAX`nested routines can be used with `COROUTINE_CALL`.
 *
 * Here is a quick example to show the syntax:
 *
 *     `COROUTINE_CALL(co, call_some_other_func(co, my_params));`
 */
#define COROUTINE_CALL(co, ...)      co->flag = 0; case __LINE__: COROUTINE_ASSERT(co->index < COROUTINE_MAX_DEPTH); co->line[co->index++] = __LINE__; __VA_ARGS__; co->index--; do { if (co->flag) { goto __co_end; } else { case __LINE__ + COROUTINE_CASE_OFFSET: co->line[co->index] = __LINE__ + COROUTINE_CASE_OFFSET; } } while (0)

/**
 * Must be called at the very end of the coroutine. Does not set a sequence point;
 * simply completes the coroutine syntax -- this is where code execution will resume
 * whenever control is exiting/yielding from the coroutine.
 *
 * If control flow naturally reaches `COROUTINE_END` without jumping, the sequence
 * point will be reset to `COROUTINE_START` to simply reset the whole coroutine.
 * However, the sequence point will not be set in the usual case of calling
 * `COROUTINE_YIELD`, `COROUTINE_WAIT` or `COROUTINE_EXIT`.
 */
#define COROUTINE_END(co)            } co->line[co->index] = 0; __co_end:; co->stack_pointer = 0; } while (0)

#define CUTE_COROUTINE_H
#endif // CUTE_COROUTINE_H

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
