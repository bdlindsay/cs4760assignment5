#include "oss.h"

// Brett Lindsay 
// cs4760 assignment 5
// userProcess.c

pcb_t *pcb;
run_info_t *runInfo;
int pNum;

// prototypes
void addToClock(double);
void intr_handler();


main (int argc, char *argv[]) {
	int shm_id;
	int r; // random
	double rd; // random double
	pNum = atoi(argv[1]);
	signal(SIGINT,intr_handler);
	srandom(time(NULL));

	// get pcb info
	shm_id = atoi(argv[2]);
	if ((pcb = (pcb_t*) shmat(shm_id,0,0)) == (void*) -1) {
		perror("shmat:pcb");
	}	
	
	// get runInfo
	shm_id = atoi(argv[3]);
	if ((runInfo = (run_info_t*) shmat(shm_id,0,0)) == (void*) -1) {
		perror("shmat:runInfo");
	}

	// generate maximum claims
	fprintf(stderr, "entering and exiting process %d\n",pNum);
	pcb->isCompleted = true;

	// detach from shared
	shmdt(runInfo);
	runInfo = NULL;

	shmdt(pcb);
	pcb = NULL;
}

void addToClock(double d) {
	// wait for chance to change clock
	sem_wait(runInfo->sem_id,0);
	runInfo->lClock += d;
	fprintf(stderr, "userProcess: lClock: %.03f\n", runInfo->lClock);
	// signal others may update clock
	sem_signal(runInfo->sem_id,0);
}	

void intr_handler() {
	signal(SIGINT, SIG_DFL); // change to default SIGINT behavior
	
	// detach from shared if not already
	if (pcb != NULL) {
		shmdt(pcb);
	}	
	if (runInfo != NULL) {
		shmdt(runInfo);
	}	

	fprintf(stderr,"Received SIGINT: Process %d cleaned up and dying.\n",pNum);

	raise(SIGINT);
}	
