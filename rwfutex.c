//	Latch Manager

#define _GNU_SOURCE
#include <linux/futex.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <limits.h>

#define SYS_futex 202

int sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
	return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

#include "rwfutex.h"

//  a phase fair reader/writer lock implementation

void WriteLock (RWLock *lock)
{
uint16_t w, r, tix;
uint32_t prev;

	tix = __sync_fetch_and_add (lock->ticket, 1);

	// wait for our write ticket to come up

	while( 1 ) {
		prev = lock->rw[1];
		if( tix == (uint16_t)prev )
		  break;
		sys_futex( (void *)&lock->rw[1], FUTEX_WAIT_BITSET, prev, NULL, NULL, QueWr );
	}

	// wait for existing readers to drain while new readers queue

	w = PRES | (tix & PHID);
	r = __sync_fetch_and_add (lock->rin, w);

	while( 1 ) {
		prev = lock->rw[0];
		if( r == (uint16_t)(prev >> 16))
		  break;
		sys_futex( (void *)&lock->rw[0], FUTEX_WAIT_BITSET, prev, NULL, NULL, QueWr );
	}
}

void WriteUnlock (RWLock *lock)
{
	// clear writer waiting and phase bit
	//	and advance writer ticket

	__sync_fetch_and_and (lock->rin, ~MASK);
	lock->serving[0]++;

	// are readers waiting?

	if( (*lock->rin & ~MASK) != (*lock->rout & ~MASK) )
	  if( sys_futex( (void *)&lock->rw[0], FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueRd ) )
		return;

	//  is writer waiting (holding a ticket)?

	if( *lock->ticket == *lock->serving )
		return;

	//	are rest of writers waiting for this writer to clear?
	//	(have to wake all of them so ticket holder can proceed.)

	sys_futex( (void *)&lock->rw[1], FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueWr );
}

void ReadLock (RWLock *lock)
{
uint32_t prev;
uint16_t w;

	w = __sync_fetch_and_add (lock->rin, RINC) & MASK;

	if( w )
	  while( 1 ) {
		prev = lock->rw[0];
		if( w != ((uint16_t)prev & MASK))
		  break;
		sys_futex( (void *)&lock->rw[0], FUTEX_WAIT_BITSET, prev, NULL, NULL, QueRd );
	  }
}

void ReadUnlock (RWLock *lock)
{
	__sync_fetch_and_add (lock->rout, RINC);

	// is a writer waiting for this reader to finish?
	// or is a writer waiting for reader cycle to finish?

	if( *lock->rin & PRES )
	  sys_futex( (void *)&lock->rw[0], FUTEX_WAKE_BITSET, 1, NULL, NULL, QueWr );
	else if( *lock->ticket != *lock->serving )
	  sys_futex( (void *)&lock->rw[1], FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueWr );
}


//	lite weight futex Latch Manager

//	wait until write lock mode is clear
//	and add 1 to the share count

void futex_readlock(FutexLatch *latch)
{
FutexLatch prev[1];
uint32_t slept = 0;

  while( 1 ) {
	*prev->value = __sync_fetch_and_add (latch->value, SHARE);

	//  see if exclusive request is already granted
	//	 or if it is reader phase

	if( slept || !prev->wrt )
	  if( !prev->xlock )
		return;

	slept = 1;
	prev->read = 1;
	__sync_fetch_and_sub (latch->value, SHARE);
	__sync_fetch_and_or (latch->value, READ);
	sys_futex( latch->value, FUTEX_WAIT_BITSET, *prev->value, NULL, NULL, QueRd );
  }
}

//	wait for other read and write latches to relinquish

void futex_writelock(FutexLatch *latch)
{
FutexLatch prev[1];
uint32_t slept = 0;

  while( 1 ) {
	*prev->value = __sync_fetch_and_or(latch->value, XCL);

	if( !prev->xlock )			// did we set XCL bit?
	  if( !(prev->share) )	{		// any readers?
	    if( slept )
		  __sync_fetch_and_sub(latch->value, WRT);
		return;
	  } else
		  __sync_fetch_and_and(latch->value, ~XCL);

	if( !slept ) {
		prev->wrt++;
		__sync_fetch_and_add(latch->value, WRT);
	}

	sys_futex (latch->value, FUTEX_WAIT_BITSET, *prev->value, NULL, NULL, QueWr);
	slept = 1;
  }
}

//	try to obtain write lock

//	return 1 if obtained,
//		0 otherwise

int futex_writetry(FutexLatch *latch)
{
FutexLatch prev[1];

	*prev->value = __sync_fetch_and_or(latch->value, XCL);

	//	take write access if all bits are clear

	if( !prev->xlock )
	  if( !prev->share )
		return 1;
	  else
		__sync_fetch_and_and(latch->value, ~XCL);

	return 0;
}

//	clear write mode
//	wake up sleeping readers

void futex_releasewrite(FutexLatch *latch)
{
FutexLatch prev[1];

	*prev->value = __sync_fetch_and_and(latch->value, ~(XCL | READ));

	//	alternate read/write phases

	if( prev->read )
	  if( sys_futex( latch->value, FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueRd ) )
		return;

	if( prev->wrt )
	  sys_futex( latch->value, FUTEX_WAKE_BITSET, 1, NULL, NULL, QueWr );
}

//	decrement reader count
//	wake up sleeping writers

void futex_releaseread(FutexLatch *latch)
{
FutexLatch prev[1];

	*prev->value = __sync_sub_and_fetch(latch->value, SHARE);

	//	alternate read/write phases

	if( prev->wrt ) {
	  if( !prev->share )
		sys_futex( latch->value, FUTEX_WAKE_BITSET, 1, NULL, NULL, QueWr );
	  return;
	}

	if( prev->read ) {
	  __sync_fetch_and_and(latch->value, ~READ);
	  sys_futex (latch->value, FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueRd);
	}
}

#ifdef STANDALONE
#include <stdio.h>
int ThreadCnt = 0;

#include <time.h>
#include <sys/resource.h>

double getCpuTime(int type)
{
struct rusage used[1];
struct timeval tv[1];

	switch( type ) {
	case 0:
		gettimeofday(tv, NULL);
		return (double)tv->tv_sec + (double)tv->tv_usec / 1000000;

	case 1:
		getrusage(RUSAGE_SELF, used);
		return (double)used->ru_utime.tv_sec + (double)used->ru_utime.tv_usec / 1000000;

	case 2:
		getrusage(RUSAGE_SELF, used);
		return (double)used->ru_stime.tv_sec + (double)used->ru_stime.tv_usec / 1000000;
	}

	return 0;
}

#include <pthread.h>

unsigned char Array[256] __attribute__((aligned(64)));
pthread_rwlock_t syslock[1] = {PTHREAD_RWLOCK_INITIALIZER};
FutexLatch futexlatch[1];
RWLock rwlock[1];

enum {
	systemType,
	futexType,
	RWType
} LockType;

typedef struct {
	int threadNo;
} Arg;

void work (int usecs, int shuffle) {
volatile int cnt = usecs * 300;
int first, idx;

	while(shuffle && usecs--) {
	  first = Array[0];
	  for (idx = 0; idx < 255; idx++)
		Array[idx] = Array[idx + 1];

	  Array[255] = first;
	}

	while (cnt--)
#ifdef unix
		__sync_fetch_and_add(&usecs, 1);
#else
		_InterlockedIncrement(&usecs);
#endif
}

void *launch(void *arg) {
Arg *info = (Arg *)arg;
int idx;

	for( idx = 0; idx < 1000000 / ThreadCnt; idx++ ) {
		if (LockType == systemType)
		  pthread_rwlock_rdlock(syslock), work(1, 0), pthread_rwlock_unlock(syslock);
		else if (LockType == futexType)
		  futex_readlock(futexlatch), work(1, 0), futex_releaseread(futexlatch);
		else if (LockType == RWType)
		  ReadLock(rwlock), work(1, 0), ReadUnlock(rwlock);
		else
		  work(1, 0);

		if( (idx & 511) == 0)
		  if (LockType == systemType)
		    pthread_rwlock_wrlock(syslock), work(10, 1), pthread_rwlock_unlock(syslock);
		  else if (LockType == futexType)
			futex_writelock(futexlatch), work(10, 1), futex_releasewrite(futexlatch);
		  else if (LockType == RWType)
			WriteLock(rwlock), work(10, 1), WriteUnlock(rwlock);
		  else
			work(1, 0);
#ifdef DEBUG
		if (!(idx % 100000))
			fprintf(stderr, "Thread %d loop %d\n", info->threadNo, idx);
#endif
	}

	return NULL;
}

int main (int argc, char **argv)
{
pthread_t thread_id[1];
pthread_t *threads;
double start, elapsed;
Arg *args;
int idx;

	for (idx = 0; idx < 256; idx++)
		Array[idx] = idx;

	start = getCpuTime(0);

	if (argc < 2) {
		fprintf(stderr, "Usage: %s #thrds type\n", argv[0]); 
		exit(1);
	}

	ThreadCnt = atoi(argv[1]);
	LockType = atoi(argv[2]);

	args = calloc(ThreadCnt, sizeof(Arg));

	printf("sizeof SystemLatch: %d\n", (int)sizeof(syslock));
	printf("sizeof FutexLatch: %d\n", (int)sizeof(FutexLatch));
	printf("sizeof RWLock: %d\n", (int)sizeof(RWLock));
	threads = malloc (ThreadCnt * sizeof(pthread_t));

	for (idx = 0; idx < ThreadCnt; idx++) {
	  args[idx].threadNo = idx;
	  if (pthread_create(threads + idx, NULL, launch, args + idx))
		fprintf(stderr, "Unable to create thread %d, errno = %d\n", idx, errno);
	}

	// 	wait for termination

	for( idx = 0; idx < ThreadCnt; idx++ )
		pthread_join (threads[idx], NULL);

	for( idx = 0; idx < 256; idx++)
	  if (Array[idx] != (unsigned char)(Array[(idx+1) % 256] - 1))
		fprintf (stderr, "Array out of order\n");

	elapsed = getCpuTime(0) - start;
	fprintf(stderr, " real %.3fus\n", elapsed);
	elapsed = getCpuTime(1);
	fprintf(stderr, " user %.3fus\n", elapsed);
	elapsed = getCpuTime(2);
	fprintf(stderr, " sys  %.3fus\n", elapsed);
}
#endif
