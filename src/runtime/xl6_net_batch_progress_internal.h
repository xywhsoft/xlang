#ifndef XL6_NET_BATCH_PROGRESS_INTERNAL_H
#define XL6_NET_BATCH_PROGRESS_INTERNAL_H
#include "xl6_text_value.h"
typedef struct XL6_NetBatchProgressV1 {
    uint64_t completed,start;
    int64_t status;
    bool attempted;
} XL6_NetBatchProgressV1;
#endif
