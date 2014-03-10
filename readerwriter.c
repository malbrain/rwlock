//  a phase fair reader/writer lock implementation
//	07 MAR 14

typedef unsigned short ushort;
#include <stdlib.h>
#include <memory.h>

typedef struct {
	ushort rin[1];
	ushort rout[1];
	ushort ticket[1];
	ushort serving[1];
} RWLock;

#define PHID 0x1
#define PRES 0x2
#define MASK 0x3
#define RINC 0x4

void WriteLock (RWLock *lock)
{
ushort w, r, tix;

#ifdef unix
	tix = __sync_fetch_and_add (lock->ticket, 1);
#else
	_InterlockedAdd16 (lock->ticket, 1);
#endif
	// wait for our ticket to come up

	while( tix != lock->serving[0] )
		sched_yield();

	w = PRES | (tix & PHID);
#ifdef  unix
	r = __sync_fetch_and_add (lock->rin, w);
#else
	r = _InterlockedAdd16 (lock->rin, w);
#endif
	while( r != *lock->rout )
#ifdef unix
		sched_yield();
#else
		SwitchToThread();
#endif
}

void WriteRelease (RWLock *lock)
{
#ifdef unix
	__sync_fetch_and_and (lock->rin, ~MASK);
#else
	_InterlockedAnd16 (lock->rin, ~MASK);
#endif
	lock->serving[0]++;
}

void ReadLock (RWLock *lock)
{
ushort w;
#ifdef unix
	w = __sync_fetch_and_add (lock->rin, RINC) & MASK;
#else
	w = _InterlockedAdd16 (lock->rin, RINC) & MASK;
#endif
	if( w )
	  while( w == (*lock->rin & MASK) )
		sched_yield ();
}

void ReadRelease (RWLock *lock)
{
#ifdef unix
	__sync_fetch_and_add (lock->rout, RINC);
#else
	_InterlockedAdd16 (lock->rout, RINC);
#endif
}

//	lite weight spin lock Latch Manager

volatile typedef struct {
	ushort exclusive:1;
	ushort pending:1;
	ushort share:14;
} BtSpinLatch;

#define XCL 1
#define PEND 2
#define BOTH 3
#define SHARE 4

//	wait until write lock mode is clear
//	and add 1 to the share count

void bt_spinreadlock(BtSpinLatch *latch)
{
ushort prev;

  do {
#ifdef unix
	prev = __sync_fetch_and_add ((ushort *)latch, SHARE);
#else
	prev = _InterlockedAdd16((ushort *)latch, SHARE);
#endif
	//  see if exclusive request is granted or pending

	if( !(prev & BOTH) )
		return;
#ifdef unix
	prev = __sync_fetch_and_add ((ushort *)latch, -SHARE);
#else
	prev = _InterlockedAdd16((ushort *)latch, -SHARE);
#endif
#ifdef  unix
  } while( sched_yield(), 1 );
#else
  } while( SwitchToThread(), 1 );
#endif
}

//	wait for other read and write latches to relinquish

void bt_spinwritelock(BtSpinLatch *latch)
{
ushort prev;

  do {
#ifdef  unix
	prev = __sync_fetch_and_or((ushort *)latch, PEND | XCL);
#else
	_InterlockedOr16((ushort *)latch, PEND | XCL);
#endif
	if( !(prev & XCL) )
	  if( !(prev & ~BOTH) )
		return;
	  else
#ifdef unix
		__sync_fetch_and_and ((ushort *)latch, ~XCL);
#else
		_InterlockedAnd16((ushort *)latch, ~XCL);
#endif
#ifdef  unix
  } while( sched_yield(), 1 );
#else
  } while( SwitchToThread(), 1 );
#endif
}

//	try to obtain write lock

//	return 1 if obtained,
//		0 otherwise

int bt_spinwritetry(BtSpinLatch *latch)
{
ushort prev;

#ifdef  unix
	prev = __sync_fetch_and_or((ushort *)latch, XCL);
#else
	_InterlockedOr16((ushort *)latch, XCL);
#endif
	//	take write access if all bits are clear

	if( !(prev & XCL) )
	  if( !(prev & ~BOTH) )
		return 1;
	  else
#ifdef unix
		__sync_fetch_and_and ((ushort *)latch, ~XCL);
#else
		_InterlockedAnd16((ushort *)latch, ~XCL);
#endif
	return 0;
}

//	clear write mode

void bt_spinreleasewrite(BtSpinLatch *latch)
{
#ifdef unix
	__sync_fetch_and_and((ushort *)latch, ~BOTH);
#else
	_InterlockedAnd16((ushort *)latch, ~BOTH);
#endif
}

//	decrement reader count

void bt_spinreleaseread(BtSpinLatch *latch)
{
#ifdef unix
	__sync_fetch_and_add((ushort *)latch, -SHARE);
#else
	_InterlockedAdd16((ushort *)latch, -SHARE);
#endif
}

int main (int argc, char *argv)
{
BtSpinLatch latch[1];
RWLock lock[1];
int idx;

	memset (lock, 0, sizeof(RWLock));
	memset (latch, 0, sizeof(BtSpinLatch));
	for( idx = 0; idx < 1000000000; idx++ ) {
		bt_spinreadlock(latch);
		bt_spinreleaseread(latch);
	}
}
