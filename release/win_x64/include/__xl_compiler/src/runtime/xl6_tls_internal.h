#ifndef XL6_TLS_INTERNAL_H
#define XL6_TLS_INTERNAL_H
#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

bool xl6TlsFailV1(xerrkind,const char*);
bool xl6TlsOutcomeV1(bool);
bool xl6TlsLimitV1(size_t,int64_t,const char*);
#endif
