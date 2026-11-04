#ifndef FOREIGN_XGE3_PACKAGE_OWNERSHIP_BATCH_H
#define FOREIGN_XGE3_PACKAGE_OWNERSHIP_BATCH_H
#include "src/common/xl6_xrt.h"
typedef struct NativeBatchNode {
    unsigned refs,drops;
    bool cleared;
    const void* claim;
} NativeBatchNode;
#if defined(_WIN32)
#define NATIVE_BATCH_EXPORT __declspec(dllexport)
#else
#define NATIVE_BATCH_EXPORT __attribute__((visibility("default")))
#endif
NATIVE_BATCH_EXPORT const xrtownershipops* native_batch_ops(void);
NATIVE_BATCH_EXPORT const xrtownershipadapterv1* native_batch_adapter(void);
#endif
