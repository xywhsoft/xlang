#ifndef FOREIGN_PACK_OWNED_CALLBACK_NESTED_H
#define FOREIGN_PACK_OWNED_CALLBACK_NESTED_H
#include <__xl_runtime/xrt_decl.h>
typedef struct GpnOwner {xrtownershipref reference;bool (*hold)(const void*);bool (*drop)(const void*);} GpnOwner;
typedef struct GpnDesc {int seed;int (*callback)(int,void*);void* user;} GpnDesc;
static inline int gpn_call(const GpnDesc* desc,const GpnOwner* owner)
{
    if(!desc || !owner || desc->user!=owner->reference.Data || !owner->hold(owner->reference.Data))return -1;
    int result=desc->callback ? desc->callback(desc->seed,desc->user):desc->seed;
    if(!owner->drop(owner->reference.Data))return -1;
    return result;
}
#endif
