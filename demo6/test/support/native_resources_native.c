#include "release/include/xruntime_decl.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_process_run_internal.h"
#include "src/runtime/xl6_random_state.h"
#include "src/runtime/xl6_fs_file.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef const XL6_NativeHandlePolicyV1* (*Policy)(void);
typedef struct Case { const char* name;Policy policy;size_t nodes; } Case;
static const Case cases[]={
    {"xl6DateTimeDestroyV1",xl6DateTimeHandlePolicyV1Get,1},
    {"xl6UnicodeValidatorDestroyV1",xl6UnicodeValidatorHandlePolicyV1Get,1},
    {"xl6TextIteratorDestroyV1",xl6TextIteratorHandlePolicyV1Get,1},
    {"xl6PatternDestroyV1",xl6PatternHandlePolicyV1Get,1},
    {"xl6PatternOptionsDestroyV1",xl6PatternOptionsHandlePolicyV1Get,1},
    {"xl6PatternMatchDestroyV1",xl6PatternMatchHandlePolicyV1Get,1},
    {"xl6PatternBuilderDestroyV1",xl6PatternBuilderHandlePolicyV1Get,2},
    {"xl6PatternEditDestroyV1",xl6PatternEditHandlePolicyV1Get,3},
    {"xl6PatternKeyDestroyV1",xl6PatternKeyHandlePolicyV1Get,3},
    {"xl6ProcessPipelineDestroyV1",xl6ProcessPipelineHandlePolicyV1Get,1},
    {"xl6PipelineResultDestroyV1",xl6PipelineResultHandlePolicyV1Get,1},
    {"xl6ProcessStageDestroyV1",xl6ProcessStageHandlePolicyV1Get,1},
    {"xl6ProcessResultDestroyV1",xl6ProcessResultHandlePolicyV1Get,1},
    {"xl6RegexDestroyV1",xl6RegexHandlePolicyV1Get,1},
    {"xl6RegexMatchDestroyV1",xl6RegexMatchHandlePolicyV1Get,2},
    {"xl6RegexIteratorDestroyV1",xl6RegexIteratorHandlePolicyV1Get,2},
    {"xl6RegexSplitDestroyV1",xl6RegexSplitHandlePolicyV1Get,2},
    {"xl6RegexSplitPartDestroyV1",xl6RegexSplitPartHandlePolicyV1Get,2},
    {"xl6RegexSetDestroyV1",xl6RegexSetHandlePolicyV1Get,1},
    {"xl6RegexReplacerDestroyV1",xl6RegexReplacerHandlePolicyV1Get,2},
    {"xl6SignalQueueDestroyV1",xl6SignalQueueHandlePolicyV1Get,1},
    {"xl6SignalEventDestroyV1",xl6SignalEventHandlePolicyV1Get,1},
    {"private-publication",xl6PatternPublicationHandlePolicyV1Get,1},
    {"xl6ProcessRunOptionsDestroyV1",xl6ProcessRunOptionsHandlePolicyV1Get,2},
    {"xl6_xrt_rng_destroy",xl6RandomStateHandlePolicyV1Get,1},
    {"xl6_xrt_fs_file_destroy",xl6FsFileHandlePolicyV1Get,1},
    {"xl6_xrt_vfs_destroy",xl6VfsHandlePolicyV1Get,1},
    {"xl6_xrt_vfs_memory_destroy",xl6VfsMemoryHandlePolicyV1Get,1},
    {"xl6_xrt_vfs_disk_destroy",xl6VfsDiskHandlePolicyV1Get,1},
    {"xl6_xrt_vfs_pack_destroy",xl6VfsPackHandlePolicyV1Get,1},
    {"xl6_xrt_vfs_mount_destroy",xl6VfsMountHandlePolicyV1Get,1},
    {"xl6_xrt_fs_info_destroy",xl6FsInfoHandlePolicyV1Get,1},
    {"xl6_xrt_fs_dir_destroy",xl6FsDirHandlePolicyV1Get,1},
    {"xl6_xrt_fs_pack_options_destroy",xl6PackOptionsHandlePolicyV1Get,1},
    {"xl6_xrt_fs_pack_stats_destroy",xl6PackStatsHandlePolicyV1Get,1}
};
static const char* child;
static const char* fixtureRoot;
static const char* fixturePack;
static bool forbidden(const xvalue* value,xrtownershipvisitor visitor,void* context)
{(void)value;(void)visitor;(void)context;assert(0);return false;}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void* regex(void)
{return xl6RegexCreateV1(XRT_STR_LITERAL("a"),0,1048576,64,-1);}
static void* command(void)
{
    xvalue* args=xrtValueArray();assert(args && xrtValueArrayAppendNew(args,xrtValueString(XRT_STR_LITERAL("echo"))));
    void* result=xl6ProcessCommandDirectV1((xstrview){child,strlen(child)},args,-1,-1);xrtValueRelease(args);return result;
}
static void* processResult(bool pipeline)
{
    void* cmd=command();assert(cmd);
    void* options=xl6ProcessRunOptionsCreateV1(5000,4096,4096,0,100,0);assert(options);
    xvalue* bytes=xrtValueBytes((xbytesview){(const unsigned char*)"a\0b",3});assert(bytes);
    void* result;
    if(pipeline){
        void* stages=xl6ProcessPipelineCreateV1();assert(stages);xl6ProcessPipelineAddV1(stages,cmd);assert(!xrtGetError());
        result=xl6ProcessPipelineRunV1(stages,options,bytes,-1);xl6ProcessPipelineDestroyV1(stages);
    }else result=xl6ProcessRunV1(cmd,options,bytes,-1);
    xl6ProcessRunOptionsDestroyV1(options);xl6ProcessCommandDestroyV1(cmd);xrtValueRelease(bytes);return result;
}
static xstrview packName(void)
{ return (xstrview){fixturePack,strlen(fixturePack)}; }
static void* memory(void)
{
    void* provider=xl6_xrt_vfs_memory_create();assert(provider);
    assert(xl6_xrt_vfs_memory_put_string(provider,XRT_STR_LITERAL("entry.bin"),XRT_STR_LITERAL("a\0b"),-1));
    assert(xl6_xrt_vfs_memory_seal(provider));return provider;
}
static void* populatedVfs(void)
{
    void* fs=xl6_xrt_vfs_create();assert(fs);void* provider=memory();
    void* mount=xl6_xrt_vfs_mount_memory(fs,provider,XRT_STR_LITERAL("/app"),0,0);assert(mount);
    xl6_xrt_vfs_memory_destroy(provider);xl6_xrt_vfs_mount_destroy(mount);return fs;
}
static void* create(unsigned which)
{
    if(which==0)return xl6DateTimeFieldsV1(2024,2,29,0,0,0,0,0);
    if(which==1)return xl6UnicodeValidatorCreateV1();
    if(which==2)return xl6TextIteratorCreateV1(XRT_STR_LITERAL("a\0b"),XRT_STR_LITERAL(","),0,-1,-1);
    if(which==3)return xl6PatternCreateV1(XRT_STR_LITERAL("*"),-1);
    if(which==4){xpatternconfig c;xrtPatternConfigInit(&c);return xl6PatternOptionsCreateV1(XRT_STR_LITERAL("/"),
        c.MaxPatternBytes,c.MaxPatterns,c.MaxCaptures,c.MaxStates,c.MaxCompiledBytes,-1);}
    if(which==5){void* p=xl6PatternCreateV1(XRT_STR_LITERAL("*"),-1);assert(p);
        void* result=xl6PatternMatchCreateV1(p,XRT_STR_LITERAL("a\0b"),-1);xl6PatternDestroyV1(p);return result;}
    if(which==6)return xl6PatternBuilderCreateV1();
    if(which==7 || which==8){
        void* b=xl6PatternBuilderCreateV1();assert(b);void* edit=xl6PatternEditAddV1(b,XRT_STR_LITERAL("*"),0,-1);assert(edit);
        if(which==7){xl6PatternBuilderDestroyV1(b);return edit;}
        void* key=xl6PatternKeyCreateV1(edit,0);xl6PatternEditDestroyV1(edit);xl6PatternBuilderDestroyV1(b);return key;
    }
    if(which==9){void* p=xl6ProcessPipelineCreateV1();void* c=command();assert(p && c);
        xl6ProcessPipelineAddV1(p,c);xl6ProcessCommandDestroyV1(c);return p;}
    if(which==10)return processResult(true);
    if(which==11){void* p=processResult(true);assert(p);void* result=xl6PipelineResultStageV1(p,0);xl6PipelineResultDestroyV1(p);return result;}
    if(which==12)return processResult(false);
    if(which==13)return regex();
    if(which>=14 && which<=19){
        if(which==18){xvalue* patterns=xrtValueArray();assert(patterns && xrtValueArrayAppendNew(patterns,xrtValueString(XRT_STR_LITERAL("a"))));
            void* result=xl6RegexSetCreateV1(patterns,0,1048576,64,4,-1);xrtValueRelease(patterns);return result;}
        void* r=regex();assert(r);void* result;
        if(which==14)result=xl6RegexMatchCreateV1(r,XRT_STR_LITERAL("a\0b"),0,0,-1);
        else if(which==15)result=xl6RegexIteratorCreateV1(r,XRT_STR_LITERAL("a\0b"),0,-1,-1);
        else if(which==16 || which==17){
            result=xl6RegexSplitCreateV1(r,XRT_STR_LITERAL("a\0b"),-1,0,-1);
            if(which==17){assert(result && xl6RegexSplitHasNextV1(result));void* part=xl6RegexSplitSnapshotV1(result);
                xl6RegexSplitDestroyV1(result);result=part;}
        }else result=xl6RegexReplacerCreateV1(r,XRT_STR_LITERAL("a\0b"),-1,-1);
        xl6RegexDestroyV1(r);return result;
    }
    if(which==20)return xl6SignalQueueCreateV1(XSIGNAL_INT,false,8);
    if(which==21){void* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,8);assert(q && xrtSignalRaise(XSIGNAL_INT));
        assert(xl6SignalQueuePrepareV1(q,5000));void* result=xl6SignalQueueSnapshotV1(q);xl6SignalQueueDestroyV1(q);return result;}
    if(which==22)return xrtCalloc(1,sizeof(xatomic32));
    if(which==23)return xl6ProcessRunOptionsCreateV1(5000,4096,4096,0,100,0);
    if(which==24)return xl6_xrt_rng_create(42,1);
    if(which==25){xlir_fs_file* file=xlir_fs_file_alloc();assert(file);
        file->file=xrtOpen(child,XFILE_READ);assert(file->file);return file;}
    if(which==26)return populatedVfs();
    if(which==27)return memory();
    if(which==28)return xl6_xrt_vfs_disk_create((xstrview){fixtureRoot,strlen(fixtureRoot)},XVFS_DISK_READ);
    if(which==29)return xl6_xrt_vfs_pack_create(packName(),0,-1,0,1048576,1048576,1048576);
    if(which==30){void* fs=xl6_xrt_vfs_create();assert(fs);void* provider=memory();
        void* mount=xl6_xrt_vfs_mount_memory(fs,provider,XRT_STR_LITERAL("/app"),0,0);assert(mount);
        xl6_xrt_vfs_memory_destroy(provider);xl6_xrt_vfs_destroy(fs);return mount;}
    if(which==31 || which==32){void* fs=populatedVfs();
        void* result=which==31?xl6_xrt_fs_info_vfs(fs,XRT_STR_LITERAL("/app/entry.bin"),true):xl6_xrt_fs_vfs_dir_open(fs,XRT_STR_LITERAL("/app"),0);
        xl6_xrt_vfs_destroy(fs);return result;}
    if(which==33)return xl6_xrt_fs_pack_options_create(0,100,1048576,1048576,1048576,1048576);
    assert(which==34);void* pack=create(29);assert(pack);void* result=xl6_xrt_fs_pack_stats_create(pack);
    xl6_xrt_vfs_pack_destroy(pack);return result;
}
static XL6_OwnershipCatalog* catalog(void)
{
    const XL6_OwnershipDomainV1* domains[]={
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) Getter(),
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    };return xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));
}
static bool admit(xrtownershipref ref,void* data)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* ready=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    return xl6OwnershipCatalogResolveV1(ref,data,&adapter,&ready,&finish);
}
static void graph(unsigned which,XL6_OwnershipCatalog* cat,xvalue* value,size_t nodes)
{
    xrtownershipref root=xrtValueOwnership(value);xrtownershipsnapshot* snapshot=NULL;xrtownershipscope scope={0};
    assert(xrtOwnershipFreezeTryBegin(&scope));assert(xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,cat,&snapshot));
    assert(xrtOwnershipSnapshotNodeCount(snapshot)==nodes);xrtOwnershipSnapshotDestroy(snapshot);
    assert(xrtOwnershipScopeEnd(&scope));
    if(which==15 || which==16){void* raw=NULL;assert(xrtValueGetHandle(value,&raw,NULL,NULL));
        if(which==15)xl6RegexIteratorCloseV1(raw);else xl6RegexSplitCloseV1(raw);
        graph(99,cat,value,1);
    }
}
static void legacyNotify(void* data)
{ (void)data;assert(0); }
static void vfsConstructorFailures(void)
{
    static const unsigned kinds[]={26,27,28,29,33,34,25,31,32,30};
    for(unsigned operation=0;operation<10;++operation){
        size_t point;
        for(point=0;point<256;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            void* parent=NULL;void* provider=NULL;void* result=NULL;
            if(operation==5){parent=create(29);assert(parent);}
            else if(operation>=6 && operation<=8)parent=populatedVfs();
            else if(operation==9){parent=xl6_xrt_vfs_create();provider=memory();assert(parent && provider);}
            assert(xrtMemDebugFailAfter(point));
            switch(operation){
            case 0:result=xl6_xrt_vfs_create();break;
            case 1:result=xl6_xrt_vfs_memory_create();break;
            case 2:result=xl6_xrt_vfs_disk_create((xstrview){fixtureRoot,strlen(fixtureRoot)},XVFS_DISK_READ);break;
            case 3:result=xl6_xrt_vfs_pack_create(packName(),0,-1,0,1048576,1048576,1048576);break;
            case 4:result=xl6_xrt_fs_pack_options_create(0,100,1048576,1048576,1048576,1048576);break;
            case 5:result=xl6_xrt_fs_pack_stats_create(parent);break;
            case 6:result=xl6_xrt_fs_vfs_open(parent,XRT_STR_LITERAL("/app/entry.bin"),1,0,7);break;
            case 7:result=xl6_xrt_fs_info_vfs(parent,XRT_STR_LITERAL("/app/entry.bin"),true);break;
            case 8:result=xl6_xrt_fs_vfs_dir_open(parent,XRT_STR_LITERAL("/app"),0);break;
            case 9:result=xl6_xrt_vfs_mount_memory(parent,provider,XRT_STR_LITERAL("/app"),0,0);break;
            }
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(hit?!result:result!=NULL);
            if(result)cases[kinds[operation]].policy()->Ops->Drop(result,NULL);
            if(operation==5)xl6_xrt_vfs_pack_destroy(parent);
            else if(operation>=6)xl6_xrt_vfs_destroy(parent);
            xl6_xrt_vfs_memory_destroy(provider);balance(&before);
            if(!hit)break;
        }
        assert(point<256);printf("vfs factory %u prefixes=%zu first-no-hit=%zu balanced\n",operation,point+1,point);
    }
}
static void cancellationAndFileOwners(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* raw=create(23);XL6_ProcessRunOptions* options=raw;
    xvalue* value=xl6NativeHandleTakeV1(&raw,xl6ProcessRunOptionsHandlePolicyV1Get());assert(value && !raw);
    XL6_OwnershipCatalog* cat=catalog();assert(cat);
    xcancelwatch* watch=xrtCancelWatch(options->options.Cancel,legacyNotify,NULL);assert(watch);
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    assert(!xl6OwnershipCatalogResolveV1(xrtCancelOwnership(options->options.Cancel),cat,&adapter,&preparation,&finish));
    assert(!adapter && !preparation && !finish);assert(xrtOwnershipScopeEnd(&freeze));
    xrtCancelUnwatch(watch);xrtClearError();graph(99,cat,value,2);
    xcancel* retained=xrtCancelRef(options->options.Cancel);assert(retained);
    xrtValueRelease(value);assert(xrtCancelRequest(retained) && xrtCancelRequested(retained));xrtCancelDestroy(retained);
    raw=create(25);xlir_fs_file* file=raw;assert(xlir_fs_file_ref(file));
    value=xl6NativeHandleTakeV1(&raw,xl6FsFileHandlePolicyV1Get());assert(value && !raw);
    xrtValueRelease(value);uint64_t size=0;assert(xrtFileSize(file->file,&size) && size>0);
    xlir_fs_file_drop(file);xl6OwnershipCatalogDestroyV1(cat);balance(&before);
}
typedef struct ConcurrentClose {
    unsigned which;
    xvalue* value;
    xatomic32 ready,go,attempt;
} ConcurrentClose;
static int32 closeWorker(void* data)
{
    ConcurrentClose* work=data;void* raw=create(work->which);assert(raw);
    work->value=xl6NativeHandleTakeV1(&raw,cases[work->which].policy());assert(work->value && !raw);
    xrtAtomic32Store(&work->ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&work->go,XMEMORY_ACQUIRE))xrtThreadYield();
    assert(xrtValueGetHandle(work->value,&raw,NULL,NULL));xrtAtomic32Store(&work->attempt,1,XMEMORY_RELEASE);
    if(work->which==15)xl6RegexIteratorCloseV1(raw);else xl6RegexSplitCloseV1(raw);
    assert(!xrtGetError());return 0;
}
static void concurrentClose(unsigned which)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ConcurrentClose work={.which=which};
    xthread* thread=xrtThreadCreate(closeWorker,&work,0);assert(thread);
    double deadline=xrtTimer()+5;
    while(!xrtAtomic32Load(&work.ready,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    XL6_OwnershipCatalog* cat=catalog();assert(cat);xrtownershipscope scope={0};
    while(!xrtOwnershipFreezeTryBegin(&scope)){assert(xrtTimer()<deadline);xrtThreadYield();}
    xrtAtomic32Store(&work.go,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&work.attempt,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    /* The creator can begin Close, but cannot detach input while this other
     * thread owns freeze. A real snapshot still sees its complete owning edge. */
    xrtownershipref root=xrtValueOwnership(work.value);xrtownershipsnapshot* snapshot=NULL;
    assert(xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,cat,&snapshot));
    assert(xrtOwnershipSnapshotNodeCount(snapshot)==2);xrtOwnershipSnapshotDestroy(snapshot);
    assert(xrtOwnershipScopeEnd(&scope));assert(xrtThreadWait(thread)==XWAIT_OK && !xrtThreadExitCode(thread));
    xrtThreadDestroy(thread);graph(99,cat,work.value,1);xl6OwnershipCatalogDestroyV1(cat);xrtValueRelease(work.value);balance(&before);
}
int main(int argc,char** argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    assert(argc==4 && xrtMemDebugEnable(true));child=argv[1];fixtureRoot=argv[2];fixturePack=argv[3];assert(sizeof(cases)/sizeof(*cases)==35);
    void* warm=create(20);assert(warm);xl6SignalQueueDestroyV1(warm);
    for(unsigned which=0;which<35;++which){
        const Case* test=&cases[which];const XL6_NativeHandlePolicyV1* policy=test->policy();
        for(unsigned round=0;round<4;++round){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* raw=create(which);assert(raw && !xrtGetError());
            xvalue* value=xl6NativeHandleTakeV1(&raw,policy);assert(value && !raw);
            XL6_OwnershipCatalog* cat=catalog();assert(cat);graph(which,cat,value,test->nodes);
            xl6OwnershipCatalogDestroyV1(cat);xrtValueRelease(value);balance(&before);
        }
        for(unsigned forged=0;forged<2;++forged){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* raw=create(which);assert(raw);
            xvaluehandleops copied=*policy->Ops;xvalue* value=xrtValueHandleTake(&raw,forged?policy->Ops:&copied,NULL);assert(value && !raw);
            assert(xrtValueHandleOwnershipBindPhased(value,forbidden));XL6_OwnershipCatalog* cat=catalog();assert(cat);
            const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* ready=NULL;const XL6_OwnershipFinishV1* finish=NULL;
            xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope));
            assert(!xl6OwnershipCatalogResolveV1(xrtValueOwnership(value),cat,&adapter,&ready,&finish) && !adapter && !ready && !finish);
            assert(xrtOwnershipScopeEnd(&scope));xl6OwnershipCatalogDestroyV1(cat);xrtValueRelease(value);balance(&before);
        }
        size_t point=0;
        for(;point<64;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* raw=create(which);assert(raw);
            assert(xrtMemDebugFailAfter(point));xvalue* value=xl6NativeHandleTakeV1(&raw,policy);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(!raw && (hit?!value:value!=NULL));
            xrtValueRelease(value);balance(&before);if(!hit)break;
        }
        assert(point<64);printf("native resource %s nodes=%zu prefixes=%zu first-no-hit=%zu balanced\n",test->name,test->nodes,point+1,point);
    }
    concurrentClose(15);concurrentClose(16);cancellationAndFileOwners();vfsConstructorFailures();
    assert(xrtSignalShutdown());puts("native resources: 35 exact policies, real factories, owning graphs, closed prefixes and 70 rejecting controls; two concurrent close transitions; opaque cancel observer rejected and shared owners preserved");return 0;
}
