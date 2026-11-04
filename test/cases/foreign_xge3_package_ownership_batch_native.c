#include "foreign_xge3_package_ownership_batch.h"
#include <assert.h>
static bool count(const void* raw,size_t* output)
{const NativeBatchNode* node=raw;if(!node || !output || node->cleared || !node->refs)return false;*output=node->refs;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user)
{(void)raw;(void)visit;(void)user;return true;}
static bool hold(const void* raw)
{NativeBatchNode* node=(NativeBatchNode*)raw;assert(node->refs);++node->refs;return true;}
static void drop(const void* raw)
{NativeBatchNode* node=(NativeBatchNode*)raw;assert(node->refs);--node->refs;++node->drops;}
static bool claim(const void* raw,const void* token)
{NativeBatchNode* node=(NativeBatchNode*)raw;if(node->claim && node->claim!=token)return false;node->claim=token;return true;}
static void restore(const void* raw,const void* token)
{NativeBatchNode* node=(NativeBatchNode*)raw;assert(node->claim==token);node->claim=NULL;}
static void clear(const void* raw,const void* token)
{NativeBatchNode* node=(NativeBatchNode*)raw;assert(node->claim==token && !node->cleared);node->cleared=true;}
NATIVE_BATCH_EXPORT const xrtownershipops* native_batch_ops(void)
{static const xrtownershipops value={count,trace};return &value;}
NATIVE_BATCH_EXPORT const xrtownershipadapterv1* native_batch_adapter(void)
{static const xrtownershipadapterv1 value={sizeof(value),hold,drop,claim,restore,NULL,clear,NULL};return &value;}
