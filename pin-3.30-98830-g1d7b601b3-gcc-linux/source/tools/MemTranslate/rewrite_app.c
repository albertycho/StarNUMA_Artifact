/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * This application initialize an allocated array buffer.
 * Four modes of operation:
 * 32: buffer is allocated in 32b memory space and initilaization is done using
 *     rep stos instruction with 32 bit address size, meaning 0x67 instruction prefix
 * 64: buffer is allocated normally, 64b memory space and initialization is done
 *     using regular rep stos instruction with no prefix
 * movs32: Same as mode 32 but instead of rep stos instruction, we use rep movs
 * movs64: Same as mode 64 but instead of rep stos instruction, we use rep movs
 * mov32: Same as mode 32 but instead of rep stos instruction, we use mov
 * mov64: Same as mode 64 but instead of rep stos instruction, we use mov
 */

#define TEST_VALUE 0x12
#define TEST_LOOP_COUNT 0xa

unsigned char* buff = NULL;
unsigned char* buff_last = NULL;
unsigned char* mov_source = NULL;

// Optimize -O0 since compiler is manipulating
// assembly code in some unexpected ways
#pragma GCC optimize("O0")

int check_write_test()
{
    if (*buff != TEST_VALUE || buff[sizeof(int) * (TEST_LOOP_COUNT - 1)] != TEST_VALUE) return 1;
    return 0;
}

int check_mov_test()
{
    if (*buff != TEST_VALUE) return 1;
    return 0;
}

// Those test function are disabled inline since
// we want the tool to detect function name during
// instrumentation
int __attribute__ ((noinline)) movs_to_buff_32()
{
    __asm__ ("mov buff, %rdi");
    __asm__ ("movl $0xa, %ecx"); 
    __asm__ ("mov mov_source, %rsi");
    __asm__ (".byte 0x67"); // rep movsd [esi], [edi]
    __asm__ (".byte 0xf3");
    __asm__ (".byte 0xa5");
    return check_write_test();
}

int __attribute__ ((noinline)) movs_to_buff_64()
{
    __asm__ ("mov buff, %rdi");
    __asm__ ("movl $0xa, %ecx"); 
    __asm__ ("mov mov_source, %rsi");
    __asm__ (".byte 0xf3"); // rep movsd [rsi], [rdi]
    __asm__ (".byte 0xa5");
    return check_write_test();
}

// In the next two methods fnop is used as
// a marker to determine the mov instruction (just
// after that) during the instrumentation function.
int __attribute__((noinline)) mov_to_buff_32()
{
    __asm__ ("mov $0x12, %rax"); 
    __asm__ ("mov buff, %rdi");
    __asm__ ("fnop");
    __asm__ (".byte 0x67"); // mov eax, [edi]
    __asm__ (".byte 0x89");
    __asm__ (".byte 0x07");
    return check_mov_test();
}

int __attribute__((noinline)) mov_to_buff_64()
{
    __asm__ ("mov $0x12, %rax"); 
    __asm__ ("mov buff, %rdi");
    __asm__ ("fnop");
    __asm__ (".byte 0x89"); // mov eax, [rdi]
    __asm__ (".byte 0x07");
    return check_mov_test();
}

int __attribute__((noinline)) write_to_buff_32()
{
    __asm__ ("movl $0x12, %eax"); 
    __asm__ ("movl $0xa, %ecx"); 
    __asm__ ("mov buff, %rdi");
    __asm__ (".byte 0x67");
    __asm__ (".byte 0xf3");
    __asm__ (".byte 0xab");
    return check_write_test();
}

int __attribute__((noinline)) write_to_buff_64()
{
    __asm__ ("movl $0x12, %eax"); 
    __asm__ ("movl $0xa, %ecx"); 
    __asm__ ("mov buff, %rdi");
    __asm__ (".byte 0xf3");
    __asm__ (".byte 0xab");
    return check_write_test();
}

enum test_mode_e
{
    e_write32,
    e_write64,
    e_moves32,
    e_moves64,
    e_mov32,
    e_mov64
};

void Usage(char** argv)
{
    fprintf(stderr, "Syntax is: %s <test name>\n", argv[0]);
    fprintf(stderr, "\t\ttest_name:\n");
    fprintf(stderr, "\t\t\t64:\trep stos test 64 bits address size\n");
    fprintf(stderr, "\t\t\t32:\trep stos test 32 bits address size\n");
    fprintf(stderr, "\t\t\tmovs64:\trep movs test 64 bits address size\n");
    fprintf(stderr, "\t\t\tmovs32:\trep movs test 32 bits address size\n");
    fprintf(stderr, "\t\t\tmov64:\tmov test 64 bits address size\n");
    fprintf(stderr, "\t\t\tmov32:\tmov test 32 bits address size\n");
}

int main(int argc, char *argv[])
{
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    enum test_mode_e mode = e_write32;

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "32") == 0))
    {
        fprintf(stdout, "rep stos and use 32 bits prefix address\n");
        flags |= MAP_32BIT;
    }
    else if (argc == 2 && strcmp(argv[1], "movs32") == 0)
    {
        fprintf(stdout, "rep movs and use 32 bits prefix address\n");
        flags |= MAP_32BIT;
        mode = e_moves32;
    }
    else if (argc == 2 && strcmp(argv[1], "movs64") == 0)
    {
        fprintf(stdout, "rep movs and use 64 bits prefix address\n");
        mode = e_moves64;
    }
    else if (argc == 2 && strcmp(argv[1], "64") == 0)
    {
        fprintf(stdout, "rep stos and use 64 bits prefix address\n");
        mode = e_write64;
    }
    else if (argc == 2 && strcmp(argv[1], "mov64") == 0)
    {
        fprintf(stdout, "Mov and use 64 bits prefix address\n");
        mode = e_mov64;
    }
    else if (argc == 2 && strcmp(argv[1], "mov32") == 0)
    {
        fprintf(stdout, "Mov and use 32 bits prefix address\n");
        flags |= MAP_32BIT;
        mode = e_mov32;
    }
    else
    {
        Usage(argv);
        return 1;
    }

    buff = (unsigned char*)mmap(0, 4096, PROT_READ|PROT_WRITE, flags, -1, 0);
    if (buff == MAP_FAILED)
    {
        perror("mmap");
        // If the allocation failed in 32 bits mode: we do not fail the test as
        // this could happen and is not related to a test failure.
        if (flags & MAP_32BIT) return 0;
        return 1;
    }

    buff_last = buff + 36;
    fprintf(stderr, "Buff: %p\n", buff);

    int ret;
    if (mode == e_write32)
        ret = write_to_buff_32();
    else if (mode == e_write64)
        ret = write_to_buff_64();
    else
    {
        // Since mov / rep movs instructions work with
        // two buffers (src and dst) as opposed to a destination buffer
        // and a register value source, therefore we need to initialize
        // first the source buffer.
        mov_source = buff + 2048;
        unsigned char* cur = mov_source;
        int i              = 0;
        for (; i < TEST_LOOP_COUNT; cur += 4, i++)
            *cur = TEST_VALUE;

        switch (mode)
        {
            case e_moves64:
                ret = movs_to_buff_64();
                break;
            case e_moves32:
                ret = movs_to_buff_32();
                break;
            case e_mov64:
                ret = mov_to_buff_64();
                break;
            case e_mov32:
                ret = mov_to_buff_32();
                break;
            default:
                assert(0);
        }
    }

    if (ret)
    {
        fprintf(stderr, "Failure!!!\n");
        return 1;
    }

    fprintf(stdout, "Done\n");
    return(0);
}
