#include "oss.h"

// Brett Lindsay
// cs4760 assignment4
// oss.c

char *arg2; // to send process_id num to process
char *arg3; // to send pcb shm_id num to process 
char *arg4; // to send runInfo shm_id num to process 
char *arg5; // to send lClock shm_id to process
pcb_t *pcbs[18] = { NULL };
run_info_t *runInfo;
sim_stats_t stats; 
// signal handler prototypes
void free_mem();
void timeout();

main (int argc, char *argv[]) {
	char *arg0 = "procSch";
	char *arg1 = "userProcess";
	arg2 = malloc(sizeof(int)); // process num in pcbs
	arg3 = malloc(sizeof(int)); // shm_id to pcb
	arg4 = malloc(sizeof(int)); // shm_id to runInfo
	int i, shm_id, q, n;
	double r; // for random "milli seconds"
	int usedPcbs[1] = { 0 }; // bit vector 0-17 needed for 19 PCBs
	bool isPcbsFull = false;
	int next, nextCreate = 0; // points to next available PID
	int sp_pid;
	srandom(time(NULL));
	signal(SIGINT, free_mem);
	signal(SIGALRM, timeout);

	// init sim_stats_t
	stats.avgSysTime = 0.000;
	stats.avgWaitTime = 0.000;
	stats.idleTime = 0.000;
	stats.tput = 0;

	// create shared runInfo
	if((shm_id = shmget(IPC_PRIVATE,sizeof(run_info_t*),IPC_CREAT|0755)) == -1){
		perror("shmget:runinfo");
	}	
	runInfo = (run_info_t*) shmat(shm_id,0,0);
	runInfo->shm_id = shm_id;
	runInfo->lClock = 0.000;
	// set burst and process_num each time a process is chosen
	//q = -1; // 1 queue b/w process creation, init -1 so first queue is 0
	while(1) { // infinite loop until alarm finishes
		if (runInfo->lClock > 120) {
			alarm(1);
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
				fprintf(stderr, "pcbs array was full trying again...\n");
			} else { // create a new process
				// create and assign pcb to the OS's array
				// init PCB
				fprintf(stderr, "Creating new process at pcb[%d]\n",next);
				pcbs[next] = initPcb();
				setBit(usedPcbs,next);

				// args for userProcess
				sprintf(arg2, "%d", next/*selected*/); // process num in pcbs
				sprintf(arg3, "%d", pcbs[next]->shm_id); // pcb for userProcess
				sprintf(arg4, "%d", runInfo->shm_id); // runInfo for userProcess

				if ((pcbs[next]->pid = fork()) == -1) {
					perror("fork");
					// reset init values if fork fails
					removePcb(pcbs,next);
					clearBit(usedPcbs,next);
					// run a process to try to let fork work next time
					for (i = 0; i < 3; i++) {
						runInfo->burst = 4 - i; // -1 sec burst for each lower priority
						scheduleProcess(i, arg1, -1); // -1 is NULL value for func
					}
					// update pcbs after process run
					updatePcbs(usedPcbs);
					// update logical clock
					r = (double)(random() % 1000) / 1000;	
					updateClock(r);
					continue; // so that oss doesn't get execl
				}
				if (pcbs[next]->pid == 0) { // child process 
					execl("userProcess", arg1, arg2, arg3, arg4, 0);
				} 
			}
			// next process creation time
			r = rand() % 3; // 0-2
			nextCreate = runInfo->lClock + r;
		} // end create process if block
		
		// schedule a process
		// process create -> all queues -> process create...
		for (i = 0; i < 3; i++) {
			runInfo->burst = 4 - i; // -1 sec burst for each lower priority
	  	scheduleProcess(i, arg1, -1); // -1 is NULL value for func
		}
		/*// process create -> queue -> process create...
		runInfo->burst = 8 - q; // -1 sec burst for each lower priority
		scheduleProcess(q, arg1, -1); // -1 is NULL value for func*/
		// OR start process scheduler - tested, not implemented
		/*sprintf(arg4,"%d",runInfo->shm_id);
		if ((sp_pid = fork()) == -1) {
			perror("fork: sp");
		}
		if (sp_pid == 0) { // if child
			execl("procSch", arg0,arg4,0); 
		} // else parent*/

		// check for finished processes
		updatePcbs(usedPcbs);

		// update logical clock
		r = (double)(random() % 1000) / 1000;	
		updateClock(r);
		//if (++q >= 3) // proc create -> queue -> proc create 0 -> 1 -> 2 -> 0... 
		//	q = 0; // for other implementation test
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
			stats.tput++;
			stats.avgSysTime += pcbs[i]->totalSysTime;
			stats.avgWaitTime += pcbs[i]->totalSysTime - pcbs[i]->totalCpuTime;
			stats.totalCpuTime += pcbs[i]->totalCpuTime; 

			// remove pcb
			fprintf(stderr,"oss: Removing finished pcb[%d]\n",i);
			removePcb(pcbs, i);
			clearBit(usedPcbs,i);
		} else if (pcbs[i]->ioInterupt) {
			fprintf(stderr, "Process %d seems io-bound, maintain priority.\n", i);	
		} else if (pcbs[i]->cTime == runInfo->lClock) {
			fprintf(stderr,"Not changing priority of process just created.\n");
		}	else if (pcbs[i]->priority == 0) { 
			// not completed/no i/o intr
			fprintf(stderr, "Process %d decreased to priority 1: %.3f.\n",i,
				pcbs[i]->totalCpuTime);
			pcbs[i]->priority = 1;
		} else if (pcbs[i]->priority == 1) { 
			// if still not done
			fprintf(stderr, "Process %d decreased to priority 2. %.3f\n",i,
				pcbs[i]->totalCpuTime);
			pcbs[i]->priority = 2;
		} else if (pcbs[i]->priority == 2) {
			fprintf(stderr, "Process %d in PQ2 %.3f/%.3f.\n", i,
				pcbs[i]->totalCpuTime, pcbs[i]->timeToComplete, pcbs[i]->totalSysTime);
		}
	} 
} // end updatePcbs()

double calcCompletionTime(int i) {
	// SRTF
	// total time to complete - total cpu time so far = completion time
	return (double) pcbs[i]->timeToComplete - pcbs[i]->totalCpuTime;
}
double getCompletionTime(int i) {
	// SJN
	return pcbs[i]->timeToComplete;
}

// which process should run next, queue 0 or 1 SJN, queue 2 Round Robin (rr)
// send scheduleProcess queue -1 and a valid process number
// for rr_p_num schedules that process to run for runInfo->burst/burstDiv
void scheduleProcess(int queue, char *arg1, int rr_p_num) {
	int selected = -1;
	bool q2ran = false; // don't print no processes msg after Q2 completes

	// schedule next process
	if (queue == 0 || queue == 1) {
		selected = findSJN(queue);	
	} else if (queue == 2) {
		q2ran = scheduleRR(arg1);	
	} else if (queue == -1) {
		selected = rr_p_num;
	}

	// in case there are no processes ready
	if (selected == -1 && !q2ran) {
		fprintf(stderr,"Queue %d: No processes to schedule\n", queue);
		return;
	}	
	
	// tell selected process to run
	if(pcbs[selected] != NULL && !pcbs[selected]->isCompleted) { //safety
		// -1 value of queue is really queue 2
		if (queue == -1) {
			queue = 2;
		}

		fprintf(stderr,"Queue %d scheduling process %d.\n", queue, selected);
		sem_signal(pcbs[selected]->sem_id,0); 
		sleep(1); // don't pick up your own signal
		sem_wait(pcbs[selected]->sem_id,0);
		//wait();
		//pcbs[selected]->pid = -1; // process still running this implementation 
	}
} // end scheduleProcess()

// determine shortest process for SJN/SRTN strategey priority 0,1
int findSJN(int queue) {
	int k;
	int selected = -1;
	double sjn = 0; // shortest job next
	double tmp = 0; // holder
	
	// find shortest out of processes in queue
	for (k = 0; k < 18; k++) {
		// continue if no pcb for this index
		if(pcbs[k] != NULL && pcbs[k]->priority == queue &&
			 !pcbs[k]->isCompleted) { 
			if (sjn == 0) { // init to completion time of first non-null pcb
				sjn = calcCompletionTime(k); // SRTN
				//sjn = getCompletionTime(k); // SJN
				selected = k;
			} else {
				if ((tmp = calcCompletionTime(k)) < sjn) { // SRTN
				//if ((tmp = getCompletionTime(k)) < sjn) { // SJN
					sjn = tmp;
					selected = k;
				} else if (tmp == sjn) {
					// ties broken by FIFO
					if (pcbs[selected]->cTime > pcbs[k]->cTime) {
						sjn = tmp;
						selected = k;
					}
				}
			}
		}
	} // end for loop for process selection
	return selected;
}

// round robin scheduling for priority 2
bool scheduleRR(char *arg1) {
	int i = 0;
	int num = 0;
	bool ranProc = false;
	// see how many processes are priority 2
	for(i = 0; i < 18; i++) {
		if (pcbs[i] != NULL && pcbs[i]->priority == 2 && !pcbs[i]->isCompleted) {
			num++;
		}
	}
	if (num > 0) {
		fprintf(stderr, "Round-Robin Scheduling for %d processes.\n", num); 
	}
	runInfo->burst /= (double) num;
	for(i = 0; i < 18; i++) {
		if (pcbs[i] != NULL && pcbs[i]->priority == 2 && !pcbs[i]->isCompleted) {
			scheduleProcess(-1, arg1, i); // run process i for burst/num
			ranProc = true;
		}
	}

	return ranProc;
}
// update clock for 1 iteration, or update by a custom millisec. amt
void updateClock(double r) {
	runInfo->lClock++;
	runInfo->lClock += r;

	fprintf(stderr, "oss: lClock: %.3f\n", runInfo->lClock); 
		
	sleep(1); // slow it down
}

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

	pcb->totalCpuTime = 0.000;
	pcb->totalSysTime = 0.000;
	pcb->cTime = runInfo->lClock;
	pcb->dTime = -1.000;
	pcb->lastBurstTime = -1.000;
	pcb->ioInterupt = false;
	pcb->priority = 0; // 0 high, 1 medium, 2 low
	pcb->isCompleted = false;
	pcb->shm_id = shm_id;
	r = rand() % 2; // between 0-1
	if (r < 1) {
		pcb->bound = io;
	} else { // r  > 1
		pcb->bound = cpu;
	}	
	if (pcb->bound == io) { // 2.000-4.999
		pcb->timeToComplete = (double)((rand() % 3) + 2); // 2-4 
		pcb->timeToComplete += (double)(rand() % 1000) / 1000;// 0-.999
	} else { // == cpu 6.000-8.999
		pcb->timeToComplete = (double) ((rand() % 3) + 6); // 6 - 8 
		pcb->timeToComplete += (double) (rand() % 1000) / 1000;// 0-.999
	}
	// semaphore to pause and resume the process
	if ((pcb->sem_id = semget(IPC_PRIVATE,1,IPC_CREAT | 0755)) == -1) {
		perror("semget");
	}
	if (initelement(pcb->sem_id,0,0) != 0) { // init to 0
		perror("semctl:initelement");
	}	
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
	// clean up semaphore
	if ((n = semctl(pcbs[i]->sem_id,0,IPC_RMID)) == -1) {
		perror("semctl:IPC_RMID:semaphore");
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

// clean up with free(), remove lClock, call cleanUpPcbs()
void cleanUp() {
	int shm_id = runInfo->shm_id;

	if ((shmdt(runInfo)) == -1) {
		perror("shmdt:runInfo");
	}
	if ((shmctl(shm_id, IPC_RMID, NULL)) == -1) {
		perror("shmctl:IPC_RMID:runInfo");	
	}

	cleanUpPcbs(pcbs);
	free(arg2);
	free(arg3);
	free(arg4);
	free(arg5);
}
// SIGINT handler
void free_mem() {
	int z;
	FILE *fp;
	fprintf(stderr, "Recieved SIGINT. Cleaning up and quiting.\n");

	// end stats
	stats.idleTime = runInfo->lClock - stats.totalCpuTime;
	stats.avgSysTime /= (double) stats.tput;
	stats.avgWaitTime /= (double) stats.tput;
	
	if ((fp = fopen("endStats.txt","w")) == NULL) {
		perror("fopen:endstats");
	} else {
		// overwrite/write to file
		fprintf(fp,"End Stats:\nAvg Sys Time: %.3f\nAvg Wait Time: %.3f\nSys Idle Time: %.3f\n",
			stats.avgSysTime, stats.avgWaitTime, stats.idleTime);
		fprintf(fp,"TotalCpuTime: %.3f\nTotal Run Time: %.3f\n",
			stats.totalCpuTime, runInfo->lClock);
		fclose(fp);
	}
	// write to stderr
	fprintf(stderr,"End Stats:\nAvgSysTime: %.3f\nAvgWaitTime: %.3f\nSysIdleTime: %.3f\n",
		stats.avgSysTime, stats.avgWaitTime, stats.idleTime);
	fprintf(stderr,"TotalCpuTime: %.3f\nTotal Run Time: %.3f\n",
			stats.totalCpuTime, runInfo->lClock);
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
