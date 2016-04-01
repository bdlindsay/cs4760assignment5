#include "semaphore.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>

// Brett Lindsay
// cs4760 assignment 5
// oss.h

typedef enum {false, true} bool;

// a process control block
typedef struct {
	int  maxClaim[20]; // value = max claim for that res. index
	double totalSysTime; // time in system
	double totalCpuTime; // time spent computing
	double cTime; // create time
	double dTime; // destroy time
	bool isCompleted;
	int shm_id; // ref to id for shm
	int pid; // pid for fork() return value
} pcb_t;	

// a resource descriptor
typedef struct {
	bool isShared; // is it a shared resource
	int requests; // how many processes are requesting
	int allocations; // # of allocations made
	int releases; // # needing to be released
	int total; // # num of resource
	int available; // # available
} rd_t;	

// logical clock and resource descriptors in oss 
typedef struct {
	rd_t rds[20]; // array of resource descripts
	double lClock;
	int sem_id; // semaphore for lClock access
	int shm_id; // run_info_t shm_id 
} run_info_t;

// simulation stats 
typedef struct {
	int tPut; // throughput
	double turnA; // turnaround
	double waitT; // waiting time
	double totalCpuTime; // total Cpu time to calc utilil.
	double cpuU; // cpu utilization
} sim_stats_t;

// helper functions
pcb_t* initPcb();
void updateClock(double);
void cleanUpPcbs(pcb_t *pcbs[]);
void cleanUp();
void removePcb(pcb_t *pcbs[], int i);
void updatePcbs(int usedPcbs[]);
void setBit(int*,int);
void clearBit(int*,int);
int testBit(int*,int);
void initRunInfo(int);
