#include "oss.h"

// Brett Lindsay
// cs4760 assignment5
// oss.c

char *arg1; // to send process_id num to process
char *arg2; // to send pcb shm_id num to process 
char *arg3; // to send runInfo shm_id num to process 
pcb_t *pcbs[18] = { NULL };
run_info_t *runInfo = NULL;
sim_stats_t stats; 
// signal handler prototypes
void free_mem();

main (int argc, char *argv[]) {
	char *arg0 = "userProcess";
	arg1 = malloc(sizeof(int)); // process num in pcbs
	arg2 = malloc(sizeof(int)); // shm_id to pcb
	arg3 = malloc(sizeof(int)); // shm_id to runInfo
	int i, shm_id, q, n;
	double r; // for random "milli seconds"
	int usedPcbs[1] = { 0 }; // bit vector 0-17 needed for 19 PCBs
	bool isPcbsFull = false;
	int next, nextCreate = 0; // points to next available PID
	int res_pid;
	srand(time(NULL));
	signal(SIGINT, free_mem);

	// init sim_stats_t for averages
	stats.tPut = 0;
	stats.turnA = 0.000;
	stats.waitT = 0.000;
	stats.totalCpuTime = 0.000;
	stats.cpuU = 0.000;

	// create shared runInfo
	if((shm_id = shmget(IPC_PRIVATE,sizeof(run_info_t*),IPC_CREAT | 0755)) == -1){
		perror("shmget:runinfo");
	}	
	runInfo = (run_info_t*) shmat(shm_id,0,0);
	fprintf(stderr, "shm: %d\n",shm_id);
	runInfo->shm_id = shm_id;
	initRunInfo(shm_id);

	while(1) { // infinite loop until alarm finishes
		if (runInfo->lClock > 30) {
			fprintf(stderr,"Timeout duration reached\n");
			raise(SIGINT);
		}
		if (nextCreate < runInfo->lClock) {
			isPcbsFull = true;
			for (i = 0; i < 18; i++) {
				if (testBit(usedPcbs,i) == 0) { 
					next = i;	
					isPcbsFull = false;	// need to set false when process killed
					break;
				}	
			}
			if (isPcbsFull) {
				//fprintf(stderr, "pcbs array was full trying again...\n");
			} else { // create a new process
				// create and assign pcb to the OS's array
				// init PCB
				fprintf(stderr, "Creating new process at pcb[%d]\n",next);
				pcbs[next] = initPcb(next);
				setBit(usedPcbs,next);

				// args for userProcess
				sprintf(arg1, "%d", next); // process num in pcbs
				sprintf(arg2, "%d", pcbs[next]->shm_id); // pcb for userProcess
				sprintf(arg3, "%d", runInfo->shm_id); // runInfo for userProcess

				if ((pcbs[next]->pid = fork()) == -1) {
					perror("fork");
					// reset init values if fork fails
					removePcb(pcbs,next);
					clearBit(usedPcbs,next);
				}
				if (pcbs[next] != NULL && pcbs[next]->pid == 0) { // child process 
					execl("userProcess", arg0, arg1, arg2, arg3, 0);
				} 
				//wait();
			}
			if (pcbs[next] != NULL) { // don't change nextCreate if fork failed
				// next process creation time
				r = (double)((rand() % 500) + 1) / 1000; // 1-500 milli
				nextCreate = runInfo->lClock + r;
			}
		} // end create process if block
		
		// check for finished processes
		updatePcbs(usedPcbs);

		// check for process requests and allocate if no deadlock could occur
		//deadlock();

		// update logical clock
		r = (double)(rand() % 1000) / 1000;	
		updateClock(r);
	} // end infinite while	

	// cleanup after normal execution - never reached in current implementation
	cleanUp(); // clean up with free(), remove lClock, call cleanUpPcbs()
}

// check for deadlock. 
// pcbs->action is what each process needs
// runInfo->rds is what the system has available/has allocated
void deadlock() {
	int i, need, available, rNum;
	fprintf(stderr,"Seeing if I can allocate/release stuff\n");
	// loop through process here instead of a separate loop calling deadlock
	// i is process number
	for(i = 0; i < 18; i++) {
		// no process for this pcb
		if (pcbs[i] == NULL) {
			continue;
		}	
		// if process has no request 
		if ( pcbs[i]->action.num == 0 || pcbs[i]->action.isDone || pcbs[i]->isCompleted) {
			sem_post(&pcbs[i]->sem);
			sem_post(&pcbs[i]->sem);
			continue;
		}	
		// claim or release
		rNum = pcbs[i]->action.res;
		need = pcbs[i]->action.num;
		if (pcbs[i]->action.isClaim) { // claim 
			// process i needs resources, enough available? 
			if (need <= runInfo->rds[rNum].available) { // can allocate
				runInfo->rds[rNum].available -= need;
				//runInfo->rds[rNum].allocations[i].num += need;

				// tell process allocation granted
				sem_post(&pcbs[i]->sem);
				sem_post(&pcbs[i]->sem);
			}  else { // process i waits on semaphore : can't allocate
				// keep track of which requests are outstanding
				// pNum will keep track of how long it has waited
				//fprintf(stderr, "TEST: tracking outstanding Process %d req -> %d %d %d\n", 
			//		i, runInfo->rds[rNum].requests[i].pNum, rNum, need);
				//runInfo->rds[rNum].requests[i].pNum += 1;
			}	

		} else { // release
			//runInfo->rds[rNum].allocations[i] -= need;
			runInfo->rds[rNum].available += need;
			// TODO remove later
			if (runInfo->rds[rNum].available > runInfo->rds[rNum].total) 
				fprintf(stderr, "TEST: FAIL: %d %d\n", runInfo->rds[rNum].available, runInfo->rds[rNum].total);
			// tell process release finished
			sem_post(&pcbs[i]->sem);
			sem_post(&pcbs[i]->sem);
		}
	}
	fprintf(stderr,"Finished seeing if I can allocate/release stuff\n");
	fflush(stderr);
}

// check for finished processes
void updatePcbs(int usedPcbs[]) {
	int i;

	for (i = 0; i < 18; i++) {  // TODO change back
		// continue if no pcb
		if (pcbs[i] == NULL) {
			continue;
		}	
		// if there is a completed process
		if (pcbs[i]->isCompleted) {
			// collect data on userProcess
			stats.tPut++;
			stats.turnA += pcbs[i]->totalSysTime;
			stats.waitT += pcbs[i]->totalSysTime - pcbs[i]->totalCpuTime;
			stats.totalCpuTime += pcbs[i]->totalCpuTime; 

			// remove pcb
			fprintf(stderr,"oss: Removing finished pcb[%d]\n",i);
			removePcb(pcbs, i);
			clearBit(usedPcbs,i);
		}
	} 
} // end updatePcbs()

// update clock for 1 iteration, or update by a custom millisec. amt
void updateClock(double r) {
	// wait for chance to change clock
	sleep(1); // don't let it reclaim
	//sem_wait(runInfo->sem_id,0);
	sem_wait(&runInfo->sem);
	runInfo->lClock += r;
	// signal others can update the clock
	//sem_signal(runInfo->sem_id,0);
	sem_post(&runInfo->sem);
	fprintf(stderr, "oss: lClock: %.03f\n", runInfo->lClock); 
		
	//sleep(1); // slow it down
}

void initRunInfo(int shm_id) {
	int i; // index
	int r; // rand int

	runInfo->shm_id = shm_id;

	// determine total system resources
	for (i = 0; i < 20; i++) { 
		// requests, allocations, & releases arrays init to { NULL }
		runInfo->rds[i].total = (rand() % 10) + 1; // 1-10
		runInfo->rds[i].available = runInfo->rds[i].total;	
		// 20% shared TODO make this random +/- 5%
		if (i < 16) {
			runInfo->rds[i].isShared = false;		
		} else {
			runInfo->rds[i].isShared = true;
		}
		fprintf(stderr, "Created resource %d - shared: %d - total: %d\n",
			i, runInfo->rds[i].isShared, runInfo->rds[i].total);
	}

	// init lClock
	runInfo->lClock = 0.000;
	// init semaphore for lClock
	sem_init(&runInfo->sem, 0, 1);

} // end initRunInfo

// init and return pointer to shared pcb
pcb_t* initPcb(int pNum) {
	int shm_id, r;
	pcb_t *pcb;
	//srandom(time(NULL));

	if ((shm_id = shmget(IPC_PRIVATE, sizeof(pcb_t*), IPC_CREAT | 0755)) == -1) {
		perror("shmget");
		exit(1);
	}	
	if ((pcb = (pcb_t*) shmat(shm_id,0,0)) == (void*)-1) {
		perror("shmat");
		exit(1);
	}
	// pcb->claimed, ->maxClaim are init to 0
	pcb->action.res = -1;
	pcb->action.num = 0;
	pcb->action.pNum = pNum;
	pcb->totalSysTime = 0.000;
	pcb->totalCpuTime = 0.000;
	pcb->cTime = runInfo->lClock;
	pcb->dTime = -1.000;
	pcb->total = 0;
	pcb->isCompleted = false;
	pcb->shm_id = shm_id;
	// init arrays in userProcess
	
	// init semaphore for userProcess - block self 
	sem_init(&pcb->sem, 0, 0);
	
	return pcb;
} // end initPcb()

// bit array methods
// sets kth bit to 1
void setBit(int v[], int k) {
	v[(k/32)] |= 1 << (k % 32);
}
// sets kth bit to 0
void clearBit(int v[], int k) {
	v[(k/32)] &= ~(1 << (k % 32));
}
// returns value of kth bit
int testBit(int v[], int k) {
	return ((v[(k/32)] & (1 << (k % 32))) != 0);	
}

// detach and remove a pcb
void removePcb(pcb_t *pcbs[], int i) {
	int shm_id, n;
	if (pcbs[i] == NULL) {
		return;
	}

	// TODO clean up zombie?
	waitpid(pcbs[i]->pid,NULL,0);
	// remove pcb semaphore
	sem_destroy(&pcbs[i]->sem);

	// clean up shared memory
	shm_id = pcbs[i]->shm_id;
	if((n = shmdt(pcbs[i])) == -1) {
		perror("shmdt:pcb");
	}
	if((n = shmctl(shm_id, IPC_RMID, NULL)) == -1) {
		perror("shmctl:IPC_RMID:pcb");
	}
	pcbs[i] = NULL;
}

// call removePcb on entire array of pcbs
void cleanUpPcbs(pcb_t *pcbs[]) {
	int i;
	for(i = 0; i < 18; i++) {
		if (pcbs[i] != NULL) {
			removePcb(pcbs, i);
		}
	}
}

// clean up with free(), clean up runInfo, call cleanUpPcbs()
void cleanUp() {
	int shm_id = runInfo->shm_id;

	// destory runInfo semaphore
	sem_destroy(&runInfo->sem);

	if ((shmdt(runInfo)) == -1) {
		perror("shmdt:runInfo");
	}
	if ((shmctl(shm_id, IPC_RMID, NULL)) == -1) {
		perror("shmctl:IPC_RMID:runInfo");	
	}

	cleanUpPcbs(pcbs);
	free(arg1);
	free(arg2);
	free(arg3);
}

// SIGINT handler
void free_mem() {
	int z;
	FILE *fp;
	fprintf(stderr, "Recieved SIGINT. Cleaning up and quiting.\n");
	
	// end stats
	stats.turnA /= (double) stats.tPut;
	stats.waitT /= (double) stats.tPut;
	stats.cpuU = stats.totalCpuTime / runInfo->lClock; 
	
	if ((fp = fopen("endStats.txt","w")) == NULL) {
		perror("fopen:endstats");
	} else {
		// overwrite/write to file
		fprintf(fp,"End Stats:\nThroughput: %d\nAvg Turnaround: %.3f\n Avg Wait Time: %.3f\n",
			stats.tPut, stats.turnA, stats.waitT);
		fprintf(fp,"TotalCpuTime: %.3f\nTotal Run Time: %.3f\nCPU Utilization: %.3f\n",
			stats.totalCpuTime, runInfo->lClock, stats.cpuU);
		fclose(fp);
	}
	// write to stderr
	fprintf(stderr,"End Stats:\nThroughput: %d\nAvg Turnaround: %.3f\n Avg Wait Time: %.3f\n",
		stats.tPut, stats.turnA, stats.waitT);
	fprintf(stderr,"TotalCpuTime: %.3f\nTotal Run Time: %.3f\nCPU Utilization: %.3f\n",
		stats.totalCpuTime, runInfo->lClock, stats.cpuU);
	// make sure processes are killed
	for (z = 0; z < 18; z++) {
		if (pcbs[z] != NULL) {
			if (pcbs[z]->pid != -1) {
				kill(pcbs[z]->pid,SIGINT);
				waitpid(pcbs[z]->pid,NULL,0);
			}
		}
	}
	// clean up with free(), remove lClock, call cleanUpPcbs()
	cleanUp();

	signal(SIGINT, SIG_DFL); // resore default action to SIGINT
	raise(SIGINT); // take normal action for SIGINT after my clean up
}
