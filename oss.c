#include "oss.h"

// Brett Lindsay
// cs4760 assignment4
// oss.c

char *arg1; // to send process_id num to process
char *arg2; // to send pcb shm_id num to process 
char *arg3; // to send runInfo shm_id num to process 
pcb_t *pcbs[18] = { NULL };
run_info_t *runInfo;
sim_stats_t stats; 
// signal handler prototypes
void free_mem();
void timeout();

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
	int sp_pid;
	srandom(time(NULL));
	signal(SIGINT, free_mem);
	signal(SIGALRM, timeout);

	// init sim_stats_t for averages
	stats.tPut = 0;
	stats.turnA = 0.000;
	stats.waitT = 0.000;
	stats.totalCpuTime = 0.000;
	stats.cpuU = 0.000;

	// create shared runInfo
	if((shm_id = shmget(IPC_PRIVATE,sizeof(run_info_t*),IPC_CREAT|0755)) == -1){
		perror("shmget:runinfo");
	}	
	runInfo = (run_info_t*) shmat(shm_id,0,0);
	initRunInfo(shm_id);

	while(1) { // infinite loop until alarm finishes
		/*if (runInfo->lClock > 120) {
			alarm(1);
		}*/
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
				fprintf(stderr, "pcbs array was full trying again...\n");
			} else { // create a new process
				// create and assign pcb to the OS's array
				// init PCB
				fprintf(stderr, "Creating new process at pcb[%d]\n",next);
				pcbs[next] = initPcb();
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
			}
			if (pcbs[next] != NULL) { // don't change nextCreate if fork failed
				// next process creation time
				r = rand() % 3; // 0-2
				nextCreate = runInfo->lClock + r;
			}
		} // end create process if block
		
		// check for finished processes
		updatePcbs(usedPcbs);

		// update logical clock
		r = (double)(random() % 1000) / 1000;	
		updateClock(r);
	} // end infinite while	

	// cleanup after normal execution - never reached in current implementation
	cleanUp(); // clean up with free(), remove lClock, call cleanUpPcbs()
}

// check for finished processes
void updatePcbs(int usedPcbs[]) {
	int i;

	for (i = 0; i < 18; i++) {
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
	sem_wait(runInfo->sem_id,0);
	runInfo->lClock++;
	runInfo->lClock += r;
	// signal others can update the clock
	sem_signal(runInfo->sem_id,0);
	fprintf(stderr, "oss: lClock: %.03f\n", runInfo->lClock); 
		
	sleep(1); // slow it down
}

void initRunInfo(int shm_id) {
	int i; // index
	int r; // rand int
	srandom(time(NULL));

	runInfo->shm_id = shm_id;

	// determine total system resources
	for (i = 0; i < 20; i++) {
		runInfo->rds[i].requests = 0;	
		runInfo->rds[i].allocations = 0;	
		runInfo->rds[i].total = (rand() % 10) + 1; // 1-10
		runInfo->rds[i].available = runInfo->rds[i].total;	
		// 20% shared
		if (i < 16) {
			runInfo->rds[i].isShared = false;		
			fprintf(stderr, "Created resource %d - shared: F - total: %d\n",
				i, runInfo->rds[i].total);
		} else {
			runInfo->rds[i].isShared = true;
			fprintf(stderr, "Created resource %d - shared: T - total: %d\n",
				i, runInfo->rds[i].total);
		}
	}

	// init lClock
	runInfo->lClock = 0.000;

	// init semaphore for lClock
	if ((runInfo->sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0755)) == -1) {
		perror("semget:runInfo");
	} else {	
		initelement(runInfo->sem_id, 0, 1); // init semaphore to 1
	}

} // end initRunInfo

// init and return pointer to shared pcb
pcb_t* initPcb() {
	int shm_id, r;
	pcb_t *pcb;
	srandom(time(NULL));

	if ((shm_id = shmget(IPC_PRIVATE, sizeof(pcb_t*), IPC_CREAT | 0755)) == -1) {
		perror("shmget");
		exit(1);
	}	
	if ((pcb = (pcb_t*) shmat(shm_id,0,0)) == (void*)-1) {
		perror("shmat");
		exit(1);
	}

	pcb->totalSysTime = 0.000;
	pcb->totalCpuTime = 0.000;
	pcb->cTime = runInfo->lClock;
	pcb->dTime = -1.000;
	pcb->isCompleted = false;
	pcb->shm_id = shm_id;

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
	int sem_id = runInfo->sem_id;
	int shm_id = runInfo->shm_id;

	if (semctl(runInfo->sem_id, 0, IPC_RMID) == -1) {
		perror("semctl:sem:rmid");
	}

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
	wait(); // wait for children??
	fprintf(stderr, "Finished watiing for children\n");
	
	// end stats
	stats.turnA /= (double) stats.tPut;
	stats.waitT /= (double) stats.tPut;
	stats.cpuU = stats.totalCpuTime / runInfo->lClock; 
	
	if ((fp = fopen("endStats.txt","w")) == NULL) {
		perror("fopen:endstats");
	} else {
		// overwrite/write to file
		fprintf(fp,"End Stats:\nThroughput: %.3f\nAvg Turnaround: %.3f\n Avg Wait Time: %.3f\n",
			stats.tPut, stats.turnA, stats.waitT);
		fprintf(fp,"TotalCpuTime: %.3f\nTotal Run Time: %.3f\nCPU Utilization: %.3f\n",
			stats.totalCpuTime, runInfo->lClock, stats.cpuU);
		fclose(fp);
	}
	// write to stderr
	fprintf(stderr,"End Stats:\nThroughput: %.3f\nAvg Turnaround: %.3f\n Avg Wait Time: %.3f\n",
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

void timeout() {
	// timeout duration passed, send SIGINT
	fprintf(stderr, "Timeout duraction reached.\n");
	raise(SIGINT);
}
