rwlock
======

Phase Fair and Standard Reader Writer Locks

readerwriter.c: linux/Windows sched_yield/SwitchToThread version that includes light weight rwlocks.

rwfutex.c:	Linux only version that utilizes futex calls on reader/writer contention.

Phase-fair locks are 8 bytes each. Light weight locks are 2 bytes each. Initialze new locks to all bytes zero.

Please address any questions or concerns to the program author: Karl Malbrain, malbrain@cal.berkeley.edu.
