#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// Brett Lindsay
// cs4760 assignment4
// semaphore.h

void sem_wait(int sem_id, int sem_num);
void sem_signal(int sem_id, int sem_num);
int initelement(int id, int num, int val);

#endif
