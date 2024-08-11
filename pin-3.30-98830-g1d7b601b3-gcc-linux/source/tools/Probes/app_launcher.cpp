/*
 * Copyright (C) 2022-2022 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/syscall.h>

/* 
 * This application is originally copied from one of VTune tests.
 * It is testing the scenario of one application launching another application using different APIs.
 */

static bool verbose = true;

/***************************************************************************/

typedef enum _Api_t
{
    API_EXEC = 0,
    API_FORK_EXEC,
    API_VFORK_EXEC_SINGLE,
    API_VFORK_EXEC_MULTIPLE,
    API_SYSTEM,
    API_POPEN
} Api_t;

typedef struct _ThreadInfo_s
{
    Api_t api;
    char* pExtProgPath;
} ThreadInfo_s;


void* thread(void* arg) { return 0; }

static void print_ppid_pid_tid()
{
    if (verbose) printf("ppid %u ; pid %u ; tid %lu ; \t", getppid(), getpid(), syscall(SYS_gettid));
}

/* test: fork + exec */
int testForkExec(char* pExtProgPath)
{
    print_ppid_pid_tid();
    if (verbose) printf("fork()\n");

    if (fork())
    {
        int res;
        pthread_t tid1, tid2, tid3;

        pthread_create(&tid1, NULL, thread, (void*)1);
        pthread_create(&tid2, NULL, thread, (void*)2);
        pthread_create(&tid3, NULL, thread, (void*)3);

        pthread_join(tid1, NULL);
        pthread_join(tid2, NULL);
        pthread_join(tid3, NULL);

        wait(&res);
    }
    else
    {
        char* args[] = {pExtProgPath, NULL};

        print_ppid_pid_tid();
        if (verbose) printf("execv( \"%s\" )\n", pExtProgPath);

        if (execv(pExtProgPath, args) == -1)
        {
            if (verbose) fprintf(stderr, "%d:%s\n", errno, strerror(errno));
            return 1;
        }
    }
    return 0;
}

/***************************************************************************/
/* test: vfork + exec - single thread */

int testVforkExecSingle(char* pExtProgPath)
{
    print_ppid_pid_tid();
    if (verbose) printf("vfork()\n");

    if (vfork())
    {
        int res;

        print_ppid_pid_tid();
        if (verbose) printf("In parent \n");

        wait(&res);
    }
    else
    {
        print_ppid_pid_tid();
        if (verbose) printf("In child \n");
        if (verbose) printf("execve( \"%s\" )\n", pExtProgPath);

        char* args[] = {pExtProgPath, NULL};
        if (execve(pExtProgPath, args, NULL) == -1)
        {
            if (verbose) fprintf(stderr, "%d:%s\n", errno, strerror(errno));
        }
    }
    return 0;
}

/***************************************************************************/
/* test: vfork + exec - multiple threads */

int testVforkExecMultiple(char* pExtProgPath)
{
    print_ppid_pid_tid();
    if (verbose) printf("vfork()\n");

    if (vfork())
    {
        print_ppid_pid_tid();
        if (verbose) printf("In parent \n");

        int res;
        pthread_t tid1, tid2, tid3;
        pthread_create(&tid1, NULL, thread, (void*)1);
        pthread_create(&tid2, NULL, thread, (void*)2);
        pthread_create(&tid3, NULL, thread, (void*)3);

        pthread_join(tid1, NULL);
        pthread_join(tid2, NULL);
        pthread_join(tid3, NULL);

        wait(&res);
    }
    else
    {
        print_ppid_pid_tid();
        if (verbose) printf("In child \n");
        if (verbose) printf("execve( \"%s\" )\n", pExtProgPath);

        char* args[] = {pExtProgPath, NULL};
        if (execve(pExtProgPath, args, NULL) == -1)
        {
            fprintf(stderr, "%d:%s\n", errno, strerror(errno));
            return 1;
        }
    }
    return 0;
}

/***************************************************************************/

void* threadFunc(void* arg)
{
    long retVal               = 0;
    FILE* pFile               = NULL;
    ThreadInfo_s* pThreadInfo = (ThreadInfo_s*)arg;
    char* args[]              = {pThreadInfo->pExtProgPath, NULL};

    switch (pThreadInfo->api)
    {
        case API_EXEC:
            /* test: exec */
            print_ppid_pid_tid();
            printf("execv( \"%s\" )\n", pThreadInfo->pExtProgPath);

            if (-1 == execv(pThreadInfo->pExtProgPath, args))
            {
                fprintf(stderr, "\n\nError: %d:%s\n\n", errno, strerror(errno));
                retVal = 1;
            }
            break;

        case API_FORK_EXEC:
            retVal = testForkExec(pThreadInfo->pExtProgPath);
            break;

        case API_VFORK_EXEC_SINGLE:
            retVal = testVforkExecSingle(pThreadInfo->pExtProgPath);
            break;

        case API_VFORK_EXEC_MULTIPLE:
            retVal = testVforkExecMultiple(pThreadInfo->pExtProgPath);
            break;

        case API_SYSTEM:
            /* test: system */
            print_ppid_pid_tid();
            if (verbose) printf("system( \"%s\" )\n", pThreadInfo->pExtProgPath);

            if (-1 == system(pThreadInfo->pExtProgPath))
            {
                fprintf(stderr, "\n\nError: %d:%s\n\n", errno, strerror(errno));
                retVal = 1;
            }
            break;

        case API_POPEN:
            /* test: p\n\topen */
            print_ppid_pid_tid();
            if (verbose) printf("popen( \"%s\" )\n", pThreadInfo->pExtProgPath);

            pFile = popen(pThreadInfo->pExtProgPath, "w");
            if (NULL == pFile)
            {
                fprintf(stderr, "\n\nError: %d:%s\n\n", errno, strerror(errno));
                retVal = 1;
            }
            else
            {
                pclose(pFile);
            }
            break;

        default:
            fprintf(stderr, "\n\nInternal error: Invalid API was specified\n\n");
            return (void*)1;
    }

    return (void*)retVal;
}

/***************************************************************************/
void printHelpMessage(char* argv0)
{
    printf("\n\nUsage:\n\t <API index> <executable0> [<executable1> [...]]\n\n");
    printf("API indicies:\n");
    printf("\t%d - execv\n", API_EXEC);
    printf("\t%d - fork + execv\n", API_FORK_EXEC);
    printf("\t%d - vfork + execve - single thread\n", API_VFORK_EXEC_SINGLE);
    printf("\t%d - vfork + execve - multiple threads\n", API_VFORK_EXEC_MULTIPLE);
    printf("\t%d - system\n", API_SYSTEM);
    printf("\t%d - popen\n", API_POPEN);
    printf("\n");
}

/* Example: main -mode 2 -nthreads 2
 *  Args:
 * -mode     ##     - A number between 0 and 5 that represent the launch mode, corresponds with Api_t
 * -nthreads ##     - Number of threads.
 *                   If nthreads==0 then the application will be launched from the main thread.
 *                   Else, nthreads will be created, each will launch the application.
 * -quiet            Don't print
 */
int main(int argc, char** argv)
{
    int numThreads = -1;
    int mode;
    const char* optNthreads = "-nthreads";
    const char* optMode     = "-mode";
    const char* optApp      = "-app";
    const char* optQuiet    = "-quiet";
    char* app               = NULL;
    int i                   = 0;
    while (i < argc)
    {
        if (strcmp(argv[i], optMode) == 0)
        {
            i++;
            assert(i < argc);
            mode = atoi(argv[i]);
        }
        else if (strcmp(argv[i], optNthreads) == 0)
        {
            i++;
            assert(i < argc);
            numThreads = atoi(argv[i]);
        }
        else if (strcmp(argv[i], optApp) == 0)
        {
            i++;
            assert(i < argc);
            app = argv[i];
        }
        else if (strcmp(argv[i], optQuiet) == 0)
        {
            verbose = false;
        }
        i++;
    }
    if (!app)
    {
        printf("Missing -app \n");
        return 1;
    }
    if (mode < API_EXEC || mode > API_POPEN)
    {
        printf("Illegal/missing -mode (should be between 0-5) \n");
        return 1;
    }
    if (numThreads < 0)
    {
        printf("Illegal/missing -nthreads \n");
        return 1;
    }

    print_ppid_pid_tid();

    switch ((Api_t)mode)
    {
        case API_EXEC:
            if (verbose) printf("API: execv\n");
            break;
        case API_FORK_EXEC:
            if (verbose) printf("API: fork + execv\n");
            break;
        case API_VFORK_EXEC_SINGLE:
            if (verbose) printf("API: vfork + execve - single thread\n");
            break;
        case API_VFORK_EXEC_MULTIPLE:
            if (verbose) printf("API: vfork + execve - multiple threads\n");
            break;
        case API_SYSTEM:
            if (verbose) printf("API: system\n");
            break;
        case API_POPEN:
            if (verbose) printf("API: popen\n");
            break;
    }

    print_ppid_pid_tid();
    if (verbose) printf("In main()\n");

    int retVal = 0;

    ThreadInfo_s threadInfo;
    threadInfo.api          = (Api_t)mode;
    threadInfo.pExtProgPath = app;
    if (numThreads == 0)
    {
        threadFunc(&threadInfo);
    }
    else
    {
        pthread_t* threads = new pthread_t[numThreads];
        for (i = 0; i < numThreads; i++)
        {
            pthread_create(&(threads[i]), NULL, threadFunc, &threadInfo);
        }

        for (i = 0; i < numThreads; i++)
        {
            void* threadRetVal;
            pthread_join(threads[i], &threadRetVal);
            if (threadRetVal != 0)
            {
                retVal = 1;
            }
        }
        delete[] threads;
    }

    print_ppid_pid_tid();
    if (verbose) printf("Done \n");
    return retVal;
}
