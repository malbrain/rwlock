//	Latch Manager

#define _GNU_SOURCE
#include <linux/futex.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <limits.h>
#define SYS_futex 202
typedef unsigned short ushort;

//	mode & definition for latch implementation

enum {
	QueRd = 1,	// reader queue
	QueWr = 2	// writer queue
} RWQueue;

typedef struct {
  union {
	struct {
	  ushort rin[1];
	  ushort rout[1];
	  ushort serving[1];
	  ushort ticket[1];
	} counts[1];
	uint rw[2];
  };
} RWLock;

//	define rin bits
#define PHID 0x1
#define PRES 0x2
#define MASK 0x3
#define RINC 0x4

//	lite weight spin latch

typedef struct {
  union {
	struct {
	  uint xlock:1;		// writer has exclusive lock
	  uint share:15;	// count of readers with lock
	  uint read:1;		// readers are waiting
	  uint wrt:15;		// count of writers waiting
	} bits[1];
	uint value[1];
  };
} FutexLatch;

#define XCL 1
#define SHARE 2
#define READ (SHARE * 32768)
#define WRT (READ * 2)

int sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
	return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

//  a phase fair reader/writer lock implementation

void WriteLock (RWLock *lock)
{
ushort w, r, tix;
uint prev;

	tix = __sync_fetch_and_add (lock->counts->ticket, 1);

	// wait for our ticket to come up

	while( 1 ) {
		prev = lock->rw[1];
		if( tix == (ushort)prev )
		  break;
		sys_futex( &lock->rw[1], FUTEX_WAIT_BITSET, prev, NULL, NULL, QueWr );
	}

	w = PRES | (tix & PHID);
	r = __sync_fetch_and_add (lock->counts->rin, w);

	while( 1 ) {
		prev = lock->rw[0];
		if( r == (ushort)prev )
		  break;
		sys_futex( &lock->rw[0], FUTEX_WAIT_BITSET, prev, NULL, NULL, QueWr );
	}
}

void WriteRelease (RWLock *lock)
{
	__sync_fetch_and_and (lock->counts->rin, ~MASK);
	lock->counts->serving[0]++;

	if( (*lock->counts->rin & ~MASK) != (*lock->counts->rout & ~MASK) )
	  if( sys_futex( &lock->rw[0], FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueRd ) )
		return;

	if( *lock->counts->ticket != *lock->counts->serving )
	  sys_futex( &lock->rw[1], FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueWr );
}

void ReadLock (RWLock *lock)
{
uint prev;
ushort w;

	w = __sync_fetch_and_add (lock->counts->rin, RINC) & MASK;

	if( w )
	  while( 1 ) {
		prev = lock->rw[0];
		if( w != (ushort)prev )
		  break;
		sys_futex( &lock->rw[0], FUTEX_WAIT_BITSET, prev, NULL, NULL, QueRd );
	  }
}

void ReadRelease (RWLock *lock)
{
	__sync_fetch_and_add (lock->counts->rout, RINC);

	if( *lock->counts->ticket == *lock->counts->serving )
		return;

	if( *lock->counts->rin & PRES )
	  if( sys_futex( &lock->rw[0], FUTEX_WAKE_BITSET, 1, NULL, NULL, QueWr ) )
		return;

	sys_futex( &lock->rw[1], FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueWr );
}


//	lite weight futex Latch Manager

//	wait until write lock mode is clear
//	and add 1 to the share count

void futex_readlock(FutexLatch *latch)
{
FutexLatch prev[1];
uint slept = 0;

  while( 1 ) {
	*prev->value = __sync_fetch_and_add (latch->value, SHARE);

	//  see if exclusive request is already granted
	//	 or if it is reader phase

	if( slept || !prev->bits->wrt )
	  if( !prev->bits->xlock )
		return;

	slept = 1;
	prev->bits->read = 1;
	__sync_fetch_and_sub (latch->value, SHARE);
	__sync_fetch_and_or (latch->value, READ);
	sys_futex( latch->value, FUTEX_WAIT_BITSET, *prev->value, NULL, NULL, QueRd );
  }
}

//	wait for other read and write latches to relinquish

void futex_writelock(FutexLatch *latch)
{
FutexLatch prev[1];
uint slept = 0;

  while( 1 ) {
	*prev->value = __sync_fetch_and_or(latch->value, XCL);

	if( !prev->bits->xlock )			// did we set XCL bit?
	  if( !(prev->bits->share) )	{		// any readers?
	    if( slept )
		  __sync_fetch_and_sub(latch->value, WRT);
		return;
	  } else
		  __sync_fetch_and_and(latch->value, ~XCL);

	if( !slept ) {
		prev->bits->wrt++;
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

	if( !prev->bits->xlock )
	  if( !prev->bits->share )
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

	if( prev->bits->read )
	  if( sys_futex( latch->value, FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueRd ) )
		return;

	if( prev->bits->wrt )
	  sys_futex( latch->value, FUTEX_WAKE_BITSET, 1, NULL, NULL, QueWr );
}

//	decrement reader count
//	wake up sleeping writers

void futex_releaseread(FutexLatch *latch)
{
FutexLatch prev[1];

	*prev->value = __sync_sub_and_fetch(latch->value, SHARE);

	//	alternate read/write phases

	if( prev->bits->wrt ) {
	  if( !prev->bits->share )
		sys_futex( latch->value, FUTEX_WAKE_BITSET, 1, NULL, NULL, QueWr );
	  return;
	}

	if( prev->bits->read ) {
	  __sync_fetch_and_and(latch->value, ~READ);
	  sys_futex (latch->value, FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueRd);
	}
}

int main (int argc, char *argv)
{
FutexLatch latch[1];
RWLock lock[1];
int idx;

	memset ((void *)lock, 0, sizeof(RWLock));
	memset ((void *)latch, 0, sizeof(FutexLatch));

	for( idx = 0; idx < 1000000000; idx++ ) {
		futex_readlock(latch);
		futex_releaseread(latch);
	}
}
