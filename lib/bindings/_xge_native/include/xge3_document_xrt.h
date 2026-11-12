#ifndef XL6_XGE3_DOCUMENT_XRT_H
#define XL6_XGE3_DOCUMENT_XRT_H
#include "xge3_runtime.h"
/* An older Document SDK uses CRT output storage. Prototype compatibility is
 * insufficient for adoption: reject it before any native export can run. */
#if !defined(XUI_DOCUMENT_EXPORT_XRT_ALLOCATOR) || XUI_DOCUMENT_EXPORT_XRT_ALLOCATOR != 1u
#error "Document byte exports require a matched XRT-allocator XGE SDK"
#endif
#endif
