#ifndef XL6_CLOSURE_CAPTURE_V3_H
#define XL6_CLOSURE_CAPTURE_V3_H
#include "xl6_closure.h"
#include "xl6_capture_v2.h"
typedef struct XL6_ClosureRuntimeV3 {
    XL6_Closure* (*Create)(const XL6_CodeOwner*,void (*)(void),void (*)(void),void*,size_t,
        const XL6_CaptureLayoutV2*,const char* const*,const xlir_call_default*,size_t);
    bool (*DropEnvironment)(const XL6_CaptureLayoutV2*,void*,size_t);
} XL6_ClosureRuntimeV3;
XL6_CODE_LEASE_API const XL6_ClosureRuntimeV3* xl6ClosureRuntimeV3Get(void);
#endif
