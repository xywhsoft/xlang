#include "src/runtime/xl6_io_private.h"
#include "src/runtime/xl6_fs_file.h"
#include "src/runtime/xl6_fs_resource.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>

static xvalue* fixtureBytes;
static xbuffer* buffer;
static xlir_fs_file* file;
static xstrview name;
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount && after.UseAfterFreeCount==before->UseAfterFreeCount);
    assert(xrtAtomic32Load(&file->refs,XMEMORY_ACQUIRE)==1);
}
static void* create(unsigned which,void* parent)
{
    switch(which){
    case 0:return xl6_xrt_io_reader_bytes(fixtureBytes,20);
    case 1:return xl6_xrt_io_reader_buffer(buffer,-1);
    case 2:return xl6_xrt_io_reader_open(name);
    case 3:return xl6_xrt_io_writer_open(name,1);
    case 4:return xl6_xrt_io_writer_buffer();
    case 5:return xl6_xrt_io_writer_standard(3);
    case 6:return xl6_xrt_io_lines_create(parent,32);
    case 7:return xl6_xrt_fs_file_reader(file);
    case 8:return xl6_xrt_fs_file_writer(file);
    case 9:return xl6_xrt_io_read_file(name,32);
    default:assert(0);return NULL;
    }
}
static void destroy(unsigned which,void* result)
{
    if(which==6)xl6_xrt_io_lines_destroy(result);
    else if(which==9)xrtValueRelease(result);
    else if(which==3 || which==4 || which==5 || which==8)xl6_xrt_io_writer_destroy(result);
    else xl6_xrt_io_reader_destroy(result);
}
static void factories(void)
{
    for(unsigned which=0;which<10;++which){
        unsigned first=0;
        for(unsigned point=0;point<256;++point){
            xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
            void* parent=which==6?xl6_xrt_io_reader_bytes(fixtureBytes,-1):NULL;
            if(which==6)assert(parent);
            assert(xrtMemDebugFailAfter(point));void* result=create(which,parent);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            assert(hit?!result:result!=NULL);
            if(which==6)assert(xl6_xrt_io_reader_closed(parent)==(result!=NULL));
            destroy(which,result);xl6_xrt_io_reader_destroy(parent);balance(&before);
            if(!hit){first=point;printf("io factory %u prefixes=%u first-no-hit=%u balanced\n",which,point+1,point);break;}
        }
        assert(first>0);
    }
}
static void bytesEqual(xvalue* value,const char* expected,size_t size)
{
    xbytesview view={0};assert(value && xrtValueGetBytes(value,&view));
    assert(view.Size==size && (!size || memcmp(view.Data,expected,size)==0));xrtValueRelease(value);
}
static void operations(void)
{
    void* reader=xl6_xrt_io_reader_bytes(fixtureBytes,20);assert(reader);
    assert(xl6_xrt_io_reader_size(reader)==3 && xl6_xrt_io_reader_can_seek(reader));
    bytesEqual(xl6_xrt_io_reader_read(reader,2),"a\0",2);
    assert(xl6_xrt_io_reader_tell(reader)==2 && xl6_xrt_io_reader_seek(reader,0,0)==0);
    void* writer=xl6_xrt_io_writer_buffer();assert(writer);
    assert(xl6_xrt_io_copy(reader,writer,3,1)==3);
    assert(xl6_xrt_io_writer_all_text(writer,XRT_STR_LITERAL("\0z"),20));
    bytesEqual(xl6_xrt_io_writer_to_bytes(writer),"a\0b\0z",5);
    assert(xl6_xrt_io_writer_seek(writer,0,0)==0 && xl6_xrt_io_writer_all_bytes(writer,fixtureBytes,1));
    assert(xl6_xrt_io_writer_flush(writer));assert(xl6_xrt_io_writer_close(writer));
    assert(xl6_xrt_io_writer_close(writer) && xl6_xrt_io_writer_closed(writer));
    bytesEqual(xl6_xrt_io_writer_to_bytes(writer),"a\0b\0z",5);
    xl6_xrt_io_reader_destroy(reader);xl6_xrt_io_writer_destroy(writer);
    xvalue* input=xrtValueBytes((xbytesview){(const unsigned char*)"a\0b\r\nz\n",7});assert(input);
    reader=xl6_xrt_io_reader_bytes(input,-1);assert(reader);xrtValueRelease(input);
    void* lines=xl6_xrt_io_lines_create(reader,4);assert(lines && xl6_xrt_io_reader_closed(reader));
    assert(xl6_xrt_io_lines_advance(lines));bytesEqual(xl6_xrt_io_lines_bytes(lines),"a\0b",3);
    assert(xl6_xrt_io_lines_ending(lines)==XLINE_END_CRLF);
    assert(xl6_xrt_io_lines_advance(lines));bytesEqual(xl6_xrt_io_lines_bytes(lines),"z",1);
    assert(!xl6_xrt_io_lines_advance(lines) && !xrtGetError());
    xl6_xrt_io_lines_destroy(lines);xl6_xrt_io_reader_destroy(reader);
    assert(!xl6_xrt_io_reader_open(XRT_STR_LITERAL("a\0b")) && xrtErrorKind(xrtGetError())==XERR_VALUE);xrtClearError();
    assert(!xl6_xrt_io_writer_open(XRT_STR_LITERAL("a\0b"),1) && xrtErrorKind(xrtGetError())==XERR_VALUE);xrtClearError();
    assert(!xl6_xrt_io_write_text_file(name,XRT_STR_LITERAL("data"),1,-2) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
}
static void noisyDrop(void* context)
{ ++*(unsigned*)context;xrtSetErrorKind(XERR_IO); }
static bool readEmpty(void* context,void* data,size_t size,size_t* done)
{ (void)context;(void)data;(void)size;*done=0;return true; }
static bool noisyClose(void* context)
{ noisyDrop(context);return false; }
static void firstFailure(void)
{
    unsigned closed=0;
    assert(!xlir_io_writer_take(NULL,&closed,noisyDrop) && closed==1 && !xrtGetError());
    xrtSetErrorKind(XERR_RANGE);const xerror* first=xrtGetError();assert(first);
    assert(!xlir_io_writer_take(NULL,&closed,noisyDrop) && closed==2 && xrtGetError()==first);xrtClearError();
    xreaderops ops={.Read=readEmpty,.Close=noisyClose};
    xreader* native=xrtReaderCreate(&ops,&closed);assert(native);
    assert(xrtMemDebugFailAfter(0));assert(!xlir_io_reader_take(native));
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(closed==3 && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();
    native=xrtReaderCreate(&ops,&closed);assert(native);void* reader=xlir_io_reader_take(native);assert(reader);
    assert(!xl6_xrt_io_reader_close(reader) && closed==4 && xl6_xrt_io_reader_closed(reader));xrtClearError();
    xl6_xrt_io_reader_destroy(reader);assert(closed==4 && !xrtGetError());
    puts("io first-failure including NULL; explicit close and exactly-once cleanup passed");
}
static void fileResources(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);size_t done=0;
    assert(xrtWriteFull(file->file,"a\0b",3,&done) && done==3);
    xlir_fs_map* mapped=xrtCalloc(1,sizeof(*mapped));assert(mapped && xlir_fs_file_ref(file));
    mapped->file=file;mapped->map=xrtFileMap(file->file,0,0,XFILE_MAP_READ);assert(mapped->map);
    xl6_xrt_fs_map_destroy(mapped);assert(!xrtGetError());
    xlir_fs_lock* locked=xrtCalloc(1,sizeof(*locked));assert(locked && xlir_fs_file_ref(file));
    assert(xrtFileLockRange(file->file,XFILE_LOCK_SHARED,0,0,false));locked->file=file;xrtAtomic32Store(&file->lock,1,XMEMORY_RELEASE);
    xl6_xrt_fs_lock_destroy(locked);assert(!xrtGetError() && !xrtAtomic32Load(&file->lock,XMEMORY_ACQUIRE));
    const char* separator=NULL;for(const char* at=name.Data;*at;++at)if(*at=='/' || *at=='\\')separator=at;
    assert(separator);char* directory=xrtStrDupN(name.Data,(size_t)(separator-name.Data));assert(directory);
    xlir_fs_root* rooted=xrtCalloc(1,sizeof(*rooted));assert(rooted);rooted->root=xrtRootOpen(directory);xrtFree(directory);assert(rooted->root);
    xl6_xrt_fs_root_destroy(rooted);assert(!xrtGetError());balance(&before);
    puts("io retained File map, lock and root native destruction balanced");
}
static bool forbiddenEdge(xrtownershipref ref,void* context)
{ (void)ref;(void)context;assert(0);return false; }
static bool admit(xrtownershipref ref,void* context)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    return xl6OwnershipCatalogResolveV1(ref,context,&adapter,&preparation,&finish);
}
static void provenance(void)
{
    /* Negative controls use real owned streams. Only their private provenance
     * is changed before publication; no fake native provider is substituted. */
    for(unsigned origin=0;origin<=2;origin+=2)for(unsigned which=0;which<3;++which){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        void* raw;const XL6_NativeHandlePolicyV1* policy;
        if(which==1){
            raw=xl6_xrt_io_writer_buffer();assert(raw);((xlir_io_writer*)raw)->origin=origin;
            policy=xl6IoWriterHandlePolicyV1Get();
        }else{
            void* reader=xl6_xrt_io_reader_bytes(fixtureBytes,-1);assert(reader);((xlir_io_reader*)reader)->origin=origin;
            if(which==2){raw=xl6_xrt_io_lines_create(reader,32);assert(raw);xl6_xrt_io_reader_destroy(reader);policy=xl6IoLinesHandlePolicyV1Get();}
            else {raw=reader;policy=xl6IoReaderHandlePolicyV1Get();}
        }
        xvalue* value=xl6NativeHandleTakeV1(&raw,policy);assert(value && !raw);
        assert(!policy->Trace(value,forbiddenEdge,NULL));
        const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainResourceIoReaderV1Get(),xl6OwnershipDomainResourceIoWriterV1Get(),xl6OwnershipDomainResourceIoLineV1Get()};
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);
        xrtownershipref reference=xrtValueOwnership(value);xrtownershipsnapshot* snapshot=NULL;xrtownershipscope scope={0};
        assert(xrtOwnershipFreezeTryBegin(&scope));
        assert(!xrtOwnershipSnapshotCreate(&reference,1,NULL,0,admit,catalog,&snapshot) && !snapshot);
        assert(xrtOwnershipScopeEnd(&scope));xl6OwnershipCatalogDestroyV1(catalog);xrtValueRelease(value);balance(&before);
    }
    puts("io unknown and process provenance refused: 6 controls, no hidden edge visited");
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));name=(xstrview){argv[1],strlen(argv[1])};
    /* Warm platform/thread error slots before measuring factory ownership. */
    xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    fixtureBytes=xrtValueBytes((xbytesview){(const unsigned char*)"a\0b",3});assert(fixtureBytes);
    buffer=xrtBufferFrom((xbytesview){(const unsigned char*)"a\0b",3});assert(buffer);
    file=xlir_fs_file_alloc();assert(file);file->file=xrtOpen(argv[1],XFILE_READ|XFILE_WRITE|XFILE_CREATE);assert(file->file);
    operations();firstFailure();fileResources();factories();provenance();
    xl6_xrt_fs_file_destroy(file);xrtBufferDestroy(buffer);xrtValueRelease(fixtureBytes);
    assert(!xrtGetError());puts("resident IO native acceptance passed");return 0;
}
