#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
//#include <sys/sem.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>

// Brett Lindsay
// cs4760 assignment 5
// oss.h

typedef enum {false, true} bool;

// info for num of a resource from which process
typedef struct {
	int res;
	int num;
	int pNum;
	bool isClaim;
	bool isDone;
} info_t;

// a process control block
typedef struct {
	info_t action;
	int  maxClaim[20]; // value = max claim for that res. index
	int claimed[20]; // value = claimed for that res. index
	int total; // total claimed
	double totalSysTime; // time in system
	double totalCpuTime; // time spent computing
	double cTime; // create time
	double dTime; // destroy time
	bool isCompleted;
	int shm_id; // ref to id for shm
	int pid; // pid for fork() return value
	//int sem_id; // semaphore for blocking self to wait for resource allocation
	sem_t sem;
} pcb_t;	

// a resource descriptor
typedef struct {
	bool isShared; // is it a shared resource
	// info_t struct for each possible process in pcb
	int requests[18]; // outstanding requests
	int available; // value = # available for that resource
	int total;
} rd_t;	

// logical clock and resource descriptors in oss 
typedef struct {
	rd_t rds[20]; // array of resource descripts
	double lClock;
	//int sem_id; // semaphore for lClock access and rds access
	sem_t sem;
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
pcb_t* initPcb(int);
void updateClock(double);
void cleanUpPcbs(pcb_t *pcbs[]);
void cleanUp();
void removePcb(pcb_t *pcbs[], int i);
void updatePcbs(int usedPcbs[]);
void setBit(int*,int);
void clearBit(int*,int);
int testBit(int*,int);
void initRunInfo(int);
void deadlock();
