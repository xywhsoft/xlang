#ifndef XL6_XAPI_LOAD_H
#define XL6_XAPI_LOAD_H

#include "src/abi/xl6_xapi_dump.h"
#include "src/diag/xl6_diag.h"

int xl6XapiLoadJsonFile(XL6_XapiModule* module, const char* path, XL6_DiagList* diags);

#endif
