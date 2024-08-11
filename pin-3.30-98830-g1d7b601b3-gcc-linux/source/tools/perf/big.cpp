/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include "syscall_iteration.h"

/* The two next templates enforce recursive compilation and
   generate a large binary with a few code lines whe j is big.
   This is important in order to make PIN process do jitting
   as much as possible and been the main processing done */
template< int j > struct Foo
{
    static int Worker(int x) { return j ^ x ^ Foo< j - 1 >::Worker(x + j); }
};

template<> struct Foo< 0 >
{
    static int Worker(int x) { return x; }
};

template< int j > struct Bar
{
    static int Worker(int x) { return j ^ x ^ Bar< j - 1 >::Worker(x + j); }
};

template<> struct Bar< 0 >
{
    static int Worker(int x) { return x; }
};

void Usage(char** argv)
{
    std::cerr << "Syntax is:";
    std::cerr << "\tOne thread test:\n";
    std::cerr << "\t" << argv[0] << "\n";
    std::cerr << "\tTwo threads test:\n";
    std::cerr << "\t" << argv[0] << " -mt\n";
}

int foo()
{
#if defined(TARGET_LINUX) && defined(TARGET_IA32)
    // For some reason 80000 with 32b takes a very long time with pin
    int value = Foo< 30000 >::Worker(rand());
#else
    int value = Foo< 80000 >::Worker(rand());
#endif
    return value;
}

int bar()
{
#if defined(TARGET_LINUX) && defined(TARGET_IA32)
    int value = Bar< 30000 >::Worker(rand());
#else
    int value = Bar< 80000 >::Worker(rand());
#endif
    return value;
}

int main(int argc, char** argv)
{
    bool mode_statistic = false;
    std::vector< std::thread > threads;

    if (argc != 1 && argc != 2)
    {
        Usage(argv);
        return 1;
    }

    /* mt == true: execute two threads otherwise, only one */
    bool mt = (argc == 2) && (0 == strcmp(argv[1], "-mt"));

    auto start = myclock::now();

    threads.push_back(std::thread(foo));
    if (mt) threads.push_back(std::thread(bar));

    threads[0].join();
    if (mt) threads[1].join();

    auto end                     = myclock::now();
    auto time_spent              = std::chrono::duration_cast< std::chrono::microseconds >(end - start);
    unsigned int test_time_usecs = time_spent.count();

    std::cout << "Iteration delay: " << test_time_usecs << " usec\n";

    return 0;
}
