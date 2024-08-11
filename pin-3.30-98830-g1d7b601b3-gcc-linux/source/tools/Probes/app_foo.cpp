/*
 * Copyright (C) 2022-2022 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdio.h>

#ifdef TARGET_WINDOWS
#include <intrin.h>
#endif

#ifdef TARGET_LINUX
#include <unistd.h>
#include <sys/syscall.h>
#endif // TARGET_LINUX

#include <iostream>

/* 
 * This application simply calls a few functions and makes a few prints.
 * It also prints the ppid pid and tid since it is being used in a test
 * where this app is launched as a secondary application using different APIs.
 */
#ifdef TARGET_LINUX
static void print_ppid_pid_tid() { printf("ppid %u ; pid %u ; tid %lu ; \t", getppid(), getpid(), syscall(SYS_gettid)); }
#endif // TARGET_LINUX

#ifdef TARGET_WINDOWS
#define NOP __nop();
#define PRINT_PIDS
#else
#define NOP asm("nop");
#define PRINT_PIDS print_ppid_pid_tid();
#endif

#define PADDING NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP

void foo_impl(int* counter)
{
    (*counter) *= 17;
    (*counter) *= 17;
    (*counter) *= 17;
}

extern "C" void foo(int* counter)
{
    PADDING
    PRINT_PIDS
    std::cout << "  -> " << __FUNCTION__ << "()" << std::endl << std::flush;
    foo_impl(counter);
}
extern "C" void foo_1(int* counter)
{
    PADDING
    PRINT_PIDS
    std::cout << "  -> " << __FUNCTION__ << "()" << std::endl << std::flush;
    foo_impl(counter);
}

extern "C" void foo_2(int* counter)
{
    PADDING
    PRINT_PIDS
    std::cout << "  -> " << __FUNCTION__ << "()" << std::endl << std::flush;
    foo_impl(counter);
}

extern "C" void bar(void)
{
    PADDING
    PRINT_PIDS
    std::cout << "  -> " << __FUNCTION__ << "()" << std::endl << std::flush;
}

int main(int argc, char* argv[])
{
    int counter = 1;
    foo(&counter);
    foo_1(&counter);
    foo_2(&counter);
    bar();
    return 0;
}
