/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

using myclock = std::chrono::high_resolution_clock;
// using myclock = std::chrono::steady_clock;

/* test iteration functions */
int init_write(char* fname);
void uninit_write(char* fname);
unsigned int one_iteration_write(const char* fname);
int init_write_uio(char* fname);
void uninit_write_uio(char* fname);
unsigned int one_iteration_write_uio(const char* fname);
int init_read(char* fname);
unsigned int one_iteration_read(const char* fname);
int init_read_uio(char* fname);
unsigned int one_iteration_read_uio(const char* fname);
unsigned int one_iteration_allocatevirtualmemory(const char* fname);
unsigned int one_iteration_execdelay(const char* fname);

#ifdef TARGET_WINDOWS
unsigned int one_iteration_queryprocess(const char* fname);
#endif
