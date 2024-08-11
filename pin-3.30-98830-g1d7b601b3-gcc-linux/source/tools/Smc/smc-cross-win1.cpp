/*
 * Copyright (C) 2008-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

 /*! @file
  *  This application creates shared memory with read-write-execute permissions
  *  and maps its view with read-write permissions.
  *  It writes code to the area and creates process that runs this code.
  *  Once the run confirmed, current process modifies the code.
  *  It is expected the child process will eventually run modified code.
  *  The current process returns exit code of the child process.
  */
#include <Windows.h>

int main(int argc, char* argv[])
{
	HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE | SEC_COMMIT,
		                             0, 0x10000, "cross-smc-region"); // Min allocation size is 64KB
	if (NULL == hMap) return 2;
	// Map all shared memory as READ-WRITE
	char* fAddr = (char*)MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, 0);
	if (NULL == fAddr) return 3;

	// Write function in shared memory.
	// mov AL, 1	; B0 01
	// ret			; C3
	fAddr[0] = 0xB0;
	fAddr[1] = 0x01;
	fAddr[2] = 0xC3;

	// Create event
	HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, "cross-smc-event");
	if (NULL == hEvent) return 4;

	STARTUPINFOA si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION processInfo;
	memset(&processInfo, 0, sizeof(processInfo));

	char cmdLine[] = "smc-cross-win2.exe";
	// Create process that will run the code in shared memory.
	BOOL ok = CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL,
		                     &si, &processInfo);
	if (!ok) return 5;
	CloseHandle(processInfo.hThread);

	// Wait for event up to 10 seconds
	if (WAIT_OBJECT_0 != WaitForSingleObject(hEvent, 10000)) return 6;
	CloseHandle(hEvent);

	// Modify shared code
	fAddr[1] = 0x00;	// Replace mov AL, 1 with mov AL, 0

	WaitForSingleObject(processInfo.hProcess, INFINITE);
	DWORD exitCode = 0;
	GetExitCodeProcess(processInfo.hProcess, &exitCode);

	UnmapViewOfFile(fAddr);
	CloseHandle(hMap);

	return exitCode;	// Return exit code of child process
}
