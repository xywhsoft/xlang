#include "foreign_span_variants.h"
#include <__xl_runtime/xl6_foreign_view.h>
#include <xrt_decl.h>
#include <assert.h>
static int live,calls;
int span_variants_sum(const float* values,int count)
{int sum=0;assert(count>=0 && (!count || values));++calls;for(int i=0;i<count;++i)sum+=(int)values[i];return sum;}
static int sum(SpanVariantsRoot* root,const float* values,int count)
{assert(root);return span_variants_sum(values,count);}
static int packed(SpanVariantsRoot* root,const SpanVariantsDesc* descriptor,int* output)
{assert(root && descriptor && output);*output=span_variants_sum(descriptor->values,descriptor->count);return 0;}
int span_variants_create(SpanVariantsRoot** output)
{*output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;(*output)->sum=sum;(*output)->packed=packed;++live;return 0;}
void span_variants_drop(SpanVariantsRoot* root){if(root){--live;xrtFree(root);}}
int span_variants_live(void){return live;}
int span_variants_calls(void){return calls;}
void span_variants_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void span_variants_reset(void){assert(xrtMemDebugFailAfter(-1));}
static bool destroy(void* raw){span_variants_drop(raw);return true;}
const XL6_ForeignViewDescriptorV1 spanVariantsPolicy={
 .size=sizeof(spanVariantsPolicy),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,.family="span.variants",
 .watchAlignment=1,.Destroy=destroy,.callAlignment=1
};
