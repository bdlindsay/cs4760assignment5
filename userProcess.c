#include "oss.h"

// Brett Lindsay 
// cs4760 assignment 5
// userProcess.c

pcb_t *pcb = NULL;
run_info_t *runInfo = NULL;
int pNum;

// prototypes
void addToClock(double);
void intr_handler();
void error_h();
int calcTotalRes();

main (int argc, char *argv[]) {
	int shm_id;
	int r; // random
	int i; // index
	int a; // for maxClaim - claimed
	double rd; // random double
	pNum = atoi(argv[1]);
	signal(SIGINT,intr_handler);
	signal(SIGFPE,error_h);
	srand(time(NULL));
	runInfo = malloc(sizeof(run_info_t*));
	pcb = malloc(sizeof(pcb_t*));

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
	for(i = 0; i < 20; i++) {
		if (runInfo->rds[i].total == 0) {
			fprintf(stderr,"i: %d ahhahahah: %d\n",i,runInfo->rds[i].total);
			runInfo->rds[i].total = 10;
		}
		pcb->maxClaim[i] = rand() % runInfo->rds[i].total; 
	}

	while (pcb->isCompleted == false) {
		// determine claim(1) or release(0)
		if (pcb->total == 0) {
			r = 1;
		} else {
			r = rand() % 2;
		}	
		pcb->action.res = rand() % 20; // which resource to claim or release
		if (r == 1) { // claim
			// how many, 0 means resources unchanged
			pcb->action.isClaim = true;
			a = pcb->maxClaim[pcb->action.res] - pcb->claimed[pcb->action.res];	
			if (a != 0) { // don't mod 0
				pcb->action.num = (rand() % a) + 1;
				pcb->action.isDone = false;
			} else {
				pcb->action.num = 0;
				pcb->action.isDone = true;
			}
			// make request
			sem_wait(&pcb->sem);
			// request granted
			pcb->action.isDone = true;
			pcb->total += pcb->action.num;
			pcb->claimed[pcb->action.res] += pcb->action.num;
			fprintf(stderr, "Process %d: claim %d of R:%d (%d/%d)\n", pNum, pcb->action.num,
				pcb->action.res,pcb->claimed[pcb->action.res], pcb->maxClaim[pcb->action.res]);
		} else { // release
			pcb->action.isClaim = false;
			while(pcb->claimed[pcb->action.res] == 0) { // will find nearest resource of claim > 0
				pcb->action.res++;
				if (pcb->action.res >= 20) { // don't go out of bounds
					pcb->action.res = 0;
				}	
			}
			a = pcb->claimed[pcb->action.res]; 
			pcb->action.num = (rand() % a) + 1; // always release at least 1
			pcb->action.isDone = false;
			// release
			sem_wait(&pcb->sem);
			// oss acknowledged release
			pcb->action.isDone = true;
			pcb->total -= pcb->action.num;
			pcb->claimed[pcb->action.res] -= pcb->action.num;
			fprintf(stderr, "Process %d: release %d of R:%d (%d/%d)\n", pNum, pcb->action.num,
				pcb->action.res,pcb->claimed[pcb->action.res], pcb->maxClaim[pcb->action.res]);
		}

		// processing time
		rd = ((double)(rand() % 1000) + 200.000) / 1000; // 200-1200
		pcb->totalCpuTime += rd;
		// add processing time to clock
		addToClock(rd);

		// time to quit?
		if (pcb->totalCpuTime > 1) {
			r = rand() % 5; // 1-yes 
			if (r >= 1) {
				pcb->dTime = runInfo->lClock;
				pcb->totalSysTime = pcb->dTime - pcb->cTime;
				pcb->isCompleted = true;
				// release resources
				fprintf(stderr, "Process %d: release all and die\n", pNum);
			}	
		}
		fflush(stderr);
		sleep(1); // get different nums from rand
	} // end while

	// detach from shared
	shmdt(runInfo);
	runInfo = NULL;

	shmdt(pcb);
	pcb = NULL;
}

void addToClock(double d) {
	// wait for chance to change clock
	//sem_wait(runInfo->sem_id,0);
	sem_wait(&runInfo->sem);
	runInfo->lClock += d;
	fprintf(stderr, "userProcess%d: lClock + %.03f : %.03f\n", pNum, d, runInfo->lClock);
	// signal others may update clock
	//sem_signal(runInfo->sem_id,0);
	sem_post(&runInfo->sem);
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

void error_h() {
	signal(SIGFPE,error_h);
	fprintf(stderr,"Error out of my control occurred gah!\n");
	pcb->isCompleted = true;
	raise(SIGINT);

}
