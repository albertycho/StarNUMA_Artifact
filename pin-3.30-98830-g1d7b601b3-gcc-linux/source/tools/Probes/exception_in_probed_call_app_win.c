/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
  This C application (Windows only) calls foo() inside a __try-__except block twice.
  The first call will throw an exception which is caught and handled.
  The second call should not throw an exception.
  The application verifies we did not execute the remaining code in the try block past the exception.
  and that we did not get into the __except block if there was no exception.
  In addition it has printouts that are validated at the end of the run.
*/

#include <windows.h>
#include <stdio.h>

static int (*pBar)() = 0;

int bar() { return 0; }

__declspec(dllexport) int foo()
{
    // May cause exception due to NULL pointer
    return pBar();
}

int main()
{
    int i;

    __try
    {
        i = foo(); // foo will throw an exception
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // If Pin translated probed code properly, exception will reach the handler
        printf("Exception %08X\n", (unsigned long)GetExceptionCode());
    }

    pBar = bar;

    __try
    {
        i = foo(); // foo will not throw an exception
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // No exception expected
        printf("Exception %08X\n", (unsigned long)GetExceptionCode());
    }

    return i;
}
