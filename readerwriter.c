//  a phase fair reader/writer lock implementation, version 2
//	by Karl Malbrain, malbrain@cal.berkeley.edu
//	20 JUN 2016

#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <errno.h>

#include "readerwriter.h"

#ifdef linux
#include <linux/futex.h>
#define SYS_futex 202

int sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
	return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}
#endif

#ifndef _WIN32
#include <sched.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef DEBUG
int NanoCnt[1];
#endif

#ifdef unix
#define pause() asm volatile("pause\n": : : "memory")

void lock_sleep (int cnt) {
struct timespec ts[1];

	ts->tv_sec = 0;
	ts->tv_nsec = cnt/2;
	nanosleep(ts, NULL);
#ifdef DEBUG
	__sync_fetch_and_add(NanoCnt, 1);
#endif
}

int lock_spin (int *cnt) {
volatile int idx;

	if (!*cnt)
	  *cnt = 8;
	else if (*cnt < 8192)
	  *cnt += *cnt / 4;

	if (*cnt < 256 )
	  for (idx = 0; idx < *cnt; idx++)
		pause();
	else if (*cnt < 8192)
		sched_yield();
	else
		return 1;

	return 0;
}
#else
// Replacement for nanosleep on Windows.

void nanosleep (const uint32_t ns, HANDLE *timer)
{
	LARGE_INTEGER sleepTime;

	sleepTime.QuadPart = ns / 100;

	if (!*timer)
		*timer = CreateWaitableTimer (NULL, TRUE, NULL);

	SetWaitableTimer (*timer, &sleepTime, 0, NULL, NULL, 0);
	WaitForSingleObject (*timer, INFINITE);
}

int lock_spin (uint32_t *cnt) {
volatile int idx;

	if (*cnt < 8192)
	  *cnt += *cnt / 8;

	if (*cnt < 1024 )
	  for (idx = 0; idx < *cnt; idx++)
		YieldProcessor();
	else if (*cnt < 8192)
		SwitchToThread();
	else
		return 1;

	return 0;
}
#endif

//	reader/writer lock implementation

void WriteLock1 (RWLock1 *lock)
{
uint32_t spinCount = 0;
#ifndef _WIN32
	uint16_t me = __sync_fetch_and_add(lock->tix, 1);

	while (me != *lock->writers)
	  if (lock_spin(&spinCount))
		lock_sleep (spinCount);
#else
HANDLE timer = NULL;

	uint16_t me = _InterlockedIncrement16(lock->tix)-1;

	while (me != *lock->writers)
	  if (lock_spin(&spinCount))
		nanosleep(spinCount, &timer);

  if (timer)
	CloseHandle(timer);
#endif
}

void WriteUnlock1 (RWLock1 *lock)
{
RWLock1 tmp;

	tmp.urw = lock->urw;
	(*tmp.writers)++;
	(*tmp.readers)++;
	lock->urw = tmp.urw;
}

void ReadLock1 (RWLock1 *lock)
{
uint32_t spinCount = 0;
#ifndef _WIN32
	uint16_t me = __sync_fetch_and_add(lock->tix, 1);

	while (me != *lock->readers)
	  if (lock_spin(&spinCount))
		lock_sleep (spinCount);

	__sync_fetch_and_add(lock->readers, 1);
#else
HANDLE timer = NULL;

	uint16_t me = _InterlockedIncrement16(lock->tix) - 1;

	while (me != *lock->readers)
	  if (lock_spin(&spinCount))
		nanosleep(spinCount, &timer);

  if (timer)
	CloseHandle(timer);

	_InterlockedIncrement16(lock->readers);
#endif
}

void ReadUnlock1 (RWLock1 *lock)
{
#ifndef _WIN32
	__sync_fetch_and_add(lock->writers, 1);
#else
	_InterlockedIncrement16(lock->writers);
#endif
}

void latch_mutexlock(MutexLatch *latch)
{
uint32_t idx, waited = 0;
MutexLatch prev[1];

 while( 1 ) {
  for( idx = 0; idx < 100; idx++ ) {
#ifdef unix
	*prev->value = __sync_fetch_and_or (latch->value, 1);
#else
	*prev->value = _InterlockedOr (latch->value, 1);
#endif
	if( !*prev->xcl ) {
	  if( waited )
#ifdef unix
		__sync_fetch_and_sub (latch->waiters, 1);
#else
		_InterlockedDecrement16 (latch->waiters);
#endif
	  return;
	}
  }

  if( !waited ) {
#ifdef unix
	__sync_fetch_and_add (latch->waiters, 1);
#else
	_InterlockedIncrement16 (latch->waiters);
#endif
	*prev->waiters += 1;
	waited++;
  }

#ifdef linux
  sys_futex ((void *)latch->value, FUTEX_WAIT, *prev->value, NULL, NULL, 0);
#elif defined(_WIN32)
  {
  uint32_t spinCount = 0;
  HANDLE timer = NULL;

	if (lock_spin(&spinCount))
	  nanosleep (spinCount, &timer);

	if (timer)
	  CloseHandle(timer);
  }
#else
  {
  uint32_t spinCount = 0;
	if (lock_spin(&spinCount))
	  lock_sleep (spinCount);
  }
#endif
 }
}

int latch_mutextry(MutexLatch *latch)
{
#ifdef unix
	return !__sync_lock_test_and_set (latch->xcl, 1);
#else
	return !_InterlockedExchange16 (latch->xcl, 1);
#endif
}

void latch_releasemutex(MutexLatch *latch)
{
MutexLatch prev[1];

#ifdef unix
	*prev->value = __sync_fetch_and_and (latch->value, 0xffff0000);
#else
	*prev->value = _InterlockedAnd (latch->value, 0xffff0000);
#endif

	if( *prev->waiters )
#ifdef linux
		sys_futex( (void *)latch->value, FUTEX_WAKE, 1, NULL, NULL, 0 );
#elif defined(_WIN32)
  {
  uint32_t spinCount = 0;
  HANDLE timer = NULL;

	if (lock_spin(&spinCount))
	  nanosleep (spinCount, &timer);

	if (timer)
	  CloseHandle(timer);
  }
#else
  {
  uint32_t spinCount = 0;
	if (lock_spin(&spinCount))
	  lock_sleep (spinCount);
  }
#endif
}

//	reader/writer lock implementation
//	mutex based

void WriteLock2 (RWLock2 *lock)
{
	latch_mutexlock (lock->xcl);
	latch_mutexlock (lock->wrt);
	latch_releasemutex (lock->xcl);
}

void WriteUnlock2 (RWLock2 *lock)
{
	latch_releasemutex (lock->wrt);
}

void ReadLock2 (RWLock2 *lock)
{
	latch_mutexlock (lock->xcl);

#ifdef unix
	if( !__sync_fetch_and_add (lock->readers, 1) )
#else
	if( !(_InterlockedIncrement16 (lock->readers)-1) )
#endif
	latch_mutexlock (lock->wrt);
	latch_releasemutex (lock->xcl);
}

void ReadUnlock2 (RWLock2 *lock)
{
#ifdef unix
	if( __sync_fetch_and_sub (lock->readers, 1) == 1 )
#else
	if( !_InterlockedDecrement16 (lock->readers) )
#endif
		latch_releasemutex (lock->wrt);
}

void WriteLock3 (RWLock3 *lock)
{
uint32_t spinCount = 0;
uint16_t w, r, tix;
#ifdef _WIN32
HANDLE timer = NULL;
#endif

#ifdef unix
	tix = __sync_fetch_and_add (lock->ticket, 1);
#else
	tix = _InterlockedExchangeAdd16 (lock->ticket, 1);
#endif
	// wait for our ticket to come up

	while( tix != lock->serving[0] )
	  if (lock_spin(&spinCount))
#ifdef _WIN32
	    nanosleep (spinCount, &timer);
#else
		lock_sleep (spinCount);
#endif

	w = PRES | (tix & PHID);
#ifdef  unix
	r = __sync_fetch_and_add (lock->rin, w);
#else
	r = _InterlockedExchangeAdd16 (lock->rin, w) - w;
#endif

	while( r != *lock->rout )
	  if (lock_spin(&spinCount))
#ifdef _WIN32
	    nanosleep (spinCount, &timer);
#else
		lock_sleep (spinCount);
#endif
#ifdef _WIN32
	if( timer )
		CloseHandle(timer);
#endif
}

void WriteUnlock3 (RWLock3 *lock)
{
#ifdef unix
	__sync_fetch_and_and (lock->rin, ~MASK);
#else
	_InterlockedAnd16 (lock->rin, ~MASK);
#endif
	lock->serving[0]++;
}

void ReadLock3 (RWLock3 *lock)
{
uint32_t spinCount = 0;
uint16_t w;
#ifdef _WIN32
HANDLE timer = NULL;
#endif

#ifdef unix
	w = __sync_fetch_and_add (lock->rin, RINC) & MASK;
#else
	w = _InterlockedExchangeAdd16 (lock->rin, RINC) & MASK;
#endif
	if( w )
	  while( w == (*lock->rin & MASK) )
	   if (lock_spin(&spinCount))
#ifndef _WIN32
		lock_sleep (spinCount);
#else
	    nanosleep (spinCount, &timer);
#endif
#ifdef _WIN32
	if( timer )
		CloseHandle(timer);
#endif
}

void ReadUnlock3 (RWLock3 *lock)
{
#ifdef unix
	__sync_fetch_and_add (lock->rout, RINC);
#else
	_InterlockedExchangeAdd16 (lock->rout, RINC);
#endif
}

#ifdef STANDALONE
#include <stdio.h>
int ThreadCnt = 0;

#ifndef unix
double getCpuTime(int type)
{
FILETIME crtime[1];
FILETIME xittime[1];
FILETIME systime[1];
FILETIME usrtime[1];
SYSTEMTIME timeconv[1];
double ans = 0;

	memset (timeconv, 0, sizeof(SYSTEMTIME));

	switch( type ) {
	case 0:
		GetSystemTimeAsFileTime (xittime);
		FileTimeToSystemTime (xittime, timeconv);
		ans = (double)timeconv->wDayOfWeek * 3600 * 24;
		break;
	case 1:
		GetProcessTimes (GetCurrentProcess(), crtime, xittime, systime, usrtime);
		FileTimeToSystemTime (usrtime, timeconv);
		break;
	case 2:
		GetProcessTimes (GetCurrentProcess(), crtime, xittime, systime, usrtime);
		FileTimeToSystemTime (systime, timeconv);
		break;
	}

	ans += (double)timeconv->wHour * 3600;
	ans += (double)timeconv->wMinute * 60;
	ans += (double)timeconv->wSecond;
	ans += (double)timeconv->wMilliseconds / 1000;
	return ans;
}
#else
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
#endif

#ifdef unix
unsigned char Array[256] __attribute__((aligned(64)));
#include <pthread.h>
pthread_rwlock_t lock0[1] = {PTHREAD_RWLOCK_INITIALIZER};
#else
__declspec(align(64)) unsigned char Array[256];
SRWLOCK lock0[1] = {SRWLOCK_INIT};
#endif
RWLock1 lock1[1];
RWLock2 lock2[1];
RWLock3 lock3[1];

enum {
	systemType,
	RW1Type,
	RW2Type,
	RW3Type
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

#ifdef _WIN32
DWORD WINAPI launch(Arg *arg) {
#else

void *launch(void *vals) {
Arg *arg = (Arg *)vals;
#endif
int idx;

	for( idx = 0; idx < 1000000 / ThreadCnt; idx++ ) {
	  if (LockType == systemType)
#ifdef unix
		pthread_rwlock_rdlock(lock0), work(1, 0), pthread_rwlock_unlock(lock0);
#else
		AcquireSRWLockShared(lock0), work(1, 0), ReleaseSRWLockShared(lock0);
#endif
	  else if (LockType == RW1Type)
		ReadLock1(lock1), work(1, 0), ReadUnlock1(lock1);
	  else if (LockType == RW2Type)
		ReadLock2(lock2), work(1, 0), ReadUnlock2(lock2);
	  else if (LockType == RW3Type)
		ReadLock3(lock3), work(1, 0), ReadUnlock3(lock3);
	  if( (idx & 511) == 0)
	    if (LockType == systemType)
#ifdef unix
		  pthread_rwlock_wrlock(lock0), work(10, 1), pthread_rwlock_unlock(lock0);
#else
		  AcquireSRWLockExclusive(lock0), work(10, 1), ReleaseSRWLockExclusive(lock0);
#endif
	  	else if (LockType == RW1Type)
		  WriteLock1(lock1), work(10, 1), WriteUnlock1(lock1);
	  	else if (LockType == RW2Type)
		  WriteLock2(lock2), work(10, 1), WriteUnlock2(lock2);
	  	else if (LockType == RW3Type)
		  WriteLock3(lock3), work(10, 1), WriteUnlock3(lock3);
#ifdef DEBUG
	  if (!(idx % 100000))
		fprintf(stderr, "Thread %d loop %d\n", arg->threadNo, idx);
#endif
	}

#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}

int main (int argc, char **argv)
{
double start, elapsed;
Arg *args;
int idx;

#ifdef unix
pthread_t *threads;
#else
DWORD thread_id[1];
HANDLE *threads;
#endif

	for (idx = 0; idx < 256; idx++)
		Array[idx] = idx;

	start = getCpuTime(0);

	if (argc < 2) {
		fprintf(stderr, "Usage: %s #thrds\n", argv[0]); 
		exit(1);
	}

	ThreadCnt = atoi(argv[1]);
	LockType = atoi(argv[2]);

	args = calloc(ThreadCnt, sizeof(Arg));

	printf("sizeof RWLock0: %d\n", (int)sizeof(lock0));
	printf("sizeof RWLock1: %d\n", (int)sizeof(lock1));
	printf("sizeof RWLock2: %d\n", (int)sizeof(lock2));
	printf("sizeof RWLock3: %d\n", (int)sizeof(lock3));
#ifdef unix
	threads = malloc (ThreadCnt * sizeof(pthread_t));
#else
	threads = malloc (ThreadCnt * sizeof(HANDLE));
#endif
	for (idx = 0; idx < ThreadCnt; idx++) {
	  args[idx].threadNo = idx;
#ifdef _WIN32
	  do threads[idx] = CreateThread(NULL, 0, (PTHREAD_START_ROUTINE)launch, args + idx, 0, thread_id);
	  while ((int64_t)threads[idx] == -1 && (SwitchToThread(), 1));
#else
	  if (pthread_create(threads + idx, NULL, launch, (void *)(args + idx)))
		fprintf(stderr, "Unable to create thread %d, errno = %d\n", idx, errno);

#endif
	}

	// 	wait for termination

#ifdef unix
	for (idx = 0; idx < ThreadCnt; idx++)
		pthread_join (threads[idx], NULL);
#else

	for (idx = 0; idx < ThreadCnt; idx++) {
		WaitForSingleObject (threads[idx], INFINITE);
		CloseHandle(threads[idx]);
	}
#endif
	for( idx = 0; idx < 256; idx++)
	  if (Array[idx] != (unsigned char)(Array[(idx+1) % 256] - 1))
		fprintf (stderr, "Array out of order\n");

	elapsed = getCpuTime(0) - start;
	fprintf(stderr, " real %.3fus\n", elapsed);
	elapsed = getCpuTime(1);
	fprintf(stderr, " user %.3fus\n", elapsed);
	elapsed = getCpuTime(2);
	fprintf(stderr, " sys  %.3fus\n", elapsed);
#ifdef DEBUG
	fprintf(stderr, " nano %d\n", NanoCnt[0]);
#endif
}
#endif

