rwlock
======

Phase Fair and Standard Reader Writer Locks

readerwriter.c: linux/Windows sched_yield/SwitchToThread phase-fair version. Also includes non-fair light weight rwlocks.

rwfutex.c:	Linux only phase-fair version that utilizes futex calls on reader/writer contention. Also includes a semi-fair light weight rwlock of 4 bytes.

Phase-fair locks are 8 bytes each. Light weight locks are 2 or 4 bytes each. Initialze new locks to all bytes zero.

Please address any questions or concerns to the program author: Karl Malbrain, malbrain@cal.berkeley.edu.
