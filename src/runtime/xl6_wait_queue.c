#include "xl6_wait_queue.h"
#define XL6_Q_PREFIX xl6WaitQueue
#define XL6_Q_BACKEND xrtTypedMPMCQueue
#define XL6_Q_TYPE xtypedmpmcqueue
#define XL6_Q_SINGLE_PRODUCER 0
#define XL6_Q_SINGLE_CONSUMER 0
#define XL6_Q_WAIT 1
#include "src/runtime/xl6_concurrent_queue_impl.h"
