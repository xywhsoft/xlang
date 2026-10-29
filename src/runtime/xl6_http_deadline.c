#include "xl6_http_internal.h"
#include "xl6_concurrent_deadline.h"
uint64_t xl6Http1DeadlineV2(int64_t millis)
{XL6_DeadlineV1 deadline=0;return xl6ConcurrentDeadline(millis,&deadline)?deadline:0;}
