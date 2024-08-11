/*
 * Copyright (C) 2009-2021 Intel Corporation.
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
#include <assert.h>

static int(__fastcall* pBar)(int, int, int, int, int, int, int) = 0;

int __fastcall bar(int a0, int a1, int a2, int a3, int a4, int a5, int a6) { return a0 + a1 + a2 + a3 + a4 + a5 + a6; }

__declspec(dllexport) int __fastcall foo(int a0, int a1, int a2, int a3, int a4, int a5, int a6)
{
    // May cause exception due to NULL pointer
    return pBar(a0, a1, a2, a3, a4, a5, a6);
}

int main()
{
    int i;

    __try
    {
        i = foo(1, 2, 3, 4, 5, 6, 0); // expected result 21, foo will throw an exception
        assert(0); // if we get here the application will abort. However we should not ge here since foo throws an exception.
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // If Pin translated probed code properly, exception will reach the handler
        printf("Exception %08X\n", (unsigned long)GetExceptionCode());
        fflush(stdout);
    }

    pBar = bar;

    __try
    {
        i = foo(1, 2, 3, 4, 5, 6, 7); // expected result 28, foo will not throw an exception
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // No exception expected
        printf("Exception %08X\n", (unsigned long)GetExceptionCode());
        fflush(stdout);
        assert(
            0); // if we get here the application will abort. However we should not ge here since foo did not throw an exception.
    }

    printf("result=%d\n", i);
    fflush(stdout);

    return 0;
}
