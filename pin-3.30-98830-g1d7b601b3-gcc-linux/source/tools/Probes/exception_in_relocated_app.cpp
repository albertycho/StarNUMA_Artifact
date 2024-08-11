/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
  This application calls foo() inside a try-catch block.
  foo has an assembly pattern that makes it impossible to place a probe on the top of the pattern.
  This is done in order to force the relocation of foo.
  Then foo throws an exception which is caught and handled.
*/

#ifdef TARGET_WINDOWS
#include <windows.h>
#endif
#include <stdio.h>
#include <assert.h>
#include <iostream>

static void foo()
{
    // This assembly creates a pattern that does not allow placing a probe on the first instruction unless relocated.
    // The instruction in L2 is outside the probe area but it is a branch into the probe area (L1).
    __asm__("L0:    jmp L2;"
            "       nop;"
            "L1:    jmp L3;"
            "       nop;"
            "L2:    jmp L1;"
            "L3:    nop;");

    throw(0);
    return;
}

bool destructed = false;

// cpp exceptions - Exercise windows exception mechanism
class MyClass
{
  public:
    ~MyClass() { destructed = true; }
};

int main()
{
    try
    {
        MyClass ins;
        foo();
        assert(false); // if we get here the application will abort. However we should not ge here since foo throws an exception.
    }
    catch (...)
    {
        // If Pin translated probed code properly, exception will reach the handler
        printf("Exception caught\n");
    }

    // Check that destructor was called when exception was handled
    assert(destructed);
    return 0;
}
