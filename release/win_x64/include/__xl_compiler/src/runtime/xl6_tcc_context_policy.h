#ifndef XL6_TCC_CONTEXT_POLICY_H
#define XL6_TCC_CONTEXT_POLICY_H
#include "xl6_native_handle.h"

typedef int (*XL6_TccMainProc)(int, char**);
typedef struct XL6_TccHandler {
    void* state;
    XL6_TccMainProc main;
} XL6_TccHandler;

#endif
