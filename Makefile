CC = gcc
CFLAGS = -g
RM = rm

EXEM = oss 
EXES = userProcess 
#EXEPS = procSch 
SRCSM = oss.c semaphore.c
SRCSS = userProcess.c semaphore.c
#SRCSPS = processScheduler.c
OBJSM = ${SRCSM:.c=.o}
OBJSS = ${SRCSS:.c=.o}
#OBJSPS = ${SRCSPS:.c=.o}

.c:.o
	$(CC) $(CFLAGS) -c $<

all : $(EXEM) $(EXES) #$(EXEPS)

$(EXEM) : $(OBJSM)
	$(CC) -o $@ $(OBJSM)

$(OBJSM) : oss.h semaphore.h

$(EXES) : $(OBJSS)
	$(CC) -o $@ $(OBJSS)

$(OBJSS) : oss.h semaphore.h

#$(EXEPS) : $(OBJSPS)
#	$(CC) -o $@ $(OBJSPS)

#$(OBJSPS) : oss.h	semaphore.h

clean :
	$(RM) -f $(EXES) $(EXEM) $(OBJSS) $(OBJSM) endStats.txt




