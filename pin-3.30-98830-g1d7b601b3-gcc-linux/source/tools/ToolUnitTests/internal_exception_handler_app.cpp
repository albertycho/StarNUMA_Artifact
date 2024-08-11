/*
 * Copyright (C) 2009-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 * This application verifies that pin tool can correctly emulate exceptions on behalf of the application
 */

#include <string>
#include <iostream>
#include <fpieee.h>
#include <excpt.h>
#include <float.h>

#include "windows.h"

using std::cout;
using std::endl;
using std::flush;
using std::hex;
using std::string;

//==========================================================================
// Printing utilities
//==========================================================================
string FunctionTestName;

static void EndFunctionTest(const string& functionTestName)
{
        cout << "internal_exception_handler_app"
         << "[" << functionTestName << "] Success" << endl
             << flush;
}

static void Abort(const string& msg, const string& functionTestName)
{
    cout << "internal_exception_handler_app"
         << "[" << functionTestName << "] Failure: " << msg << endl
         << flush;
    exit(1);
}

//==========================================================================
// Exception handling utilities
//==========================================================================

/*!
 * Exception filter for the ExecuteSafe function: copy the exception record 
 * to the specified structure.
 * @param[in] exceptPtr        pointers to the exception context and the exception 
 *                             record prepared by the system
 * @param[in] pExceptRecord    pointer to the structure that receives the 
 *                             exception record
 * @return the exception disposition
 */
static int SafeExceptionFilter(LPEXCEPTION_POINTERS exceptPtr) { return EXCEPTION_EXECUTE_HANDLER; }

static int SafeExceptionFilterFloating(_FPIEEE_RECORD* pieee) { return EXCEPTION_EXECUTE_HANDLER; }

/*!
 * Execute the specified function and return to the caller even if the function raises 
 * an exception.
 * @param[in] pExceptRecord    pointer to the structure that receives the 
 *                             exception record if the function raises an exception
 * @return TRUE, if the function raised an exception
 */
template< typename FUNC > bool ExecuteSafe(FUNC fp, DWORD* pExceptCode)
{
    __try
    {
        fp();
        return false;
    }
    __except (*pExceptCode = GetExceptionCode(), SafeExceptionFilter(GetExceptionInformation()))
    {
        return true;
    }
}

#pragma float_control(except, on)
template< typename FUNC > bool ExecuteSafeFloating(FUNC fp, DWORD* pExceptCode)
{
    unsigned int currentControl;
    errno_t err = _controlfp_s(&currentControl, ~_EM_ZERODIVIDE, _MCW_EM);
    __try
    {
        fp();
        return false;
    }
    __except (*pExceptCode = GetExceptionCode(), SafeExceptionFilter(GetExceptionInformation()))
    {
        return true;
    }
}

/*!
* The tool replaces this function and raises the EXCEPTION_INT_DIVIDE_BY_ZERO system exception. 
*/
void RaiseIntDivideByZeroException()
{
    volatile int zero = 0;
    volatile int i    = 1 / zero;
}

/*!
* The tool replaces this function and raises the EXCEPTION_FLT_DIVIDE_BY_ZERO system exception. 
*/
void RaiseFltDivideByZeroException()
{
    volatile float zero = 0.0;
    volatile float i    = 1.0 / zero;
}

/*!
 * The tool replace this function and raises the specified system exception. 
 */
void RaiseSystemException(unsigned int sysExceptCode) { RaiseException(sysExceptCode, 0, 0, NULL); }

/*!
 * Check to see if the specified exception record represents an exception with the 
 * specified exception code.
 */
static bool CheckExceptionCode(DWORD exceptCode, DWORD expectedExceptCode)
{
    if (exceptCode != expectedExceptCode)
    {
        cout << "Unexpected exception code " << hex << exceptCode << ". Should be " << hex << expectedExceptCode << endl << flush;
        return false;
    }
    return true;
}

/*!
 * The main procedure of the application.
 */
int main(int argc, char* argv[])
{
    DWORD exceptCode;
    bool exceptionCaught;
    const char *int_div_zero  = "Raise int divide by zero in the tool";
    const char *flt_div_zero  = "Raise FP divide by zero in the tool";

    // Raise int divide by zero exception in the tool
    exceptionCaught = ExecuteSafe(RaiseIntDivideByZeroException, &exceptCode);
    if (!exceptionCaught)
    {
        Abort("Unhandled exception", int_div_zero);
    }
    if (!CheckExceptionCode(exceptCode, EXCEPTION_INT_DIVIDE_BY_ZERO))
    {
        Abort("Incorrect exception information (EXCEPTION_INT_DIVIDE_BY_ZERO)", int_div_zero);
    }
    EndFunctionTest(int_div_zero);

    // Raise flt divide by zero exception in the tool

    exceptionCaught = ExecuteSafeFloating(RaiseFltDivideByZeroException, &exceptCode);
    if (!exceptionCaught)
    {
        Abort("Unhandled exception", flt_div_zero);
    }
    if (EXCEPTION_FLT_DIVIDE_BY_ZERO != exceptCode && STATUS_FLOAT_MULTIPLE_TRAPS != exceptCode)
    {
        CheckExceptionCode(exceptCode, EXCEPTION_FLT_DIVIDE_BY_ZERO); // For reporting only
        Abort("Incorrect exception information (EXCEPTION_FLT_DIVIDE_BY_ZERO)", flt_div_zero);
    }
    EndFunctionTest(flt_div_zero);

    cout << "internal_exception_handler_app"
         << " : Completed successfully" << endl
         << flush;

    return 0;
}
