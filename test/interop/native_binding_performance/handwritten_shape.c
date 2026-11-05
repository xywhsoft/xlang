#include "binding_perf.h"
#include <__xl_runtime/xl6_native_sequence.h>
#include <__xl_runtime/xl_foreign_error.h>
#include "shape_schemas.h"
#include <assert.h>
#include <stdio.h>
#ifdef BINDING_PERF_SAFE
static xvalue* ownClusters(const xui_text_shape_t* shape)
{
 XL6_NativeSequenceResultV1 result={0};const xui_text_cluster_t* data=NULL;size_t count=0;
 assert(xl6NativeSequenceResultReserveV1(&result,&perf_cluster_schema,&xl6Xui3TextShapePolicy,&xl6Xui3OwnershipDomainV1,NULL));
 assert(xuiTextShapeGetClusters(shape,&data,&count)==XUI_OK);
 assert(xl6NativeSequenceResultSpanV1(&result,data,count) && xuiTextShapeAddRef(shape)==XUI_OK);
 assert(xl6NativeSequenceResultAdoptV1(&result,shape));xvalue* value=xl6NativeSequenceResultTakeV1(&result);
 assert(value && xl6NativeSequenceResultEndV1(&result));return value;
}
static xvalue* ownCarets(const xui_text_shape_t* shape)
{
 XL6_NativeSequenceResultV1 result={0};const xui_text_caret_t* data=NULL;size_t count=0;
 assert(xl6NativeSequenceResultReserveV1(&result,&perf_caret_schema,&xl6Xui3TextShapePolicy,&xl6Xui3OwnershipDomainV1,NULL));
 assert(xuiTextShapeGetCarets(shape,&data,&count)==XUI_OK);
 assert(xl6NativeSequenceResultSpanV1(&result,data,count) && xuiTextShapeAddRef(shape)==XUI_OK);
 assert(xl6NativeSequenceResultAdoptV1(&result,shape));xvalue* value=xl6NativeSequenceResultTakeV1(&result);
 assert(value && xl6NativeSequenceResultEndV1(&result));return value;
}
static int64_t clustersLoop(xvalue* value,int64_t sum,int64_t iteration)
{
 xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
 xvalue* pinned=xrtValueRetain(value);assert(pinned);XL6_NativeSequenceReadV1 read={0};
 assert(xl6NativeSequenceReadBeginV1(pinned,&perf_cluster_schema,&read));
 const xui_text_cluster_t* data=read.data;
 for(size_t i=0;i<read.count;++i)sum=(sum^(int64_t)data[i].iTextStart)+iteration;
 assert(xl6NativeSequenceReadEndV1(&read));xrtValueRelease(pinned);
 xl6_error_result result=xl6ErrorScopeEndAtSafePointV1(&errors);assert(result.ok && !result.error);return sum;
}
static int64_t caretsLoop(xvalue* value,int64_t sum,int64_t iteration)
{
 xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
 xvalue* pinned=xrtValueRetain(value);assert(pinned);XL6_NativeSequenceReadV1 read={0};
 assert(xl6NativeSequenceReadBeginV1(pinned,&perf_caret_schema,&read));
 const xui_text_caret_t* data=read.data;
 for(size_t i=0;i<read.count;++i)sum=(sum^(int64_t)data[i].iTextOffset)+iteration;
 assert(xl6NativeSequenceReadEndV1(&read));xrtValueRelease(pinned);
 xl6_error_result result=xl6ErrorScopeEndAtSafePointV1(&errors);assert(result.ok && !result.error);return sum;
}
#endif
int main(void)
{
 binding_perf_counter_begin();assert(xl6Xge3RuntimeBindV1()==0);binding_perf_counter_end(-1);
 binding_perf_counter_begin();
 xui_proxy proxy=NULL;xui_font font=NULL;const xui_text_shape_t* shape=NULL;
 assert(binding_perf_proxy(&proxy)==XUI_OK && binding_perf_font(proxy,&font)==XUI_OK && binding_perf_shape(proxy,font,&shape)==XUI_OK);
#ifdef BINDING_PERF_SAFE
 xvalue* clusters=ownClusters(shape);xvalue* carets=ownCarets(shape);
#else
 const xui_text_cluster_t* clusters=NULL;const xui_text_caret_t* carets=NULL;size_t nc=0,nt=0;
 assert(xuiTextShapeGetClusters(shape,&clusters,&nc)==XUI_OK && nc==4096 && xuiTextShapeAddRef(shape)==XUI_OK);
 assert(xuiTextShapeGetCarets(shape,&carets,&nt)==XUI_OK && nt==4096 && xuiTextShapeAddRef(shape)==XUI_OK);
#endif
 assert(xuiTextShapeReleaseChecked(shape)==XUI_OK && xuiFontReleaseChecked(font)==XUI_OK && xuiProxyReleaseChecked(proxy)==XUI_OK);
 binding_perf_counter_end(0);binding_perf_counter_begin();
 unsigned long long start=binding_perf_clock();int64_t sum=0;
 for(int64_t iteration=0;iteration<5000;++iteration){
#ifdef BINDING_PERF_SAFE
  sum=clustersLoop(clusters,sum,iteration);sum=caretsLoop(carets,sum,iteration);
#else
  for(size_t i=0;i<nc;++i)sum=(sum^(int64_t)clusters[i].iTextStart)+iteration;
  for(size_t i=0;i<nt;++i)sum=(sum^(int64_t)carets[i].iTextOffset)+iteration;
#endif
 }
 unsigned long long elapsed=binding_perf_clock()-start;binding_perf_counter_end(1);
 printf("shape %llu %llu %lld\n",elapsed,binding_perf_frequency(),(long long)sum);
 binding_perf_counter_begin();
#ifdef BINDING_PERF_SAFE
 xrtValueRelease(clusters);xrtValueRelease(carets);assert(xl6ForeignViewDrainV1());
#else
 assert(xuiTextShapeReleaseChecked(shape)==XUI_OK && xuiTextShapeReleaseChecked(shape)==XUI_OK);
#endif
 assert(binding_perf_verify());binding_perf_counter_end(5);return 0;
}
