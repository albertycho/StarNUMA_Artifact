#
# Copyright (C) 2023-2023 Intel Corporation.
# SPDX-License-Identifier: MIT
#

Performance tests description:
-----------------------------

Table of contents:
-----------------

1. Introduction
2. Methodology
3. System call tests
4. JIT test
5. Inline test
6. Using python script goperf.py to run the test

1. Introduction:
   ------------
   Performance tests in this directory are designed to measure the overhead of PIN in different scenarios
   and flows. The first flow to be investigated below is the system call flow.
   We first start by describing the common methodology for all the tests presented here. We then proceed 
   with the different test categories and their descriptions.

2. Methodology:
   -----------
   The goal of the performance task force is to analyze the efficiency of PIN and improve it accordingly.
   To do so we have selected a set of scenarios and flows that seems important to us. Each of the subsequent
   chapters explorates those flows. The high level method is to run a test application with and without
   PIN. We measure a metric described later on and compare both results. Usually the PIN metric will be higher
   than the one without PIN. So we finally present a ratio between the two results, the higher the value,
   the greater the overhead due to PIN.
   Although since those tests may have some variation, and since the resulting metric may vary as well.
   In order to adress this issue each test runs for a certain amount of time (defaulted to five seconds).
   This amount of time must be significant enough in order for the process to reach a steady state. In
   addition the metric is calculated for each of those five run, the maximum and minimum metric are removed
   from the set while the three remaining are averaged and finally supplies the result.
   In addition, a standard deviation of the result is calculated in order to appreciate the confidence
   of the result.
   A python script goperf.py was coded to perform the above.

3. System call tests:
   -----------------
   (a) Background:
       ----------
       The system call flow in PIN is an important one since, any system call is being intercepted by PIN,
       the VM is invoked and PIN is doing some work to handle it. Those system calls are handled in two 
       different manners: the dispatch and the emulation manner. The dispatch finally gets back to the 
       code cache where the system call is being processed. The emulation manner means PIN run the call
       in its context returns and proceeds when it is done. The list of emulated system calls are listed 
       in files vm_ia32_l/emu_ia32_linux.H and vm_ia32_w/emu_ia32_windows.H
       The overhead involved is suspected to be significant.
       For Windows, we have selected five system calls:
       1. Read: fread (mixed with fopen and fclose)
       2. Write: fwrite mixed with fopen and fclose)
       3. Execdelay: NtDelayExecution
       4. QueryProcess: NtQueryProcessInformation (Windows only test)
       5. Allocate: NtAllocateVirtualMemory (NtFreeVirtualMemory)
       6. ReadUIO: fread w/o fopen and fclose and with unbuffered I/O
       7. WriteUIO: fwrite w/o fopen and fclose and with unbuffered I/O
   
       (1) (2) are dispatched system calls (Windows and Linux)
       (3) is a Windows dispatch system call
       (4) is a Windows emulation system call
       (5) is an emulation system calls
   
   (b) The tests:
       ---------
       1. Read:
          A temporary file name is extracted is initialized with a 64 bytes content.
          This file is opened for read, its content is read and it is then closed. This iteration is done
          again during approximately a five seconds period. This process is done with 1, 2, 4, 6 and 8 
          threads. The number of iterations is counted and the average iteration time is calculated.
          This is the metric of this test.
       2. Write:
          The same is done with the fwrite system call. Same, metric is the average iteration time.
       3. Execdelay:
          The Sleep(0) API is called in one iteration thread again and again. The iterations are counted as
          before and the average iteration time is calculated. This is the metric of this test.
       4. Allocate:
          VirtualAlloc API is called and then VirtualFree in a one iteration thread. The size is 4 pages
          and MEM_COMMIT is set as the memory type parameter. On Linux, mmap and munmap APIs are used.
          The iterations are counted and the average iteration time is calculated. This is the metric of 
          this test.
       5. QueryProcess:
          This is a Windows only test.
          GetProcessAffinityMask API is called in one iteration thread again and again. The iterations 
          are counted and the average iteration time is calculated. This is the metric of this test.
       6. ReadUIO: same as Read test but w/o opening and closing a file handle in the iteration and with
          unbuffered I/O
       7. WriteUIO: same as Write but w/o opening and closing a file handle in the iteration and with
          unbuffered I/O
       
   Remarks:
   -------
   (i) NtDelayExecution is achieved via a call to Sleep() Windows API
   (ii) NtAllocateVirtualMemory is achieved via a call to VirtualAlloc() Windows API
        NtVirtualFree via VirtualFree()
   (ii) NtQueryInformation is achieved via a call to GetProcessAffinityMask() Windows API
   (iv) The test uses a tool library that does nothing right now. This may change in the future.
   (v) Some tests such as NtDelayExecution use an iteration function that by itself loops many.
       This is due since otherwise a single iteration goes below the 1 usec delay which causes
       the calculation and accuracy to be altered.
   
   syscall_app.cpp is the test application that will exercise those five flows.
   The method works in the following way: those system calls are invoked iteratively in a function
   that can run in several threads in parallel. Those iterations are done again and again during the 
   test interval time (five seconds default). After that amount of time, the test terminates and we count 
   the number of iteration processed. The final result (and metric) being the average time of a single
   iteration.
   We use c++ std::chrono::high_resolution_clock to have a final measure of the time elapsed, this method
   is good to have a portable code among platforms.

4. JIT test:
   --------
   (a) Background:
       ----------
       The jitting process is a significant part of PIN's efficiency; the binary code is being fetched by
       PIN, it is analyzed processed dumped to the code cache and executed from there. The binary code process
       includes: code segment partitionnong into traces and BBLs, register allocation, code transformation
       and dumping the resulting binary into the code cache. Jitting can be affected by many sub scenarios
       and instrumentation requests; we want to address the simplest and most common case where 
       instrumentation is not part of the flow. The purpose of this test is to measure the overhead of the
       jitting process. 

   (b) The test:
       --------
       For achieving this, we use no tool. Secondly this process is supposed to be done once, later on it 
       is reused when execution is going through a code area that has already been jitted. Therefore, we 
       need a big enough flow where each instruction is executed only once.
       The technique used to generate a big binary executable is by using template class recursive compilation
       with specialization. The template parameter is an integer and the structure defines a method that
       calls the same template with a decremented by one parameter. This generates, with 8 lines of code, about
       4MB binary. We execute the resulting application with and without PIN. The metric will be the ratio
       of the time spent between the two sessions.

5. Inline test:
   -----------
   (a) Background:
       ----------
       While jitting and instrumenting code, PIN has the feature to insert analysis routines into the original
       binary. This feature is optimized by PIN, inlining the analysis routine instead of inserting
       a call instruction. This routine is located within the tool dynamic library.
       For various reasons and specific situations, PIN gives this optimization up, causing the analysis
       routine to be invoked via the call instruction.
       For example, if the analysis routine has a nested call or if it uses xmm registers, this optimization
       is disabled. There is a good chance that disabling inlining severly affects performance. We would 
       like to measure it.

   (b) The test:
       --------
       The goal is to generate a lot of analysis routine calls. For achieving this we do not need to invent
       a new application or/and a new tool; we will use the cp-pin.exe utility application and the inscount0.dll
       tool that counts the number of instructions executed. This tool inserts (after each instruction) a
       routine that increment a global counter by one. The command looks the following:
       "pin -t inscount0.dll -- cp-pin.exe <some file> <another file>".
       The analysis routine that counts is simple enough and is always inlined.
       In order to disable inlining, we use the PIN switch option: '-inline 0' that exactly does what we
       need. Finally we execute the two following commands:
        $ pin -t inscount0.dll -- cp-pin.exe <some file> <another file>
        $ pin -inline 0 -t inscount0.dll -- cp-pin.exe <some file> <another file>
       We measure in microseconds the amount of time spent in the copy application.
       The metric is the ratio of the two delays obtained.

6. Using python script goperf.py to run the test:
   ---------------------------------------------
   goperf.py was written to have a direct invocation of the performance tests. This script will used
   later on for the automation. Also see goperf.py --help for additional information.
   The result of running this script is two table1 of the form:
   
   cmd1 mode results:
   Test=Write           ThreadNum    Avg(usec)    Stddev
                             1          933       1.1
                             2         1144       0.0
                             .....
   cmd2 mode results:
   Test=Write           ThreadNum    Avg(usec)    Stddev
                             1          600       0.1
                             2         1723       2.4
                             .....

    The first table describes the results for the PIN session while the second describes those for
    the native session.

    Before using goperf.py, you need to build the three tests being used: goperf.py will
    invoke the binary executables and libraries. In order to build you need to do the following:
    ...perf > make DEBUG=1 test
    
   (1) Running all the tests listed above in 32 bits architecture:
   PinTools/perf > python goperf.py
   (2) Running all the tests listed above in 64 bits architecture:
   PinTools/perf > python goperf.py --target intel64
   (3) Running a single test in 32 bits architecture:
   PinTools/perf > python goperf.py --target intel64 --test Write
   (4) Running all the tests with only 4 threads in 32 bits architecture:
   PinTools/perf > python goperf.py --target intel64 --threads 4
   (5) Change test default duration from default five seconds to 10 seconds: use --duration 10
   (6) Change test default number of trials from default five seconds to 8: use --numtry 8


See results and graph in folder:
...\Intel Corporation\Binary Instrumentation Team (BIT) - public - Pin\Design\Performance\perf.xlsx

