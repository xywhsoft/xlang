#include "src/runtime/xl6_array_import.h"
#include "src/runtime/xl6_array_import_chain.h"
#if defined(XRT_SINGLE_HEADER_H) || defined(XRT_API) || defined(XRUNTIME_SINGLE_HEADER_H)
#error Import declaration pulled a full runtime header
#endif
static xvalue* (*const importBoundary)(xvalue*,const xvalue*)=xl6ArrayImportTakeV1;
static xvalue* (*const chainBoundary)(xvalue**,size_t,const xvalue*)=xl6ArrayImportChainTakeV1;
int main(void) { return importBoundary==0 || chainBoundary==0; }
