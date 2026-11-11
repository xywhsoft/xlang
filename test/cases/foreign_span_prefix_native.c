#include "foreign_span_prefix.h"
#include <__xl_runtime/xl6_foreign_view.h>
#include <xrt_decl.h>
#include <assert.h>
static int calls;
static xmemdebugsnapshot before;
int prefix_sum(const void* data,size_t count)
{
    assert(!count || data);assert(count<=1024);++calls;
    int sum=0;const uint8_t* bytes=data;
    for(size_t i=0;i<count;++i)sum+=bytes[i];
    return sum;
}
int prefix_sum_int(const uint8_t* data,int count)
{assert(count>=0);return prefix_sum(data,(size_t)count);}
int prefix_mix(const char* data,int count,int* sum,size_t* length)
{assert(sum && length);*sum=prefix_sum_int((const uint8_t*)data,count);*length=(size_t)count;return 0;}
int prefix_two(const void* a,size_t ac,const void* b,size_t bc)
{return prefix_sum(a,ac)+prefix_sum(b,bc);}
static int measure(PrefixRoot* root,const void* data,size_t count)
{assert(root);return prefix_sum(data,count);}
int prefix_create(PrefixRoot** output)
{PrefixRoot* root=xrtCalloc(1,sizeof(*root));if(!root)return -1;root->measure=measure;*output=root;return 0;}
void prefix_destroy(PrefixRoot* root){xrtFree(root);}
static bool destroy(void* raw){prefix_destroy(raw);return true;}
const XL6_ForeignViewDescriptorV1 prefixRootPolicy={
    .size=sizeof(prefixRootPolicy),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,.family="span.prefix",
    .watchAlignment=1,.Destroy=destroy,.callAlignment=1
};
int prefix_calls(void){return calls;}
void prefix_counter_begin(void){xrtMemDebugSnapshot(&before);}
void prefix_counter_end(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
}
