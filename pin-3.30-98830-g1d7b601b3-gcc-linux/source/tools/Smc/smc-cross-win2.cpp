/*
 * Copyright (C) 2008-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

 /*! @file
  *  This application opens shared memory with read-execute permissions
  *  and maps its view with these permissions.
  *  It runs code in the area and notifies parent process.
  *  It is expected the parent process will eventually modify the code.
  *  Then current process repeats run of the code.
  *  Native x86 application will run modified code due to HW support of SMC.
  *  It is expected that instrumented application will also detect cross-SMC
  *  in shared memory and will run re-translated modified code.
  */
#include <Windows.h>

int main(int argc, char* argv[])
{
	HANDLE hMap = OpenFileMappingA(FILE_MAP_EXECUTE | FILE_MAP_READ, FALSE, "cross-smc-region");
	if (NULL == hMap) return 12;
	// Map all shared memory as READONLY-EXECUTE
	char* volatile fAddr = (char*)MapViewOfFile(hMap, FILE_MAP_EXECUTE | FILE_MAP_READ, 0, 0, 0);
	if (NULL == fAddr) return 13;

	int res;
	for (int state = 0; state < 3; ++state)
	{
		// This is single call to potentially modified function in the process,
		// so we expect the corresponding trace to be translated only once
		// and re-translation could be caused only by cross-SMC detection.
		res = ((int (*) ())fAddr)() & 0xff;	// Result returned in AL register
		// Initial return value is 1. Modified code returns 0.
		// Note that last invocation (in state 2) is guaranteed after code modification in parent process.
		// Return value of this last invocation is used as exit code of the process and test success measure.

		switch (state)
		{
		case 0:
			{
				// Notify parent process.
				HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "cross-smc-event");
				if (NULL == hEvent) return 14;
				SetEvent(hEvent);
				CloseHandle(hEvent);
			}
			break;
		case 1:
			// Wait for parent process to write to shared memory.
			do
			{
				Sleep(100);
			} while (0x01 == fAddr[1]); // This byte was originally set in parent process to 1.
			// The byte was modified in parent process to 0.
			// This should change return value of the modified code from 1 to 0.
			// Subsequent invocation of the instrumented original shared code in next iteration
			// should cause re-translation and run of modified code.
			break;
		}
	}
	// Exit once code memory was modified and this code was executed.

	UnmapViewOfFile(fAddr);
	CloseHandle(hMap);

	return res;	// Success if result is 0. Result 1 means Pin didn't detect cross-SMC.
}
