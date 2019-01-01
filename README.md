rwlock
======

Reader Writer Locks

Initialze locks to all bytes zero. Compile with -D STANDALONE to add benchmarking main module.  Run the benchmark with two parameters:  # of threads and lock-type.  The benchmark consists of 1000000 lock/work/unlock calls divided by the number of threads specified.  500x Ratio of Read Locks to Write Locks.

readerwriter.c: linux/Windows spinlock/phase-fair:

    0: type 0   pthread/SRW system rwlocks
    1: type 1	Phase-Fair FIFO simple rwlock
    2: type 2	Mutex based, neither FIFO nor Fair
    3: type 3	FIFO and Phase-Fair Brandenburg spin lock

    Usage: ./readerwriter #thrds lockType
    0: sizeof RWLock0: 56
    1: sizeof RWLock1: 8
    2: sizeof RWLock2: 4
    3: sizeof RWLock3: 8

sample Windows 10 64bit output:

    C:\Users\Owner\Source\Repos\malbrain\rwlock>x64\release\rwlock 20 3
     real 0.039us
     user 0.328us
     sys  0.000us
     nanosleeps 40

    C:\Users\Owner\Source\Repos\malbrain\rwlock>x64\release\rwlock 20 2
     real 0.052us
     user 0.375us
     sys  0.000us
     nanosleeps 1316

    C:\Users\Owner\Source\Repos\malbrain\rwlock>x64\release\rwlock 20 1
     real 0.689us
     user 5.406us
     sys  0.000us
     nanosleeps 49886

    C:\Users\Owner\Source\Repos\malbrain\rwlock>x64\release\rwlock 20 0
     real 0.017us
     user 0.125us
     sys  0.000us
     nanosleeps 0

    C:\Users\Owner\Source\Repos\malbrain\rwlock>x64\release\rwlock 1000 3
     real 0.043us
     user 0.031us
     sys  0.078us
     nanosleeps 0

    C:\Users\Owner\Source\Repos\malbrain\rwlock>x64\release\rwlock 1000 2
     real 0.070us
     user 0.421us
     sys  0.093us
     nanosleeps 4399

    C:\Users\Owner\Source\Repos\malbrain\rwlock>x64\release\rwlock 1000 1
     real 0.042us
     user 0.031us
     sys  0.109us
     nanosleeps 0

    C:\Users\Owner\Source\Repos\malbrain\rwlock>x64\release\rwlock 1000 0
     real 0.039us
     user 0.031us
     sys  0.109us
     nanosleeps 0

Please address any questions or concerns to the program author: Karl Malbrain, malbrain@cal.berkeley.edu.
