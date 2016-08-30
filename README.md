rwlock
======

Phase Fair Reader Writer Locks

readerwriter.c: linux/Windows spinlock phase-fair. Four versions, 8 or 12 bytes each.

rwfutex.c:	Four Linux only phase-fair versions that utilize futex calls on contention. Locks are 4 or 8 bytes each.

Initialze locks to all bytes zero. Compile with -D STANDALONE to add benchmarking main module.  Run the benchmark with two parameters:  # of threads and lock-type:
    0: pthread
    1: type 1
    2: type 2
    3: type 3

Please address any questions or concerns to the program author: Karl Malbrain, malbrain@cal.berkeley.edu.
