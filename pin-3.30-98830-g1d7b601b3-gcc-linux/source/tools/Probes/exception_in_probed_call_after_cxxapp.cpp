/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
  This application calls foo() inside a try-catch block twice.
  The first call will throw an exception which is caught and handled.
  The second call should not throw an exception.
  The application verifies we did not execute the remaining code in the try block past the exception.
  and that we did not get into the catch block if there was no exception.
  In addition it has printouts that are validated at the end of the run.
*/

#ifdef TARGET_WINDOWS
#include <windows.h>
#endif
#include <stdio.h>
#include <assert.h>

#ifdef TARGET_WINDOWS
#define FASTCALL __fastcall
#define DLLEXPORT __declspec(dllexport)
#else
#define FASTCALL
#define DLLEXPORT
#endif

bool destructed = false;

// cpp exceptions - Exercise windows exception mechanism
class MyClass
{
  public:
    ~MyClass() { destructed = true; }
};

static int(FASTCALL* pBar)(int, int, int, int, int, int, int) = 0;

int FASTCALL bar(int a0, int a1, int a2, int a3, int a4, int a5, int a6) { return a0 + a1 + a2 + a3 + a4 + a5 + a6; }

extern "C" DLLEXPORT int FASTCALL foo(int a0, int a1, int a2, int a3, int a4, int a5, int a6)
{
#if defined(TARGET_LINUX) || defined(TARGET_MAC)
    if (!pBar) throw(0);
#endif
    // May cause exception due to NULL pointer
    return pBar(a0, a1, a2, a3, a4, a5, a6);
}

int main()
{
    int i = 2;

    try
    {
        MyClass ins;
        i = foo(1, 2, 3, 4, 5, 6, 0); // expected result 21, foo will throw an exception
        assert(false); // if we get here the application will abort. However we should not ge here since foo throws an exception.
    }
    catch (...)
    {
        // If Pin translated probed code properly, exception will reach the handler
        printf("Exception 1 caught\n");
    }

    // Check that destructor was called when exception was handled
    assert(destructed);

    pBar = bar;

    try
    {
        i = foo(1, 2, 3, 4, 5, 6, 7); // expected result 28, foo will not throw an exception
    }
    catch (...)
    {
        // No exception expected
        printf("Exception 2 caught\n");
        assert(
            false); // if we get here the application will abort. However we should not ge here since foo did not throw an exception.
    }

    printf("result=%d\n", i);
    return 0;
}
