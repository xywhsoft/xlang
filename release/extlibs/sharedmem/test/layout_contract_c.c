/* 固定小端协议的独立字节表；不拿被测整数写入器生成预期。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#define XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../sharedmem_native.c"

static const size_t sizes[]={1,2,7,8,9,15,16,31,32,63,64,65,127,128};
static const int64_t values[]={0,1,-1,INT64_MIN,INT64_MAX,INT64_C(0x0102030405060708),-INT64_C(0x0102030405060708)};
static const unsigned char encodings[][8]={
    {0,0,0,0,0,0,0,0},{1,0,0,0,0,0,0,0},{255,255,255,255,255,255,255,255},
    {0,0,0,0,0,0,0,128},{255,255,255,255,255,255,255,127},
    {8,7,6,5,4,3,2,1},{248,248,249,250,251,252,253,254}
};
static unsigned checks, failures, records, invalids, snapshots;
static bool emit;
static void require(bool ok,const char* label)
{
    ++checks;
    if (!ok) { if(failures<12)fprintf(stderr,"sharedmem layout: %s\n",label); ++failures; }
}
static bool memory_equal(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    return before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->OverflowCount==after.OverflowCount && before->UnderflowCount==after.UnderflowCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount;
}
static void one(const char* name,size_t size)
{
    xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
    void* owner=xshm_create(name,(int64_t)size);
    void* peer=owner ? xshm_open(name) : NULL;
    if (!owner || !peer) { require(false,"create real owner and peer"); xshm_close(peer); xshm_close(owner); return; }
    require(xshm_adopt(owner)==owner && xshm_size(peer)==(int64_t)size && strcmp(xshm_name_of(peer),name)==0,"handle identity and metadata");
    unsigned char initial[128], expected[128];
    for(size_t i=0;i<size;++i)initial[i]=(unsigned char)(i*29+size);
    for(size_t offset=0;offset+8<=size;++offset) for(unsigned v=0;v<sizeof(values)/sizeof(values[0]);++v) {
        require(xshm_write(owner,0,initial,size),"reset binary frame");
        memcpy(expected,initial,size); memcpy(expected+offset,encodings[v],8);
        require(xshm_write_int64(owner,(int64_t)offset,values[v]),"integer write including unaligned offsets");
        struct { uint64_t left; int64_t value; uint64_t right; } read={UINT64_MAX,0,UINT64_MAX};
        require(xshm_read_int64(peer,(int64_t)offset,&read.value) && read.value==values[v] &&
            read.left==UINT64_MAX && read.right==UINT64_MAX && xshm_read_int64_value(peer,(int64_t)offset)==values[v],"integer read and destination guards");
        xbuffer* wire=xshm_read(peer,0,size);
        require(wire && xrtBufferView(wire).Size==size && memcmp(xrtBufferView(wire).Data,expected,size)==0,"entire frame agrees with independent little-endian table");
        if(emit && wire) {
            printf("record\t%zu\t%zu\t%lld\t",size,offset,(long long)values[v]);
            for(size_t i=0;i<size;++i)printf("%02x",((const unsigned char*)xrtBufferView(wire).Data)[i]);
            putchar('\n');
        }
        xrtBufferDestroy(wire); ++records;
    }
    require(xshm_write_abi(owner,0,initial,size),"mutable ABI input matches const C API");
    static const int64_t offsets[]={-1,INT64_MIN,INT64_MAX};
    for(unsigned i=0;i<8;++i) {
        int64_t offset=i<3 ? offsets[i] : i==3 ? (int64_t)size+1 : i==4 ? (int64_t)size : 0;
        size_t length=i<4 ? 0 : i==4 ? 1 : SIZE_MAX-(i-5);
        require(!xshm_write(owner,offset,initial,length) && xrtErrorKind(xrtGetError())==XERR_RANGE,"invalid write range rejected before memory access");
        require(xshm_read(owner,offset,length)==NULL && xrtErrorKind(xrtGetError())==XERR_RANGE,"invalid buffer range rejected");
        require(xshm_read_text(owner,offset,length)==NULL && xrtErrorKind(xrtGetError())==XERR_RANGE,"invalid text range rejected");
        xbuffer* unchanged=xshm_read(peer,0,size);
        require(unchanged && memcmp(xrtBufferView(unchanged).Data,initial,size)==0,"failed writes do not alter mapped frame");
        xrtBufferDestroy(unchanged); ++invalids;
    }
    int64_t destination=INT64_C(123456789);
    require(!xshm_read_int64(owner,(int64_t)size,&destination) && destination==INT64_C(123456789),"failed integer read leaves destination untouched");
    require(xshm_write(owner,(int64_t)size,NULL,0),"zero-length write at end");
    xbuffer* empty=xshm_read(peer,(int64_t)size,0);
    char* emptyText=xshm_read_text(peer,(int64_t)size,0);
    require(empty && xrtBufferView(empty).Size==0 && emptyText && emptyText[0]==0,"empty owned snapshots at end");
    xrtBufferDestroy(empty); xrtFree(emptyText);

    const unsigned char unicode[]={'A',0,0xe4,0xbd,0xa0,0xe5,0xa5,0xbd};
    const unsigned char ascii[]={'A',0,'B','C','D','E','F'};
    const void* text=size>=8 ? unicode : ascii;
    size_t length=size>=8 ? 8 : size;
    require(xshm_write_text(owner,0,(xstrview){text,length}),"write valid counted UTF-8 including NUL");
    char* saved=xshm_read_text(peer,0,length);
    xbuffer* binary=xshm_read(peer,0,length);
    require(saved && binary && memcmp(saved,text,length)==0 && saved[length]==0,"saved text has explicit body size and extra terminator");
    require(xshm_zero(owner),"zero through owner");
    xshm_close(owner);
    int64_t zero=7;
    if(size>=8)require(xshm_read_int64(peer,0,&zero) && zero==0,"peer remains usable after owner retirement");
    void* reopened=xshm_open(name);
#ifdef _WIN32
    require(reopened!=NULL,"Windows name survives while peer handle exists");
#else
    require(reopened==NULL && xrtErrorKind(xrtGetError())==XERR_NOT_FOUND,"POSIX owner retirement unlinks name, not existing peer mapping");
#endif
    xshm_close(reopened); xshm_close(peer);
    require(saved && binary && memcmp(saved,text,length)==0 && memcmp(xrtBufferView(binary).Data,text,length)==0,"owned snapshots survive writes and all handle retirement");
    xrtFree(saved); xrtBufferDestroy(binary); ++snapshots;
    require(xshm_open(name)==NULL && xrtErrorKind(xrtGetError())==XERR_NOT_FOUND,"last retirement leaves no reusable old mapping name");
    require(memory_equal(&before),"size case seven memory metrics balanced");
}
static void maximum(const char* name)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    const int64_t size=INT64_C(64)*1024*1024;
    void* owner=xshm_create(name,size);
    void* peer=owner ? xshm_open(name) : NULL;
    require(owner && peer && xshm_size(peer)==size,"64 MiB public upper bound opens successfully");
    if(owner && peer) {
        int64_t value=9;
        require(xshm_read_int64(peer,0,&value) && value==0 && xshm_read_int64(peer,size-8,&value) && value==0,"new mapping first and last pages are zero");
        require(xshm_write_int64(owner,size-8,INT64_MIN) && xshm_read_int64(peer,size-8,&value) && value==INT64_MIN,"last complete integer at maximum size");
        require(!xshm_write_int64(owner,size-7,1) && xrtErrorKind(xrtGetError())==XERR_RANGE,"maximum-size tail overflow rejected");
    }
    xshm_close(peer);xshm_close(owner);
    require(memory_equal(&before),"maximum-size owners retired without touching entire mapping");
}
int main(int argc,char** argv)
{
    if(!xrtMemDebugEnabled())return 2;
    emit=argc==2 && strcmp(argv[1],"--records")==0;
    if(argc!=1 && !emit)return 2;
    uint64_t nonce;
    if(!xrtSecureRandom(&nonce,sizeof(nonce)))return 2;
    char name[96];snprintf(name,sizeof(name),"xl-shm-layout-%016llx",(unsigned long long)nonce);
    xrtSetErrorInfo(XERR_STATE,"test.sharedmem",0,"warm");xrtClearError();
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i)one(name,sizes[i]);
    maximum(name);
    require(snapshots==14 && invalids==112 && records>0,"all sizes and invalid boundaries executed");
    require(memory_equal(&before),"all mapping and snapshot owners retired");
    printf("sharedmem layout: %u records, %u invalid ranges, %u snapshot lifetimes, %u checks, %u failures\n",records,invalids,snapshots,checks,failures);
    return failures ? 1 : 0;
}
