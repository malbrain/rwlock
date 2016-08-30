// reader-writer FIFO lock -- type 1

typedef volatile union {
  struct {
	uint16_t readers[1];
	uint16_t writers[1];
	uint16_t tix[1];
  };
  uint32_t urw;
} RWLock1;

void writeLock1 (RWLock1 *lock);
void writeUnlock1 (RWLock1 *lock);
void readLock1 (RWLock1 *lock);
void readUnlock1 (RWLock1 *lock);

// reader-writer FIFO lock -- type 2

typedef volatile union {
  struct {
	uint16_t xcl[1];
	uint16_t waiters[1];
  };
  uint32_t value[1];
} MutexLatch;

typedef struct {
  MutexLatch xcl[1];
  MutexLatch wrt[1];
  uint16_t readers[1];
} RWLock2;

void writeLock2 (RWLock2 *lock);
void writeUnlock2 (RWLock2 *lock);
void readLock2 (RWLock2 *lock);
void readUnlock2 (RWLock2 *lock);

// reader-writer FIFO lock -- type 3

typedef volatile struct {
	uint16_t rin[1];
	uint16_t rout[1];
	uint16_t ticket[1];
	uint16_t serving[1];
} RWLock3;

#define PHID 0x1
#define PRES 0x2
#define MASK 0x3
#define RINC 0x4
