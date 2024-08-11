/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

extern "C" void SetMxcsr(unsigned);
extern "C" unsigned GetMxcsr();

unsigned int get_mxcsr()
{
    unsigned int val = -1;

#ifdef TARGET_WINDOWS
#ifdef TARGET_IA32

    __asm { stmxcsr [val] }

#else

    return GetMxcsr();

#endif
#else // ~TARGET_WINDOWS

    asm("stmxcsr %0" : "=m"(val));

#endif // ~TARGET_WINDOWS

    return val;
}

void set_mxcsr(unsigned int mxcsr_val)
{
#ifdef TARGET_WINDOWS
#ifdef TARGET_IA32

    __asm { ldmxcsr [mxcsr_val] }

#else

    SetMxcsr(mxcsr_val);

#endif
#else // ~TARGET_WINDOWS

    asm("ldmxcsr %0" : : "m"(mxcsr_val));

#endif // ~TARGET_WINDOWS
}
