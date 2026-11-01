#ifndef XL6_TEST_NATIVE_USERDATA_MODULE_H
#define XL6_TEST_NATIVE_USERDATA_MODULE_H
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_native_callback.h"
typedef struct UserdataModuleHost {
    const XL6_NativeCallbackRuntimeV1* api;
    XL6_NativeCallback* slot;
    unsigned calls;
    bool bodyReturned,adapterReturned;
} UserdataModuleHost;
typedef int (*UserdataModuleBody)(void*);
typedef int (*UserdataModuleAdapter)(UserdataModuleHost*,XL6_NativeCallback*);
#endif
