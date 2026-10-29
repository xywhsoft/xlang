#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_io_private.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_value_admission.h"
#include <assert.h>
#include <stdio.h>

static const char* child;
static void frozen(xrtownershipscope* scope)
{ double deadline=xrtTimer()+5;while(!xrtOwnershipFreezeTryBegin(scope)){assert(xrtTimer()<deadline);xrtThreadYield();} }
static XL6_OwnershipCatalog* catalog(void)
{
    const XL6_OwnershipDomainV1* domains[]={
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) Getter(),
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    };return xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));
}
static bool resolve(xrtownershipref ref,void* data,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{ return xl6OwnershipCatalogResolveV1(ref,data,adapter,preparation,finish); }
static bool admit(xrtownershipref ref,void* data)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    return resolve(ref,data,&adapter,&preparation,&finish);
}
static void balanced(const xmemdebugsnapshot* before)
{
    double deadline=xrtTimer()+5;xmemdebugsnapshot after;xrtClearError();
    for(;;){xrtMemDebugSnapshot(&after);
        assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
            after.UseAfterFreeCount==before->UseAfterFreeCount);
        if(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
            after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount)break;
        assert(xrtTimer()<deadline);xrtThreadYield();
    }
}
static XL6_ProcessLive* spawn(void)
{
    xvalue* args=xrtValueArray();assert(args && xrtValueArrayAppendNew(args,xrtValueString(XRT_STR_LITERAL("echo"))));
    void* command=xl6ProcessCommandDirectV1((xstrview){child,strlen(child)},args,-1,-1);xrtValueRelease(args);assert(command);
    ((XL6_ProcessCommand*)command)->config.Stderr.Mode=XPROCESS_IO_NULL;
    XL6_ProcessLive* process=xl6ProcessSpawnV1(command);xl6ProcessCommandDestroyV1(command);assert(process);return process;
}
static xvalue* boxed(void* raw,const XL6_NativeHandlePolicyV1* policy)
{ xvalue* value=xl6NativeHandleTakeV1(&raw,policy);assert(value && !raw);return value; }
static void snapshot(XL6_OwnershipCatalog* cat,xvalue* value,size_t expected,bool accepted)
{
    xrtownershipref root=xrtValueOwnership(value);xrtownershipsnapshot* graph=NULL;xrtownershipscope freeze={0};frozen(&freeze);
    bool ok=xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,cat,&graph);assert(ok==accepted);
    if(ok)assert(graph && xrtOwnershipSnapshotNodeCount(graph)==expected);else assert(!graph);
    xrtOwnershipSnapshotDestroy(graph);assert(xrtOwnershipScopeEnd(&freeze));xrtClearError();
}
static void moves(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_OwnershipCatalog* cat=catalog();assert(cat);
    XL6_ProcessLive* process=spawn();xlir_io_reader* reader=xl6ProcessReaderV1(process,XPROCESS_STDOUT);
    xlir_io_writer* writer=xl6ProcessWriterV1(process);assert(reader && writer && process->refs==3);
    xvalue* input=boxed(writer,xl6IoWriterHandlePolicyV1Get()),*output=boxed(reader,xl6IoReaderHandlePolicyV1Get());
    snapshot(cat,input,3,true);snapshot(cat,output,3,true);
    assert(!xl6_xrt_io_lines_create(reader,0));xrtClearError();
    assert(reader->stream && reader->contextOwner.Data==process);snapshot(cat,output,3,true);
    void* lines=xl6_xrt_io_lines_create(reader,128);assert(lines && !reader->stream && !reader->contextOwner.Data && !reader->contextOwner.Ops);
    xvalue* lineValue=boxed(lines,xl6IoLinesHandlePolicyV1Get());
    assert(process->refs==3);snapshot(cat,output,1,true);snapshot(cat,lineValue,3,true);
    assert(xl6_xrt_io_writer_close(writer) && xl6_xrt_io_writer_close(writer));snapshot(cat,input,1,true);
    assert(xl6_xrt_io_lines_close(lines) && xl6_xrt_io_lines_close(lines));snapshot(cat,lineValue,1,true);
    assert(xl6_xrt_io_reader_close(reader) && process->refs==1 && !process->leased);
    xrtValueRelease(input);xrtValueRelease(output);xrtValueRelease(lineValue);xl6ProcessDestroyV1(process);
    xl6OwnershipCatalogDestroyV1(cat);balanced(&before);
    puts("process IO: exact 3-node paths; move and failed Take preserve one physical context reference; closed handles are leaves");
}
static bool forbiddenCount(const void* raw,size_t* count) { (void)raw;(void)count;abort(); }
static bool forbiddenTrace(const void* raw,xrtownershipvisitor visit,void* context) { (void)raw;(void)visit;(void)context;abort(); }
static void refusal(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_OwnershipCatalog* cat=catalog();assert(cat);
    xvalue* bytes=xrtValueBytes((xbytesview){(const unsigned char*)"x",1});assert(bytes);
    xlir_io_reader* reader=xl6_xrt_io_reader_bytes(bytes,-1);xrtValueRelease(bytes);assert(reader);
    xvalue* value=boxed(reader,xl6IoReaderHandlePolicyV1Get());
    reader->origin=XL6_IO_ORIGIN_UNKNOWN;snapshot(cat,value,0,false);
    reader->origin=XL6_IO_ORIGIN_PROCESS;snapshot(cat,value,0,false);
    static const xrtownershipops forbidden={forbiddenCount,forbiddenTrace};
    reader->contextOwner=(xrtownershipref){reader,&forbidden};snapshot(cat,value,0,false);
    reader->origin=XL6_IO_ORIGIN_BUILTIN;snapshot(cat,value,0,false);
    reader->contextOwner=(xrtownershipref){0};snapshot(cat,value,1,true);
    xrtValueRelease(value);xl6OwnershipCatalogDestroyV1(cat);balanced(&before);
    puts("process IO: unknown provenance, missing and unadmitted context edges refused before foreign Count/Trace");
}
static void cycle(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_OwnershipCatalog* cat=catalog();assert(cat);
    XL6_ProcessLive* process=spawn();xprocess* native=process->native;xrtownershipref nativeRef=xrtProcessOwnership(native);
    xrtownershipscope freeze={0};frozen(&freeze);const xrtownershipadapterv1* nativeAdapter=xrtProcessOwnershipAdapterV1(nativeRef);
    assert(nativeAdapter && nativeAdapter->Hold(native));assert(xrtOwnershipScopeEnd(&freeze));
    xfuture* terminal=xrtProcessWaitAsync(native);assert(terminal);
    xlir_io_reader* reader=xl6ProcessReaderV1(process,XPROCESS_STDOUT);void* writer=xl6ProcessWriterV1(process);assert(reader && writer);
    void* lines=xl6_xrt_io_lines_create(reader,128);assert(lines);
    xvalue* owned[]={boxed(process,xl6ProcessHandlePolicyV1Get()),boxed(writer,xl6IoWriterHandlePolicyV1Get()),
        boxed(reader,xl6IoReaderHandlePolicyV1Get()),boxed(lines,xl6IoLinesHandlePolicyV1Get())};
    const uint64_t identity=UINT64_C(0x99120001),element=UINT64_C(0x99120002);
    xvalue* ring=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_STACK_V1,identity,element,XL6_VALUE_ADMIT_ANY,false,element);assert(ring);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(ring,XL6_SEQUENCE_STACK_V1,identity,element,3,sizeof(xvalue*),xrtTypeValue());assert(pin);
    assert(xl6SequenceAdmitSlotV1(pin,&ring) && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&ring));
    for(size_t i=0;i<4;++i){assert(xl6SequenceAdmitSlotV1(pin,&owned[i]) && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&owned[i]));}
    xl6SequenceEndV1(pin);for(size_t i=0;i<4;++i)xrtValueRelease(owned[i]);xrtValueRelease(ring);
    XL6_OwnershipPlan* plan=NULL;XL6_OwnershipPlanResult result;double deadline=xrtTimer()+5;
    do{result=xl6OwnershipPlanTryCreateV3(xrtValueOwnership(ring),resolve,cat,&plan);if(result==XL6_OWNERSHIP_PLAN_BUSY){assert(xrtTimer()<deadline);xrtThreadYield();}}while(result==XL6_OWNERSHIP_PLAN_BUSY);
    assert(result==XL6_OWNERSHIP_PLAN_READY && plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    do{result=xl6OwnershipPlanTryCommit(plan);if(result==XL6_OWNERSHIP_PLAN_BUSY){assert(xrtTimer()<deadline);xrtThreadYield();}}while(result==XL6_OWNERSHIP_PLAN_BUSY);
    assert(result==XL6_OWNERSHIP_PLAN_READY && process->native==native && process->cleared && process->leased);
    assert(!xrtFutureDone(terminal));
    const XL6_OwnershipFinishV1* completion=xl6ProcessLiveOwnershipFinishV1Get();
    assert(completion->Step(process,plan)==XRT_OWNERSHIP_FINISH_BUSY && process->native==native && !xrtGetError());
    XL6_OwnershipStepResult step;
    do{step=xl6OwnershipPlanFinishV2(plan);assert(step>=XL6_OWNERSHIP_STEP_BUSY && xrtTimer()<deadline);}while(step==XL6_OWNERSHIP_STEP_BUSY);
    assert(!process->native && !process->leased && !xrtGetError());
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtFutureWaitFor(terminal,UINT64_C(5000000))==XWAIT_OK && ((const xprocessstatus*)xrtFutureValue(terminal))->Code==0);
    nativeAdapter->Drop(native);xrtFutureDestroy(terminal);xl6OwnershipCatalogDestroyV1(cat);balanced(&before);
    puts("process IO cycle: Clear preserves streams; native Finish BUSY until real leases retire; repeated Finish and complete ledger balanced");
}
static void prefixes(void)
{
    for(unsigned operation=0;operation<3;++operation){unsigned point;
        for(point=0;point<32;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ProcessLive* process=spawn();
            xlir_io_reader* reader=operation==2?xl6ProcessReaderV1(process,XPROCESS_STDOUT):NULL;
            assert(operation!=2 || reader);assert(xrtMemDebugFailAfter(point));
            void* result=operation==0?xl6ProcessReaderV1(process,XPROCESS_STDOUT):operation==1?xl6ProcessWriterV1(process):xl6_xrt_io_lines_create(reader,128);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(hit?!result:result!=NULL);xrtClearError();
            if(!result){
                if(operation==2)assert(reader->stream && reader->contextOwner.Data==process && process->refs==2);
                else assert(!process->leased && !process->closed && process->refs==1);
                result=operation==0?xl6ProcessReaderV1(process,XPROCESS_STDOUT):operation==1?xl6ProcessWriterV1(process):xl6_xrt_io_lines_create(reader,128);assert(result);
            }
            if(operation==0)xl6_xrt_io_reader_destroy(result);
            else if(operation==1)xl6_xrt_io_writer_destroy(result);
            else {xl6_xrt_io_lines_destroy(result);xl6_xrt_io_reader_destroy(reader);}
            assert(process->refs==1 && !process->leased);xl6ProcessDestroyV1(process);balanced(&before);if(!hit)break;
        }
        assert(point>0 && point<32);printf("process IO operation=%u prefixes=%u first-no-hit=%u balanced\n",operation,point+1,point);
    }
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));child=argv[1];setvbuf(stdout,NULL,_IONBF,0);
    xrtSetErrorKind(XERR_MEMORY);xrtClearError();moves();refusal();cycle();prefixes();assert(!xrtGetError());return 0;
}
