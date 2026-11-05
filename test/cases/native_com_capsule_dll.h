#ifndef NATIVE_COM_CAPSULE_DLL_H
#define NATIVE_COM_CAPSULE_DLL_H
#include <stdbool.h>
#include "src/runtime/xnative_transfer.h"
typedef struct CapsuleDllState {
    unsigned calls,tails,releases;
    bool ready,releaseInside;
    int releaseStatus;
    void* resident;
} CapsuleDllState;
#if defined(_WIN32)
typedef int (*CapsuleDllCreate)(CapsuleDllState*,void**,xn_transfer_native_owner_v1*,uint64_t);
#endif
#endif
