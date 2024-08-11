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

static int (*pBar)() = 0;

int bar() { return 0; }
extern "C" DLLEXPORT int foo()
{
#if defined(TARGET_LINUX) || defined(TARGET_MAC)
    if (!pBar) throw(0);
#endif
    // May cause exception due to NULL pointer
    return pBar();
}

int main()
{
    int i = 2;

    try
    {
        MyClass ins;
        i = foo();     // foo will throw an exception
        assert(false); // if we get here the application will abort. However we should not ge here since foo rhrows an exception.
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
        i = foo(); // foo will not throw an exception
    }
    catch (...)
    {
        // No exception expected
        printf("Exception 2 caught\n");
        assert(
            false); // if we get here the application will abort. However we should not ge here since foo did not throw an exception.
    }

    return i;
}
