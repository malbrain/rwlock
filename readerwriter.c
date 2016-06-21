//  a phase fair reader/writer lock implementation, version 2
//	by Karl Malbrain, malbrain@cal.berkeley.edu
//	20 JUN 2016

#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

typedef union {
  struct {
	volatile uint16_t readers[1];
	volatile uint16_t writers[1];
	volatile uint16_t tix[1];
  };
  volatile uint32_t urw;
} RWLock;

void writeLock (RWLock *lock);
void writeUnlock (RWLock *lock);
void readLock (RWLock *lock);
void readUnlock (RWLock *lock);

#ifndef _WIN32
#include <sched.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

//	reader/writer lock implementation

void writeLock (RWLock *lock)
{
#ifndef _WIN32
	uint16_t me = __sync_fetch_and_add(lock->tix, 1);

	while (me != *lock->writers)
		relax();
#else
	uint16_t me = InterlockedIncrement16(lock->tix)-1;

	while (me != *lock->writers)
		YieldProcessor();
#endif
}

void writeUnlock (RWLock *lock)
{
	RWLock tmp;

	tmp.urw = lock->urw;
	(*tmp.writers)++;
	(*tmp.readers)++;
	lock->urw = tmp.urw;
}

void readLock (RWLock *lock)
{
#ifndef _WIN32
	uint16_t me = __sync_fetch_and_add(lock->tix, 1);

	while (me != *lock->readers)
		relax();

	__sync_fetch_and_add(lock->readers, 1);
#else
	uint16_t me = InterlockedIncrement16(lock->tix) - 1;

	while (me != *lock->readers)
		YieldProcessor();

	InterlockedIncrement16(lock->readers);
#endif
}

void readUnlock (RWLock *lock)
{
#ifndef _WIN32
	__sync_fetch_and_add(lock->writers, 1);
#else
	InterlockedIncrement16(lock->writers);
#endif
}

#ifdef STANDALONE
#include <stdio.h>

#ifdef _WIN32
DWORD WINAPI launch(RWLock *lock) {
#else
#include <pthread.h>

void *launch(RWLock *lock) {
#endif

	for( int idx = 0; idx < 1000000000; idx++ ) {
		readLock(lock), readUnlock(lock);
		if( (idx & 31) == 0)
			writeLock(lock), writeUnlock(lock);
	}

#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}

int main (int argc, char **argv)
{
int count;
RWLock lock[1];
#ifdef unix
pthread_t *threads;
#else
HANDLE *threads;
#endif
#ifdef _WIN32
	DWORD thread_id[1];
#else
	pthread_t thread_id[1];
#endif

	if (argc < 2) {
		fprintf(stderr, "Usage: %s #thrds\n", argv[0]); 
		exit(1);
	}

	count = atoi(argv[1]);
	memset (lock, 0, sizeof(RWLock));

	printf("sizeof RWLock: %d\n", (int)sizeof(lock));
#ifdef unix
	threads = malloc (count * sizeof(pthread_t));
#else
	threads = malloc (count * sizeof(HANDLE));
#endif
	for (int idx = 0; idx < count; idx++) {
#ifdef _WIN32
		threads[idx] = CreateThread(NULL, 0, (PTHREAD_START_ROUTINE)launch, lock, 0, thread_id);
#else
		pthread_create(thread_id, NULL, launch, lock);
#endif
	}

	// 	wait for termination

#ifdef unix
	for( int idx = 0; idx < count; idx++ )
		pthread_join (threads[idx], NULL);
#else
	WaitForMultipleObjects (count, threads, TRUE, INFINITE);

	for( int idx = 0; idx < count; idx++ )
		CloseHandle(threads[idx]);
#endif
}
#endif

