/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/* According to the ARM EABI, all registers have undefined values at
 * program startup except:
 *
 * - the instruction pointer (r15)
 * - the stack pointer (r13)
 * - the rtld_fini pointer (r0)
 */
#define BRANCH(stack_pointer, destination) do {			\
	register word_t _sp asm("r1") = (word_t)(stack_pointer);	\
	register word_t _dest asm("r2") = (word_t)(destination);	\
	asm volatile (						\
		"// Restore initial stack pointer.	\n\t"	\
		"mov sp, %0				\n\t"	\
		"					\n\t"	\
		"// Clear rtld_fini.			\n\t"	\
		"mov r0, #0				\n\t"	\
		"					\n\t"	\
		"// Start the program.			\n\t"	\
		"bx %1					\n"	\
		: /* no output */				\
		: "r" (_sp), "r" (_dest)			\
		: "memory", "r0");				\
	__builtin_unreachable();				\
	} while (0)

static inline __attribute__((always_inline)) word_t syscall_1(word_t number, word_t a1)
{
	register word_t r0 asm("r0") = a1;
	register word_t r7 asm("r7") = number;
	asm volatile("svc #0" : "+r"(r0) : "r"(r7) : "memory");
	return r0;
}

static inline __attribute__((always_inline)) word_t syscall_3(word_t number, word_t a1, word_t a2, word_t a3)
{
	register word_t r0 asm("r0") = a1;
	register word_t r1 asm("r1") = a2;
	register word_t r2 asm("r2") = a3;
	register word_t r7 asm("r7") = number;
	asm volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
	return r0;
}

static inline __attribute__((always_inline)) word_t syscall_4(word_t number, word_t a1, word_t a2, word_t a3, word_t a4)
{
	register word_t r0 asm("r0") = a1;
	register word_t r1 asm("r1") = a2;
	register word_t r2 asm("r2") = a3;
	register word_t r3 asm("r3") = a4;
	register word_t r7 asm("r7") = number;
	asm volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory");
	return r0;
}

static inline __attribute__((always_inline)) word_t syscall_6(word_t number, word_t a1, word_t a2, word_t a3, word_t a4, word_t a5, word_t a6)
{
	register word_t r0 asm("r0") = a1;
	register word_t r1 asm("r1") = a2;
	register word_t r2 asm("r2") = a3;
	register word_t r3 asm("r3") = a4;
	register word_t r4 asm("r4") = a5;
	register word_t r5 asm("r5") = a6;
	register word_t r7 asm("r7") = number;
	asm volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r7) : "memory");
	return r0;
}

#define SYSCALL(number_, nb_args, args...) syscall_##nb_args((word_t)(number_), args)

#define OPEN	5
#define CLOSE	6
#define MMAP	192
#define MMAP_OFFSET_SHIFT 12
#define EXECVE	11
#define EXIT	1
#define PRCTL	172
#define MPROTECT 125

