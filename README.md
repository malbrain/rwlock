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

Sample linux 3.10.0-123.9.3.el7.x86_64 output: (times are in usecs per call of lock/unlock pair)

    [root@test7x64 xlink]# cc -o readerwriter -g -O3 -D STANDALONE readerwriter.c -lpthread

    [root@test7x64 xlink]# ./readerwriter 2 1
     real 0.990us
     user 1.979us
     sys  0.000us
     nanosleeps 8

    [root@test7x64 xlink]# ./readerwriter 20 1
     real 1.049us
     user 3.476us
     sys  0.041us
     nanosleeps 96672

    [root@test7x64 xlink]# ./readerwriter 200 1
     real 4.757us
     user 13.614us
     sys  4.124us
     nanosleeps 4553530

    [root@test7x64 xlink]# ./readerwriter 2000 1
     real 5.465us
     user 5.841us
     sys  15.989us
     nanosleeps 7129710

Sample Simple Mutex based lock 64 bit linux:

    [root@test7x64 xlink]# ./readerwriter 2 2
     real 1.359us
     user 2.720us
     sys  0.001us
     nanosleeps 4

    [root@test7x64 xlink]# ./readerwriter 20 2
     real 0.746us
     user 2.933us
     sys  0.004us
     nanosleeps 2352

    [root@test7x64 xlink]# ./readerwriter 200 2
     real 0.694us
     user 2.731us
     sys  0.015us
     nanosleeps 15612

    [root@test7x64 xlink]# ./readerwriter 2000 2
     real 0.744us
     user 2.778us
     sys  0.090us
     nanosleeps 30071

Sample Brandenburg Phase-Fair FIFO 64 bit linux:

    [root@test7x64 xlink]# ./readerwriter 2 3
     real 1.283us
     user 2.569us
     sys  0.000us
     nanosleeps 3

    [root@test7x64 xlink]# ./readerwriter 20 3
     real 1.685us
     user 3.178us
     sys  0.014us
     nanosleeps 67037

    [root@test7x64 xlink]# ./readerwriter 200 3
     real 1.760us
     user 3.223us
     sys  0.229us
     nanosleeps 251252

    [root@test7x64 xlink]# ./readerwriter 2000 3
     real 2.030us
     user 3.626us
     sys  2.559us
     nanosleeps 1392172

sample Windows 7 64bit output:

    readerwriter 2 1
     real 2.329us
     user 3.416us
     sys  0.873us
     nanosleeps 1961

    readerwriter 20 1
     real 2.623us
     user 3.697us
     sys  3.198us
     nanosleeps 18973

    readerwriter 200 1
     real 3.095us
     user 4.274us
     sys  3.822us
     nanosleeps 204165

    readerwriter 2000 1
     real 3.121us
     user 4.290us
     sys  2.698us
     nanosleeps 789758

rwfutex.c:	Four Linux only versions that utilize futex calls on contention.

    0: type 0   pthread/SRW system rwlocks
    1: type 1   Not FIFO nor Phase-Fair
    2: type 2	FIFO and Phase-Fair Brandenburg futex lock
    3: type 3	Mutex based rwlock

    0: sizeof SystemLatch: 56
    1: sizeof FutexLock: 4
    2: sizeof RWLock: 8
    3: sizeof RWLock2: 12

Sample output: (times are in usecs per call of lock/unlock pair)

    [root@test7x64 xlink]# cc -o rwfutex -g -O3 -D STANDALONE rwfutex.c -lpthread

    [root@test7x64 xlink]# ./rwfutex 2 1
     real 1.311us
     user 2.546us
     sys  0.012us
     futex waits: 5202

    [root@test7x64 xlink]# ./rwfutex 20 1
     real 0.726us
     user 2.574us
     sys  0.107us
     futex waits: 38678

    [root@test7x64 xlink]# ./rwfutex 200 1
     real 1.026us
     user 2.576us
     sys  1.500us
     futex waits: 156245

    [root@test7x64 xlink]# ./rwfutex 2000 1
     real 4.690us
     user 2.618us
     sys  16.095us
     futex waits: 927765

    [root@test7x64 xlink]# ./rwfutex 2 2
     real 1.013us
     user 1.942us
     sys  0.018us
     futex waits: 3860

    [root@test7x64 xlink]# ./rwfutex 20 2
     real 2.037us
     user 2.115us
     sys  5.814us
     futex waits: 698256

    [root@test7x64 xlink]# ./rwfutex 200 2
     real 5.122us
     user 2.188us
     sys  18.063us
     futex waits: 2306898

    [root@test7x64 xlink]# ./rwfutex 2000 2
     real 14.875us
     user 3.104us
     sys  55.712us
     futex waits: 7563596

    [root@test7x64 xlink]# ./rwfutex 2 3
     real 1.150us
     user 2.147us
     sys  0.050us
     futex waits: 7802

    [root@test7x64 xlink]# ./rwfutex 20 3
     real 1.345us
     user 2.234us
     sys  0.037us
     futex waits: 10303

    [root@test7x64 xlink]# ./rwfutex 200 3
     real 1.317us
     user 2.253us
     sys  0.052us
     futex waits: 11247

    [root@test7x64 xlink]# ./rwfutex 2000 3
     real 1.071us
     user 2.234us
     sys  0.099us
     futex waits: 12821

Please address any questions or concerns to the program author: Karl Malbrain, malbrain@cal.berkeley.edu.
