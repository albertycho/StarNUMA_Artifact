/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>

#include "syscall_iteration.h"

/* enum type to identify each and each test flow
   See the file readme.txt for a detailed description of the tests */
enum E_syscall_perf_test : int
{
    Invalid        = -1,
    Write          = 0,
    Read           = 1,
    AllocateMemory = 2,
    Execdelay      = 3,
    WriteUIO       = 4,
    ReadUIO        = 5,
#ifdef TARGET_WINDOWS
    QueryProcess = 6,
#endif
    Last = 9
};

/* counter takes a cache line in order not to come and go
   from core's to core's cache */
struct fastint_t
{
    alignas(64) unsigned int counter;
    fastint_t() : counter(0) {}
};

/* a type definition for iteration test function.
   Each test flow has a main test function, init, 
   calibrate and uninit function type  */
typedef unsigned int (*t_perf_test_func)(const char*);
typedef int (*t_perf_init_func)(char*);
typedef void (*t_perf_uninit_func)(char*);

/* Each test is describe by an object of the
   following type - keeping a pointer to the
   iteration functions */
typedef struct schema_s
{
    const char* name;
    E_syscall_perf_test flow;
    t_perf_test_func test_func;
    t_perf_init_func init_func;
    t_perf_uninit_func uninit_func;
} t_test_schema;

/* Array of test functions. One function for each test flow
   at index corresponding to the corresponding enum value above */
t_test_schema globalTestSchema[] = {
    {"Write", E_syscall_perf_test::Write, one_iteration_write, init_write, uninit_write},
    {"Read", E_syscall_perf_test::Read, one_iteration_read, init_read, uninit_write},
    {"Allocate", E_syscall_perf_test::AllocateMemory, one_iteration_allocatevirtualmemory, nullptr, nullptr},
    {"Execdelay", E_syscall_perf_test::Execdelay, one_iteration_execdelay, nullptr, nullptr},
    {"WriteUIO", E_syscall_perf_test::WriteUIO, one_iteration_write_uio, init_write_uio, uninit_write_uio},
    {"ReadUIO", E_syscall_perf_test::ReadUIO, one_iteration_read_uio, init_read_uio, uninit_write_uio},
#ifdef TARGET_WINDOWS
    {"QueryProcess", E_syscall_perf_test::QueryProcess, one_iteration_queryprocess, nullptr, nullptr},
#endif
    {"Last", E_syscall_perf_test::Last, nullptr, nullptr, nullptr},
};

/* Logging */
int verbose_level = 1;
bool verbose_normal() { return verbose_level >= 1; }
bool verbose_max() { return verbose_level >= 2; }

/* Test phases */
enum TEST_PHASES : int
{
    test_iterate_t,
    test_end_t
};

/* Using a global variable start and stop state. This classified as
   UB by standart, although, no real ordering / atomic issue in this
   case. Threads are either reading or writing to this variable and
   those operations are atomic. */
TEST_PHASES global_state = test_iterate_t;

/* This is the main worker thread that invokes the test iterations
   iteration_delay_time: amount of usec to wait after each iteration */
void worker(E_syscall_perf_test flow, unsigned int* count)
{
    char name[L_tmpnam];

    if (globalTestSchema[flow].init_func && globalTestSchema[flow].init_func(name))
    {
        std::cerr << "Test initialization went wrong\n";
        return;
    }

    while (global_state != test_end_t)
    {
        if (globalTestSchema[flow].test_func(name))
        {
            std::cerr << "Test function went wrong\n";
            goto Exit;
        }
        (*count)++;
    }

Exit:
    if (globalTestSchema[flow].uninit_func) globalTestSchema[flow].uninit_func(name);
}

void Usage(char** argv)
{
    std::cerr << "Syntax is:";
    std::cerr << "\t" << argv[0] << " <options>\n";
    std::cerr << "\t\t--verb <level> (0:no output 1:minimal 2:maximal dflt:1)\n";
    std::cerr << "\t\t--duration <in seconds> (dflt:5sec)\n";
    std::cerr << "\t\t--thread <thread number> (dflt:1)\n";
    std::cerr
        << "\t\t--freq <number iteration per second> (dflt:0, frequency zero means no delay after iteration: maximum rate)\n";
    std::cerr << "\t\t--test [Read | Write | QueryProcess | Allocate | Execdelay]\n";
}

int main(int argc, char** argv)
{
    int test_amount_of_time_insec = 5;
    int nof_threads               = 1;
    E_syscall_perf_test flow      = E_syscall_perf_test::Invalid;

    /* Parsing arguments */
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--verb"))
        {
            i++;
            verbose_level = atoi(argv[i]);
        }
        else if (!strcmp(argv[i], "--duration"))
        {
            i++;
            test_amount_of_time_insec = atoi(argv[i]);
        }
        else if (!strcmp(argv[i], "--thread"))
        {
            i++;
            nof_threads = atoi(argv[i]);
        }
        else if (!strcmp(argv[i], "--test"))
        {
            i++;

            for (t_test_schema* current = globalTestSchema; current->flow != E_syscall_perf_test::Last; current++)
            {
                if (!strcmp(argv[i], current->name))
                {
                    flow = current->flow;
                    break;
                }
            }

            /* illegal flow name - not found in test schema */
            if (flow == E_syscall_perf_test::Invalid)
            {
                std::cerr << "Unrecognized flow: " << argv[i] << std::endl;
                Usage(argv);
                return 1;
            }
        }
        else
        {
            std::cerr << "Unrecognized option: " << argv[i] << std::endl;
            Usage(argv);
            return 1;
        }
    }

    if (flow == E_syscall_perf_test::Invalid)
    {
        std::cerr << "Test not defined" << std::endl;
        Usage(argv);
        return 1;
    }

    std::vector< fastint_t > thread_counters(nof_threads);

    if (verbose_max())
    {
        std::cout << "Test parameters:\n";
        std::cout << "\t Test name: " << globalTestSchema[flow].name << std::endl;
        std::cout << "\t Threads: " << nof_threads << std::endl;
        std::cout << "\t Input test duration (sec): " << test_amount_of_time_insec << std::endl;
    }

    std::vector< std::thread > threads;
    std::chrono::seconds test_duration(test_amount_of_time_insec);

    auto start = myclock::now();

    for (int i = 0; i < nof_threads; i++)
    {
        threads.push_back(std::thread(worker, flow, &thread_counters[i].counter));
    }

    std::this_thread::sleep_for(test_duration);
    global_state = test_end_t;

    for (int i = 0; i < nof_threads; i++)
    {
        threads[i].join();
    }

    auto end = myclock::now();

    auto time_spent                  = std::chrono::duration_cast< std::chrono::microseconds >(end - start);
    unsigned int test_time_usecs     = time_spent.count();
    unsigned int nof_iterations      = 0;
    for (auto ct : thread_counters)
    {
        nof_iterations += ct.counter;
    };

    if (verbose_normal())
    {
        std::cout << "Effective test duration: " << test_time_usecs / 1000 << " msecs\n";
        std::cout << "Number of iterations processed: " << nof_iterations << std::endl;
    }

    unsigned int number_of_iteration_per_thread = nof_iterations / nof_threads;
    unsigned int avg_iteration_time_usec        = test_time_usecs / number_of_iteration_per_thread;
    if (verbose_normal())
    {
        std::cout << "Iteration delay: " << avg_iteration_time_usec << " usec\n";
    }

    return 0;
}
