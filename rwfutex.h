
typedef struct {
  volatile union {
	struct {
	  uint16_t rin[1];
	  uint16_t rout[1];
	  uint16_t serving[1];
	  uint16_t ticket[1];
	};
	uint32_t rw[2];
  };
} RWLock;

//	define rin bits
#define PHID 0x1
#define PRES 0x2
#define MASK 0x3
#define RINC 0x4

//	mode & definition for latch implementation

enum {
	QueRd = 1,	// reader queue
	QueWr = 2	// writer queue
} RWQueue;

//	lite weight spin latch

typedef struct {
  union {
	struct {
	  uint16_t xlock:1;	// writer has exclusive lock
	  uint16_t share:15;	// count of readers with lock
	  uint16_t read:1;	// readers are waiting
	  uint16_t wrt:15;	// count of writers waiting
	} bits[1];
	uint32_t value[1];
  };
} FutexLatch;

#define XCL 1
#define SHARE 2
#define READ (SHARE * 32768)
#define WRT (READ * 2)

