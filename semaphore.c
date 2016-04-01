#include "semaphore.h"

// Brett Lindsay
// cs4760 assignment4
// semaphore.c

void sem_wait(int sem_id, int sem_num) {
	struct sembuf sops[1];
	sops[0].sem_num = sem_num; // which semaphore in set sem_id
	sops[0].sem_op = -1; // decrement by 1
	sops[0].sem_flg = 0; // no flag
	// wait()
	while((semop(sem_id, sops, 1)) == -1) {
		perror("semop:wait");
		if (errno != EINTR)
			return;
	}	
}

void sem_signal(int sem_id, int sem_num) {
	struct sembuf sops[1];
	sops[0].sem_num = sem_num; // which semaphore in set sem_id
	sops[0].sem_op = 1; // increment by 1
	sops[0].sem_flg = 0; // no flag
	// signal()
	if((semop(sem_id, sops, 1)) == -1) {
		perror("semop:signal");
		exit(1);
	}
}

int initelement(int id, int num, int val) {
	union semun {
		int val;
		struct semid_ds *buf;
		unsigned short *array;
	} arg;

	arg.val = val;

	return semctl(id, num, SETVAL, arg);
}
