/*
 * Copyright (C) 2009-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

volatile int lock = 0;

static void* child(void* v)
{
    printf("Child: calling pthread_spin_lock()\n");
    int locked = pthread_spin_lock(&lock);
    switch (locked)
    {
        case 0:
            printf("Child: acquired spinlock\n");
            break;
        default:
            printf("Child: pthread_spin_lock returned with error %x\n", locked);
            break;
    }
    locked = pthread_spin_unlock(&lock);
    printf("Child: released spinlock\n");
    return 0;
}

int main()
{
    pthread_t child_thread;

    pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE);

    printf("Main: calling pthread_spin_lock()\n");
    int locked = pthread_spin_lock(&lock);
    switch (locked)
    {
        case 0:
            printf("Main: acquired spinlock\n");
            break;
        default:
            printf("Main: pthread_spin_lock returned with error %x\n", locked);
            break;
    }

    printf("Main: calling pthread_create()\n");
    int status = pthread_create(&child_thread, 0, child, 0);
    if (status != 0) printf("Main: could not create child thread\n");

    printf("Main: sleep(2)\n");
    sleep(2);

    locked = pthread_spin_unlock(&lock);
    printf("Main: released spinlock\n");

    printf("Main: calling pthread_join()\n");
    status = pthread_join(child_thread, 0);
    if (status != 0) printf("Main: pthread_join failed with error %x\n", status);

    printf("Main: done\n");
    return 0;
}
