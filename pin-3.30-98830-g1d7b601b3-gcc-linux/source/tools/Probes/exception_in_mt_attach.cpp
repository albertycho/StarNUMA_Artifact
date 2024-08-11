/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * This pintool attaches to an MT application.
 * It replaces the application function foo() that throws an exception.
 * The pintool verifies the replacement function did not continue beyond the call to foo since foo threw an exception.
 * The application will verify that the exception propagated properly.
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <sched.h>
using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::hex;
using std::ios;
using std::ostream;
using std::string;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name");

ostream* TraceFile = NULL;
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This pin tool tests MT attach in probe mode.\n"
            "\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

STATIC PIN_LOCK pinLock;

STATIC UINT32 threadCounter    = 0;
STATIC BOOL isAppStartReceived = FALSE;

STATIC VOID AppStart(VOID* v)
{
    PIN_GetLock(&pinLock, PIN_GetTid());
    *TraceFile << "Application Start Callback is called from thread " << dec << PIN_GetTid() << endl;
    isAppStartReceived = TRUE;
    PIN_ReleaseLock(&pinLock);
}

STATIC VOID AttachedThreadStart(VOID* sigmask, VOID* v)
{
    PIN_GetLock(&pinLock, PIN_GetTid());
    *TraceFile << "Thread counter is updated to " << dec << (threadCounter + 1) << endl;
    ++threadCounter;
    PIN_ReleaseLock(&pinLock);
}

STATIC int ThreadsReady_replacement(unsigned int numOfThreads)
{
    PIN_GetLock(&pinLock, PIN_GetTid());

    // Check that we don't have any extra thread
    ASSERTX(threadCounter <= numOfThreads);

    // If all the threads have attached then return 1 - it will allow the application to continue
    if ((threadCounter == numOfThreads) && isAppStartReceived)
    {
        if (TraceFile != &cout) delete TraceFile;
        PIN_ReleaseLock(&pinLock);
        return 1;
    }
    PIN_ReleaseLock(&pinLock);
    return 0;
}

typedef int (*foo_t)();

STATIC int foo_replacement(foo_t orig_foo)
{
    printf("%s called\n", __FUNCTION__);

    int res = orig_foo();

    ASSERT(FALSE, "We should not get here since there was an exception in foo()");
    return res;
}

STATIC VOID ImageLoad(IMG img, void* v)
{
    if (IMG_IsMainExecutable(img))
    {
        RTN rtn = RTN_FindByName(img, "ThreadsReady");
        ASSERTX(RTN_Valid(rtn));
        ASSERTX(RTN_ReplaceProbed(rtn, AFUNPTR(ThreadsReady_replacement)) != NULL);

        rtn = RTN_FindByName(img, "foo");
        ASSERTX(RTN_Valid(rtn));
        PROTO foo_proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "foo", PIN_PARG_END());
        ASSERTX(RTN_ReplaceSignatureProbed(rtn, (AFUNPTR)foo_replacement, IARG_PROTOTYPE, foo_proto, IARG_ORIG_FUNCPTR,
                                           IARG_END) != NULL);
    }
}

/* ===================================================================== */

int main(int argc, CHAR* argv[])
{
    PIN_InitSymbols();

    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    TraceFile = KnobOutputFile.Value().empty() ? &cout : new std::ofstream(KnobOutputFile.Value().c_str());
    *TraceFile << hex;
    TraceFile->setf(ios::showbase);

    PIN_InitLock(&pinLock);

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddApplicationStartFunction(AppStart, 0);
    PIN_AddThreadAttachProbedFunction(AttachedThreadStart, 0);
    PIN_StartProgramProbed();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
