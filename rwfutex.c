//	Latch Manager

#define _GNU_SOURCE
#include <linux/futex.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>
#define SYS_futex 202
typedef unsigned short ushort;

//	mode & definition for latch implementation

enum {
	QueRd = 1,	// reader queue
	QueWr = 2	// writer queue
} RWQueue;

volatile typedef struct {
	ushort rin[1];
	ushort rout[1];
	ushort ticket[1];
	ushort serving[1];
} RWLock;

//	define rin bits
#define PHID 0x1
#define PRES 0x2
#define MASK 0x3
#define RINC 0x4

int sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
	return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

//  a phase fair reader/writer lock implementation

void WriteLock (RWLock *lock)
{
ushort w, r, tix;
uint prev;

	tix = __sync_fetch_and_add (lock->ticket, 1);

	// wait for our ticket to come up

	while( 1 ) {
		prev = *lock->ticket | *lock->serving << 16;
		if( tix == prev >> 16 )
		  break;
		sys_futex( (uint *)lock->ticket, FUTEX_WAIT_BITSET, prev, NULL, NULL, QueWr );
	}

	w = PRES | (tix & PHID);
	r = __sync_fetch_and_add (lock->rin, w);

	while( 1 ) {
		prev = *lock->rin | *lock->rout << 16;
		if( r == prev >> 16 )
		  break;
		sys_futex( (uint *)lock->rin, FUTEX_WAIT_BITSET, prev, NULL, NULL, QueWr );
	}
}

void WriteRelease (RWLock *lock)
{
	__sync_fetch_and_and (lock->rin, ~MASK);
	lock->serving[0]++;

	if( (*lock->rin & ~MASK) != (*lock->rout & ~MASK) )
	  if( sys_futex( (uint *)lock->rin, FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueRd ) )
		return;

	if( *lock->ticket != *lock->serving )
	  sys_futex( (uint *)lock->ticket, FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueWr );
}

void ReadLock (RWLock *lock)
{
uint prev;
ushort w;

	w = __sync_fetch_and_add (lock->rin, RINC) & MASK;

	if( w )
	  while( 1 ) {
		prev = *lock->rin | *lock->rout << 16;
		if( w != (prev & MASK) )
		  break;
		sys_futex( (uint *)lock->rin, FUTEX_WAIT_BITSET, prev, NULL, NULL, QueRd );
	  }
}

void ReadRelease (RWLock *lock)
{
	__sync_fetch_and_add (lock->rout, RINC);

	if( *lock->ticket == *lock->serving )
		return;

	if( *lock->rin & PRES )
	  if( sys_futex( (uint *)lock->rin, FUTEX_WAKE_BITSET, 1, NULL, NULL, QueWr ) )
		return;

	sys_futex( (uint *)lock->ticket, FUTEX_WAKE_BITSET, INT_MAX, NULL, NULL, QueWr );
}


int main (int argc, char *argv)
{
RWLock lock[1];
int idx;

	memset (lock, 0, sizeof(RWLock));

	for( idx = 0; idx < 1000000000; idx++ ) {
		ReadLock(lock);
		ReadRelease(lock);
	}
}
