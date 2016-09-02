rwlock
======

Reader Writer Locks

Initialze locks to all bytes zero. Compile with -D STANDALONE to add benchmarking main module.  Run the benchmark with two parameters:  # of threads and lock-type.  The benchmark consists of 1000000 lock/work/unlock calls divided by the number of threads specified.  500x Ratio of Read Locks to Write Locks.

readerwriter.c: linux/Windows spinlock/phase-fair:

    0: type 0   pthread/SRW system rwlocks
    1: type 1	Reader preference simple rwlock
    2: type 2	Mutex based, neither FIFO nor Fair
    3: type 3	FIFO and Phase-Fair Brandenburg spin lock

    0: sizeof RWLock0: 56
    1: sizeof RWLock1: 2
    2: sizeof RWLock2: 4
    3: sizeof RWLock3: 8

Sample linux 3.10.0-123.9.3.el7.x86_64 output: (times are in usecs per call of lock/unlock pair)

    [root@test7x64 xlink]# cc -o readerwriter -g -O3 -D STANDALONE readerwriter.c -lpthread

    [root@test7x64 xlink]# ./readerwriter 2 1
     real 2.112us
     user 2.128us
     sys  0.001us
     futex waits: 0
     nanosleeps 1575

    [root@test7x64 xlink]# ./readerwriter 20 1
     real 2.070us
     user 2.181us
     sys  0.009us
     futex waits: 0
     nanosleeps 24915

    [root@test7x64 xlink]# ./readerwriter 200 1
     real 1.926us
     user 2.470us
     sys  0.197us
     futex waits: 0
     nanosleeps 189805

    [root@test7x64 xlink]# ./readerwriter 2000 1
     real 1.756us
     user 3.052us
     sys  1.991us
     futex waits: 0
     nanosleeps 1018699

    [root@test7x64 xlink]# ./readerwriter 2 2
     real 1.109us
     user 2.072us
     sys  0.046us
     futex waits: 7947

    [root@test7x64 xlink]# ./readerwriter 20 2
     real 0.723us
     user 2.053us
     sys  0.433us
     futex waits: 41750

    [root@test7x64 xlink]# ./readerwriter 200 2
     real 1.058us
     user 2.102us
     sys  1.936us
     futex waits: 265360

    [root@test7x64 xlink]# ./readerwriter 2000 2
     real 1.378us
     user 2.196us
     sys  3.075us
     futex waits: 374353

    [root@test7x64 xlink]# ./readerwriter 2 3
     real 1.283us
     user 2.569us
     sys  0.000us
     futex waits: 0
     nanosleeps 3

    [root@test7x64 xlink]# ./readerwriter 20 3
     real 1.685us
     user 3.178us
     sys  0.014us
     futex waits: 0
     nanosleeps 67037

    [root@test7x64 xlink]# ./readerwriter 200 3
     real 1.760us
     user 3.223us
     sys  0.229us
     futex waits: 0
     nanosleeps 251252

    [root@test7x64 xlink]# ./readerwriter 2000 3
     real 2.030us
     user 3.626us
     sys  2.559us
     futex waits: 0
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
    3: type 2	FIFO and Phase-Fair Brandenburg futex lock

    0: sizeof SystemLatch: 56
    1: sizeof FutexLock: 4
    2: sizeof RWLock: 8

Sample output: (times are in usecs per call of lock/unlock pair)

	-- SCALABLE LOCK (NOT PHASE-FAIR) --

    [root@test7x64 xlink]# cc -o rwfutex -g -O0 -D STANDALONE /home/devel/xlink17/cloud/source/rwfutex.c -lpthread

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

	-- NO SO SCALABLE LOCK (PHASE-FAIR) --

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

Please address any questions or concerns to the program author: Karl Malbrain, malbrain@cal.berkeley.edu.
