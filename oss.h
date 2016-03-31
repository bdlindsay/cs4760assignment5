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
// cs4760 assignment 4
// oss.h

typedef enum {false, true} bool;
typedef enum {io,cpu} pType;

// logical clock - part of run_info_t
typedef struct {
	unsigned int sec;
	unsigned int milli;
} lClock_t;

// a process control block
typedef struct {
	double totalCpuTime; // time processing
	double totalSysTime; // time in system
	double cTime; // create time
	double dTime; // destroy time
	double lastBurstTime; 
	bool ioInterupt;
	int priority; // 0 high, 1 medium, 2 low
	bool isCompleted;
	int shm_id; // ref to id for shm
	int pid; // pid for fork() return value
	pType bound; // io or cpu 
	double timeToComplete; // varies based on pType
	int sem_id; // hold semaphore id
	//semaphore sem; part of semaphore set referenced to sem_id
} pcb_t;	

typedef struct {
	int process_num;
	double burst; // burst time in ms
	double lClock;
	int shm_id; // run_info_t shm_id 
} run_info_t;

typedef struct {
	int tPut; // throughput
	int turnA; // turnaround
	int waitT; // waiting time
	int cpuU; // cpu utilization

} sim_stats_t;

typedef struct {
	bool isShared; // is it a shared resource
	int requests; // how many processes are requesting
	int allocations; // # of allocations made
	int releases; // # needing to be released
	int available; // # available
	int total; // # num of resource
} rd_t;	

// helper functions
pcb_t* initPcb();
void updateClock(double);
void cleanUpPcbs(pcb_t *pcbs[]);
void cleanUp();
void removePcb(pcb_t *pcbs[], int i);
void scheduleProcess(int,char*,int);
double calcCompletionTime(int); // SRTF
double getCompletionTime(int); // SJN
int findSJN(int);
bool scheduleRR(char*);
void updatePcbs(int usedPcbs[]);
void setBit(int*,int);
void clearBit(int*,int);
int testBit(int*,int);
