/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#ifdef TARGET_WINDOWS
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "syscall_iteration.h"

uint8_t global_buffer[64];
// buffer used for unbuffered I/O; must be aligned on Linux to 512 multiple address
alignas(512) uint8_t global_buffer1[512];

/* One test iteration write executes:
            1 fopen
            1 fclose
            1 fwrite */
unsigned int one_iteration_write(const char* fname)
{
    FILE* fp = fopen(fname, "w+");
    size_t size;

    if (!fp)
    {
        std::cerr << "file not opened\n";
        return -1;
    }

    size = fwrite(global_buffer, 1, sizeof(global_buffer), fp);
    fclose(fp);
    if (sizeof(global_buffer) != size)
    {
        std::cerr << "unexpected size written\n";
        return -1;
    }

    return 0;
}

int init_write(char* fname)
{
    memset(global_buffer, 4, sizeof(global_buffer));
    std::tmpnam(fname);
    return 0;
}

void uninit_write(char* fname) { remove(fname); }

/* One test iteration read executes:
            1 fopen
            1 fclose
            1 fread */
unsigned int one_iteration_read(const char* fname)
{
    FILE* fp = fopen(fname, "r");
    char buffer[sizeof(global_buffer)];
    size_t size;

    if (!fp)
    {
        std::cerr << "file not opened\n";
        return -1;
    }

    size = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);
    if (sizeof(buffer) != size)
    {
        std::cerr << "unexpected size read: " << size << std::endl;
        return -1;
    }

    return 0;
}

int init_read(char* fname)
{
    memset(global_buffer, 4, sizeof(global_buffer));
    std::tmpnam(fname);
    return one_iteration_write(fname);
}

#ifdef TARGET_WINDOWS

/* One test iteration unbuffered write executes:
            1 WriteFile */
thread_local HANDLE g_hFile = INVALID_HANDLE_VALUE;
OVERLAPPED ovl = {0};
unsigned int one_iteration_write_uio(const char* fname)
{
    DWORD dwWritten;
    BOOL fSuccess = WriteFile(g_hFile, global_buffer1, sizeof(global_buffer1), &dwWritten, &ovl);
    if (fSuccess || ERROR_IO_PENDING == GetLastError()) return 0;

    return 1;
}

int init_write_uio(char* fname)
{
    memset(global_buffer1, 2, sizeof(global_buffer1));
    std::tmpnam(fname);
    g_hFile = CreateFile(fname, GENERIC_WRITE | GENERIC_READ,
                         0,    // no share
                         NULL, // no security
                         CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, // normal file, no buffering
                         NULL);
    if (g_hFile != INVALID_HANDLE_VALUE) return 0;
    return 1;
}

void uninit_write_uio(char* fname)
{
    CloseHandle(g_hFile);
    remove(fname);
}

int init_read_uio(char* fname)
{
    init_write_uio(fname);
    return one_iteration_write_uio(fname);
}

/* One test iteration unbuffered read executes:
            1 ReadFile */
unsigned int one_iteration_read_uio(const char* fname)
{
    DWORD dwRead;
    uint8_t buffer[sizeof(global_buffer1)];
    BOOL fSuccess = ReadFile(g_hFile, buffer, sizeof(buffer), &dwRead, &ovl);
    if (fSuccess || ERROR_IO_PENDING == GetLastError()) return 0;
    return 1;
}

/* One test iteration NtQueryInformation:
   this is an emulation flow test */
unsigned int one_iteration_queryprocess(const char* fname)
{
    HANDLE process = GetCurrentProcess();

    DWORD_PTR processAffinityMask;
    DWORD_PTR systemAffinityMask;

    /* One iteration loops 50 in order to get larger iteration
       elapsed time */
    for (int i = 0; i < 50; i++)
        /* This one calls finally to NtQueryInformationProcess which is an 
           emulated system call */
        if (!GetProcessAffinityMask(process, &processAffinityMask, &systemAffinityMask)) return -1;

    return 0;
}

/* One test iteration NtAllocateVirtualMemory and NtFreeVirtualMemory:
   this is an emulation flow test */
unsigned int one_iteration_allocatevirtualmemory(const char* fname)
{
    constexpr DWORD dwSize = 1 << 14;

    /* One iteration loops 50 in order to get larger iteration
       elapsed time */
    for (int i = 0; i < 50; i++)
    {
        /* VirtualAlloc finally calls NtAllocateVirtualMemory which is an 
           emulated system call */
        LPVOID lpvResult = VirtualAlloc(NULL, dwSize,
                                        MEM_COMMIT,      // Allocate a committed page
                                        PAGE_READWRITE); // Read/write access
        if (lpvResult == NULL)
        {
            return -1;
        }

        /* VirtualFree finally calls to NtFreeVirtualMemory which is also an 
           emulated system call */
        if (!VirtualFree(lpvResult, 0, MEM_RELEASE))
        {
            return -1;
        }
    }

    return 0;
}

/* One test iteration NtDelayExecution called by Sleep()
   this is a dispatch test, i.e. system call is not emulated */
unsigned int one_iteration_execdelay(const char* fname)
{
    for (int i = 0; i < 100; i++)
        Sleep(0);
    return 0;
}

#else // !TARGET_WINDOWS

#include <sched.h>

unsigned int one_iteration_execdelay(const char* fname)
{
    for (int i = 0; i < 100; i++)
        if (sched_yield())
        {
            return -1;
        }

    return 0;
}

unsigned int one_iteration_allocatevirtualmemory(const char* fname)
{
    constexpr size_t size = 1 << 14;

    /* One iteration loops 50 in order to get larger iteration
       elapsed time */
    for (int i = 0; i < 50; i++)
    {
        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED)
        {
            return -1;
        }
        if (munmap(ptr, size))
        {
            return -1;
        }
    }

    return 0;
}

/* One test iteration unbuffered write executes:
            1 pwrite */
thread_local int g_hFile = -1;
unsigned int one_iteration_write_uio(const char* fname)
{
    if (-1 != pwrite(g_hFile, global_buffer1, sizeof(global_buffer1), 0)) return 0;
    fprintf(stderr, "errno = %d\n", errno);
    return 1;
}

int init_write_uio(char* fname)
{
    memset(global_buffer1, 4, sizeof(global_buffer1));
    std::tmpnam(fname);
    g_hFile = open(fname, O_CREAT | O_RDWR | O_DIRECT);
    if (g_hFile != -1) return 0;
    printf("errno = %d\n", errno);
    return 1;
}

void uninit_write_uio(char* fname)
{
    close(g_hFile);
    remove(fname);
}

unsigned int one_iteration_read_uio(const char* fname)
{
    if (-1 != pread(g_hFile, global_buffer1, sizeof(global_buffer1), 0)) return 0;
    fprintf(stderr, "errno = %d\n", errno);
    return 1;
}

int init_read_uio(char* fname) { return init_write_uio(fname) + one_iteration_write_uio(fname); }

#endif // !TARGET_WINDOWS
