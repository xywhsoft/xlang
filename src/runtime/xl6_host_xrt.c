/* The full language host implements the resolver in xl_api_symbols.c using
 * its existing catalog. A standalone artifact explicitly includes this file
 * AFTER its selected XRT/xruntime implementation, in that same translation
 * unit. Never compile an unconditional all-modules lookup object. */
#if defined(XL6_ARTIFACT_XRT_IMPLEMENTATION)
#if !defined(XRT_CORE_H)
#error "Emit the selected XRT implementation before its native address catalog"
#endif
#include "xl6_host_xrt.h"
#include <stddef.h>
#include <string.h>
typedef struct XL6_HostXrtSymbolV1 {
    const char* name;
    XL6_HostXrtFunctionV1 address;
} XL6_HostXrtSymbolV1;
static const XL6_HostXrtSymbolV1 xl6HostXrtSymbolsV1[]={
#define XL6_HOST_XRT_SYMBOL(Name) {#Name,(XL6_HostXrtFunctionV1)(Name)},
#include "xl6_host_xrt_symbols.h"
#undef XL6_HOST_XRT_SYMBOL
};
XL6_HostXrtFunctionV1 xl6HostXrtResolveV1(const char* name)
{
    if(!name)return NULL;
    size_t first=0,last=sizeof(xl6HostXrtSymbolsV1)/sizeof(*xl6HostXrtSymbolsV1);
    while(first<last){
        size_t middle=first+(last-first)/2;
        int order=strcmp(name,xl6HostXrtSymbolsV1[middle].name);
        if(order<0)last=middle;else if(order>0)first=middle+1;
        else return xl6HostXrtSymbolsV1[middle].address;
    }
    return NULL;
}
#endif
