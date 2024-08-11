/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * this application calls a user-written assembly routine short_func
 */

void short_func();
void short_func_terminator();

int main(int argc, char* argv[])
{
    short_func();
    short_func_terminator();
    return 0;
}
