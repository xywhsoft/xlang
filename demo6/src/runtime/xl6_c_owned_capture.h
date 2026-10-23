#ifndef XL6_C_OWNED_CAPTURE_H
#define XL6_C_OWNED_CAPTURE_H
#include "xl6_c_owned.h"
#include "xl6_capture_v2.h"
XL6_CODE_LEASE_API bool xl6COwnedCopyCapturedV1(XL6_COwnedV1,XL6_COwnedV1*);
XL6_CODE_LEASE_API void xl6COwnedDropCapturedV1(XL6_COwnedV1);
XL6_CODE_LEASE_API const XL6_NativeCaptureProfileV1* xl6COwnedCaptureProfileV1Get(void);
#endif
