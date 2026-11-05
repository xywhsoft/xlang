#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_native_sequence.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
typedef struct Item{int32_t first;int64_t second;} Item;
typedef struct Native{volatile int32_t refs;Item* rows;size_t count;bool ready;unsigned drops;const void* claim;xvalue* capture;} Native;
static unsigned made,freed,retains,drops,loadedScopes;
static XL6_NativeSequenceSchemaV1 schema={sizeof(schema),XL6_NATIVE_SEQUENCE_SCHEMA_V1_TAG,1001,2001,sizeof(Item),_Alignof(Item),sizeof(Item)};
static Native* make(void)
{Native* n=xrtCalloc(1,sizeof(*n));assert(n);n->refs=1;n->ready=true;n->count=3;n->rows=xrtCalloc(n->count,sizeof(*n->rows));assert(n->rows);for(size_t i=0;i<n->count;++i)n->rows[i]=(Item){(int32_t)i,(int64_t)i*17};++made;return n;}
static int retain(Native* n){if(n->claim)return -1;assert(xrtRefRetain(&n->refs)>0);++retains;return 0;}
static bool nativeDestroy(void* raw)
{
 Native* n=raw;if(n->refs==1&&!n->ready)return false;
 int32_t refs=xrtRefRelease(&n->refs);assert(refs>=0);++drops;
 if(!refs){assert(!n->capture);xrtFree(n->rows);xrtFree(n);++freed;}return true;
}
/* This record's Count is its actual Native RC. The capture slot, when
 * populated by the explicit cycle case, is traced and cleared exactly once. */
static bool nativeCount(const void* raw,size_t* count){const Native* n=raw;*count=(size_t)n->refs;return n->refs>0;}
static bool nativeTrace(const void*,xrtownershipvisitor,void*);
static const xrtownershipops nativeOps={nativeCount,nativeTrace};
static xrtownershipref nativeRef(const Native* n){return (xrtownershipref){n,&nativeOps};}
static bool nativeTrace(const void* raw,xrtownershipvisitor visit,void* user)
{const Native* n=raw;return !n->capture||visit(xrtValueOwnership(n->capture),user);}
static bool nativeHold(const void* raw){return xrtRefRetain(&((Native*)raw)->refs)>0;}
static void nativeDrop(const void* raw){assert(nativeDestroy((void*)raw));}
static bool nativeClaim(const void* raw,const void* token){Native* n=(void*)raw;if(n->claim&&n->claim!=token)return false;n->claim=token;return true;}
static void nativeRestore(const void* raw,const void* token){Native* n=(void*)raw;assert(n->claim==token);n->claim=NULL;}
static void nativeClear(const void* raw,const void* token)
{Native* n=(void*)raw;assert(n->claim==token);xvalue* value=n->capture;n->capture=NULL;if(value)xrtValueRelease(value);}
static const xrtownershipadapterv1 nativeAdapter={sizeof(nativeAdapter),nativeHold,nativeDrop,nativeClaim,nativeRestore,NULL,nativeClear,NULL};
static bool nativeResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{(void)policies;(void)preparation;(void)completion;if(ref.Ops!=&nativeOps)return false;*adapter=&nativeAdapter;return true;}
static bool nativeRead(xrtownershipref ref){return ref.Ops==&nativeOps;}
static const XL6_OwnershipDomainV1 nativeDomain={sizeof(nativeDomain),NULL,nativeResolve,nativeRead};
static bool ownerTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user){return visit(nativeRef(raw),user);}
static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,"test.native-array-owner",
 0,1,NULL,NULL,nativeDestroy,0,1,NULL,NULL,ownerTrace,NULL,XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE,NULL,NULL};
static xvalue* produce(Native* native,const XL6_CodeOwner* code,bool postRetainOom)
{
 XL6_NativeSequenceResultV1 result={0};assert(xl6NativeSequenceResultReserveV1(&result,&schema,&policy,&nativeDomain,code));
 assert(xl6NativeSequenceResultSpanV1(&result,native->rows,native->count));
 if(postRetainOom)assert(xrtMemDebugFailAfter(0));
 assert(retain(native)==0);assert(xl6NativeSequenceResultAdoptV1(&result,native));
 xvalue* value=xl6NativeSequenceResultTakeV1(&result);assert(value);assert(xl6NativeSequenceResultEndV1(&result));
 if(postRetainOom){assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();}
 return value;
}
static void read(xvalue* value,size_t first,size_t count)
{
 XL6_NativeSequenceReadV1 span={0};assert(xl6NativeSequenceReadBeginV1(value,&schema,&span));++loadedScopes;
 assert(span.count==count&&span.stride==sizeof(Item));
 const Item* rows=span.data;for(size_t i=0;i<count;++i){Item element=rows[i];assert(element.first==(int32_t)(first+i)&&element.second==(int64_t)(first+i)*17);}
 assert(xl6NativeSequenceReadEndV1(&span));assert(xl6NativeSequenceReadEndV1(&span));
}
static void own(void)
{
 Native* n=make();unsigned before=retains;xvalue* seq=produce(n,NULL,true);assert(retains==before+1&&n->refs==2);
 assert(nativeDestroy(n)&&n->refs==1); /* Original Shape.close consumes ONLY its owner. */
 XL6_NativeSequenceReadV1 span={0};assert(xl6NativeSequenceReadBeginV1(seq,&schema,&span)&&span.data==n->rows);assert(xl6NativeSequenceReadEndV1(&span));
 xvalue* copy=xrtValueRetain(seq);assert(copy==seq);xvalue* slice=xl6NativeSequenceSliceV1(seq,1001,2001,1,-1);assert(slice);
 xvalue* tail=xl6NativeSequenceSliceV1(slice,1001,2001,1,1);assert(tail&&retains==before+1);
 assert(xl6NativeSequenceAdmitsV1(seq,1001,2001)&&!xl6NativeSequenceAdmitsV1(seq,1001,2002));
 int64_t length=-1;assert(xl6NativeSequenceLengthV1(seq,1001,2001,&length)&&length==3);
 read(seq,0,3);read(slice,1,2);read(tail,2,1);
 xrtValueRelease(seq);xrtValueRelease(copy);read(slice,1,2);xrtValueRelease(slice);read(tail,2,1);
 n->ready=false;unsigned d=drops;xrtValueRelease(tail);assert(xl6ForeignViewDrainV1()&&xl6ForeignViewPendingV1(0)==1&&drops==d&&n->refs==1);
 n->ready=true;assert(xl6ForeignViewDrainV1()&&!xl6ForeignViewPendingV1(0)&&drops==d+1);
}
static void malformed(void)
{
 Native* n=make();XL6_NativeSequenceResultV1 r={0};assert(xl6NativeSequenceResultReserveV1(&r,&schema,&policy,NULL,NULL));unsigned before=retains;
 assert(!xl6NativeSequenceResultSpanV1(&r,NULL,1));xrtClearError();assert(!xl6NativeSequenceResultSpanV1(&r,(void*)1,3));xrtClearError();
 assert(!xl6NativeSequenceResultSpanV1(&r,n->rows,UINT64_MAX));xrtClearError();
 assert(!xl6NativeSequenceResultSpanV1(&r,(void*)(UINTPTR_MAX&~(uintptr_t)15),2));xrtClearError();
 assert(xl6NativeSequenceResultSpanV1(&r,(void*)1,0)&&retains==before);
 assert(!xl6NativeSequenceResultTakeV1(&r));xrtClearError();assert(xl6NativeSequenceResultEndV1(&r)&&retains==before&&n->refs==1);assert(nativeDestroy(n));
 xvalue* empty=xl6NativeSequenceEmptyV1(&schema);assert(empty);read(empty,0,0);
 assert(!xl6NativeSequenceSliceV1(empty,1001,2001,-1,0));xrtClearError();
 assert(!xl6NativeSequenceSliceV1(empty,1001,2001,INT64_MAX,1));xrtClearError();
 assert(!xl6NativeSequenceSliceV1(empty,1001,2001,0,-2));xrtClearError();xrtValueRelease(empty);
}
static bool scopeBegin(void* raw,void* token){(void)raw;(void)token;return true;}
static bool scopeEnd(void* token){(void)token;return true;}
static bool forbiddenWatch(void* raw,void* token,void (*invalidate)(void*),void* user)
{(void)raw;(void)token;(void)invalidate;(void)user;assert(!"NativeSequence must reject a watched policy before calling Watch");return false;}
static void forbiddenUnwatch(void* token){(void)token;assert(!"NativeSequence must not install an Unwatch adapter");}
static void defensive(void)
{
 XL6_ForeignViewDescriptorV1 scoped=policy;scoped.callBytes=1;scoped.callAlignment=1;scoped.CallBegin=scopeBegin;scoped.CallEnd=scopeEnd;
 XL6_NativeSequenceResultV1 r={0};unsigned before=retains;
 assert(!xl6NativeSequenceResultReserveV1(&r,&schema,&scoped,NULL,NULL));xrtClearError();assert(retains==before);
 for(unsigned field=0;field<3;++field){XL6_ForeignViewDescriptorV1 watched=policy;
  if(field==0)watched.Watch=forbiddenWatch;
  if(field==1)watched.Unwatch=forbiddenUnwatch;
  if(field==2)watched.watchBytes=1;
  assert(xrtMemDebugFailAfter(0));assert(!xl6NativeSequenceResultReserveV1(&r,&schema,&watched,NULL,NULL));
  assert(!xrtMemDebugFailTriggered() && retains==before && !r.self && !r.owner.self);
  xrtMemDebugFailClear();xrtClearError();
 }
 r.owner.live=true;assert(!xl6NativeSequenceResultReserveV1(&r,&schema,&policy,NULL,NULL));xrtClearError();assert(r.owner.live);r.owner.live=false;
 assert(xl6NativeSequenceResultEndV1(&r));
 xvalue* empty=xl6NativeSequenceEmptyV1(&schema);assert(empty);XL6_NativeSequenceReadV1 span={0};span.call.self=&span.call;
 assert(!xl6NativeSequenceReadBeginV1(empty,&schema,&span));xrtClearError();assert(span.call.self==&span.call);span.call.self=NULL;
 assert(xl6NativeSequenceReadBeginV1(empty,&schema,&span));assert(!xl6NativeSequenceReadBeginV1(empty,&schema,&span));xrtClearError();assert(span.self==&span);
 assert(xl6NativeSequenceReadEndV1(&span));xrtValueRelease(empty);
}
static size_t prefixes(void)
{
 for(size_t point=0;point<100;++point){
  Native* n=make();XL6_NativeSequenceResultV1 r={0};unsigned before=retains;
  assert(xrtMemDebugFailAfter(point));bool ok=xl6NativeSequenceResultReserveV1(&r,&schema,&policy,NULL,NULL);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
  assert(retains==before&&n->refs==1);assert(xl6NativeSequenceResultEndV1(&r));assert(nativeDestroy(n));
  if(!hit){assert(ok);return point+1;}assert(!ok);
 }assert(!"reserve did not reach its first no-OOM prefix");return 0;
}
static int32 worker(void* raw)
{
 xvalue* value=raw;XL6_NativeSequenceReadV1 span={0};assert(!xl6NativeSequenceReadBeginV1(value,&schema,&span));xrtClearError();
 xrtValueRelease(value);return 0;
}
static void affinity(void)
{
 Native* n=make();xvalue* seq=produce(n,NULL,false);assert(nativeDestroy(n));unsigned before=drops;
 xthread* thread=xrtThreadCreate(worker,seq,0);assert(thread);assert(xrtThreadWaitFor(thread,10000)==XWAIT_OK);xrtThreadDestroy(thread);
 assert(n->refs==1&&drops==before&&xl6ForeignViewPendingV1(0)==1);assert(xl6ForeignViewDrainV1()&&drops==before+1);
}
static int acquire(void* data){unsigned* count=data;++*count;return 1;}
static void release(void* data){unsigned* count=data;assert(*count);--*count;}
static void codeLifetime(void)
{
 unsigned leases=0;XL6_CodeOwner code={sizeof(code),&leases,acquire,release};Native* n=make();xvalue* seq=produce(n,&code,false);
 assert(leases==1&&nativeDestroy(n));xvalue* slice=xl6NativeSequenceSliceV1(seq,1001,2001,0,1);assert(slice&&leases==1);
 xrtValueRelease(seq);assert(leases==1);read(slice,0,1);xrtValueRelease(slice);assert(leases==1);
 assert(xl6ForeignViewDrainV1()&&!leases);
}
static void cycle(void)
{
 Native* n=make();xvalue* seq=produce(n,NULL,false);n->capture=xrtValueRetain(seq);
 assert(nativeDestroy(n)&&n->refs==1);xrtValueRelease(seq);seq=n->capture;
 const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainNativeSequenceV1Get(),xl6OwnershipDomainForeignViewV1Get()};
 XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
 XL6_OwnershipPlan* plan=NULL;XL6_NativeSequenceReadV1 span={0};assert(xl6NativeSequenceReadBeginV1(seq,&schema,&span));
 assert(xl6OwnershipPlanTryCreateV8(xrtValueOwnership(seq),xl6OwnershipCatalogProbeV1,NULL,xl6OwnershipCatalogResolveFinalizationV1,false,catalog,xl6OwnershipCatalogBoundaryV1Get(catalog),&plan)==XL6_OWNERSHIP_PLAN_BUSY&&!plan);
 assert(!xrtGetError()&&xl6NativeSequenceReadEndV1(&span));
 assert(xl6OwnershipPlanTryCreateV8(xrtValueOwnership(seq),xl6OwnershipCatalogProbeV1,NULL,xl6OwnershipCatalogResolveFinalizationV1,false,catalog,xl6OwnershipCatalogBoundaryV1Get(catalog),&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
 assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);bool failed=false;
 assert(xl6OwnershipPlanFinalizeStepV1(plan,&failed)==XL6_OWNERSHIP_STEP_READY&&!failed);
 assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
 assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY&&!xl6ForeignViewPendingV1(0));
 xl6OwnershipCatalogDestroyV1(catalog);
}
int main(void)
{
 assert(xrtMemDebugEnable(true));own();malformed();defensive();codeLifetime();affinity();cycle();xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
 for(unsigned i=0;i<20;++i){own();malformed();codeLifetime();affinity();cycle();}
 size_t prefix=prefixes();xrtClearError();xrtMemDebugSnapshot(&after);
 assert(made==freed&&before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&!xl6ForeignViewPendingV1(0));
 printf("Owned native sequence CPU primitives: %u actual owners, %u exact Native retains, %u direct typed traversal scopes, %zu pre-retain OOM prefixes; original close/copy/slice/deferred/worker/codelease balanced; no backing copies\n",freed,retains,loadedScopes,prefix);return 0;
}
