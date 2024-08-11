/*
 * Copyright (C) 2011-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
  This application throws exceptions inside a try-catch block.
  The first exception is thrown directly by foo1().
  The second exception is thrown by ExcInDll() which resides in a separate library and is called by foo2().
  The pintool will instrument the functions from which the exceptions are thrown.
  The application verifies the following:
  o The code in the try block after the call to foo1 does not execute
  o Stack pointers to remain intact throughout exceptions.
  o Destructors are activated for objects that are inside the scope of the try block
*/
#include <stdio.h>
#include <assert.h>

extern "C" unsigned long getstack();
extern "C" unsigned long getebp();

bool destructed = false;

// cpp exceptions - Exercise windows exception mechanism
class MyClass
{
  public:
    ~MyClass() { destructed = true; }
};

extern "C" void ExcInDll();
static int (*pBar)() = 0;

int bar() { return 0; }
extern "C" void foo1()
{
    // May cause exception due to NULL pointer
    if (!pBar) throw(0);
    pBar();
}

extern "C" void foo2() { ExcInDll(); }

static bool verify_stack_pointers(unsigned long stackPtr1, unsigned long stackPtr2, unsigned long framePtr1,
                                  unsigned long framePtr2)
{
    if ((stackPtr1 != stackPtr2) || (framePtr1 != framePtr2))
    {
        printf("Error: expected identical rsp and rbp before and after try-catch block. ; "
               "rsp before 0x%x ; after 0x%x ; "
               "rbp before 0x%x ; after 0x%x \n",
               stackPtr1, stackPtr2, framePtr1, framePtr2);
        return false;
    }
    return true;
}

//
// main
//
int main()
{
    // Test 1:
    // foo1 will throw an exception.
    // We expect the destructor of MyClass to be called.
    // We expect not to execute the code in the try block after the call to foo1.
    // We expect the stack pointers to remain intact.
    unsigned long stackBefore    = getstack();
    unsigned long framePtrBefore = getebp();
    try
    {
        MyClass ins;
        foo1();        // will throw an exception
        assert(false); // if we get here the application will abort. However we should not ge here since foo throws an exception.
    }
    catch (...)
    {
        // If Pin translated probed code properly, exception will reach the handler
        printf("Exception caught\n");
    }
    unsigned long stackAfter    = getstack();
    unsigned long framePtrAfter = getebp();
    if (!verify_stack_pointers(stackBefore, stackAfter, framePtrBefore, framePtrAfter))
    {
        return -1;
    }
    // Check that destructor was called when exception was handled
    assert(destructed);

    // Test 2:
    // foo2 will throw an exception.
    // Local variables are stored in registers.
    // We expect the value of the registers to be restored correctly after the exception.
    // We expect the stack pointers to remain intact.
    int param1           = 1;
    int param2           = param1 * 5;
    float param3         = 0.5 * 1.5;
    float expectedResult = 0;
    try
    {
        expectedResult = param3 + param2 + param1;
        foo2(); // Will throw an exception
    }
    catch (...)
    {
        float afterCatchResult = param3 + param2 + param1;
        if (afterCatchResult != expectedResult)
        {
            printf("expectedResult = %f; afterCatchResult = %f\n", expectedResult, afterCatchResult);
            printf("Registers may be restored incorrectly in the catch block\n");
            return -1;
        }
        else
        {
            printf("Try-catch works correctly while exception propagation from dll\n");
        }
    }
    stackAfter    = getstack();
    framePtrAfter = getebp();
    if (!verify_stack_pointers(stackBefore, stackAfter, framePtrBefore, framePtrAfter))
    {
        return -1;
    }

    pBar = bar;

    try
    {
        foo1(); // will not throw an exception
    }
    catch (...)
    {
        // No exception expected
        printf("Unexpected exception\n");
        return -1;
    }

    return 0;
}
