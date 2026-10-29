#include "src/runtime/xl6_log.h"
#include "src/runtime/xl6_io_handle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
typedef struct fault { size_t calls, fail, live; bool hit; } fault;
#if defined(XL6_LOG_FAULT_ALLOCATOR)
static void* allocate(void* context,size_t size)
{
    fault* f=context; if (++f->calls==f->fail) { f->hit=true; return NULL; }
    void* p=malloc(size); if (p) ++f->live; return p;
}
static void* resize(void* context,void* pointer,size_t size)
{
    fault* f=context; if (++f->calls==f->fail) { f->hit=true; return NULL; }
    if (!size) { if (pointer) --f->live; free(pointer); return NULL; }
    bool fresh=pointer==NULL; void* p=realloc(pointer,size); if (p && fresh) ++f->live; return p;
}
static void deallocate(void* context,void* pointer)
{ if (pointer) { fault* f=context; assert(f->live); --f->live; free(pointer); } }
static void allocation_contract(void)
{
    unsigned hits=0;
    static fault f;
    size_t baseline=0;
    char large[4096]; memset(large,'x',sizeof(large));
    xallocator allocator={&f,allocate,resize,deallocate};
    assert(xrtSetAllocator(&allocator));
    for (size_t budget=0;budget<40;++budget) {
        f.fail=budget ? f.calls+budget:SIZE_MAX; f.hit=false;
        void* record=xl6LogRecordCreateV1(XLOG_INFO,(xstrview){large,sizeof(large)},-1);
        xvalue* value=xrtValueString((xstrview){large,sizeof(large)});
        if (record && value) (void)xl6LogRecordFieldV1(record,xrtStrView("field"),value,-1);
        void* clone=record ? xl6LogRecordCloneV1(record):NULL;
        xbuffer* buffer=xrtBufferCreate();
        if (buffer && xrtBufferAppend(buffer,(xbytesview){(const uint8*)"prefix",6}) && record) {
            size_t before=xrtBufferView(buffer).Size;
            xrtClearError();
            int64_t written=xl6LogJsonBufferV1(record,buffer,8+64,0,0,0,16);
            if (written<0) assert(xrtBufferView(buffer).Size==before);
        }
        xl6LogRecordDestroyV1(clone); xl6LogRecordDestroyV1(record);
        xrtValueRelease(value); xrtBufferDestroy(buffer); xrtClearError();
        assert(xrtMemDebugReset());
        /* The process-wide small-block pool caches backing pages. Warm its
         * stable capacity once; reset proves zero logical allocations, and
         * the backing-page baseline must never grow across failed attempts. */
        if (!budget) baseline=f.live;
        assert(f.live==baseline); hits+=f.hit ? 1:0;
    }
    f.fail=SIZE_MAX;
    assert(hits>0);
    printf("allocator failures covered: %u; logical allocations balanced\n",hits);
}
#endif
typedef struct captured { size_t records; size_t fields; xstrview message; } captured;
static xlogresult capture(const xlogrecord* record,void* context)
{
    captured* state=context;
    ++state->records; state->fields=record->FieldCount;
    xrtFree((void*)state->message.Data);
    state->message=(xstrview){xrtStrDupN(record->Message.Data,record->Message.Size),record->Message.Size};
    return state->message.Data ? XLOG_RESULT_WRITTEN:XLOG_RESULT_ERROR;
}
static bool equal(xstrview a,const char* text)
{ return a.Size==strlen(text) && !memcmp(a.Data,text,a.Size); }
int main(void)
{
#if defined(XL6_LOG_FAULT_ALLOCATOR)
    allocation_contract();
#endif
    captured state={0}; xlogsinkconfig config={0};
    config.Name=xrtStrView("capture"); config.Level=XLOG_TRACE;
    config.Write=capture; config.UserData=&state;
    xlogsink* sink=xrtLogSinkCreate(&config);
    void* logger=xl6LogCreateV1(xrtStrView("native"),XLOG_TRACE);
    assert(sink && logger && xl6LogAttachV1(logger,sink));
    int64_t result=xl6LogPrintfV1(logger,XLOG_INFO,xrtStrView("%d %s %.2f %% %jd"),7,"你",1.25,(intmax_t)INT64_MAX);
    if (result!=XLOG_RESULT_WRITTEN) fprintf(stderr,"printf error: %s\n",xrtErrorMessage(xrtGetError()));
    assert(result==XLOG_RESULT_WRITTEN);
    assert(equal(state.message,"7 你 1.25 % 9223372036854775807"));
    void* record=xl6LogRecordCreateV1(XLOG_WARN,(xstrview){"a\0\xe4\xbd\xa0",5},99);
    assert(record);
    assert(xl6LogRecordMetadataV1(record,xrtStrView("app"),0,7));
    xstrview json=xl6LogJsonV1(record,8+64,0,0,0,16);
    assert(json.Data);
    assert(equal(json,"{\"message\":\"a\\u0000你\",\"fields\":{}}"));
    xrtFree((void*)json.Data);
    xl6LogRecordDestroyV1(record);
    xl6LogDestroyV1(logger); xrtLogSinkFree(sink);
    xrtFree((void*)state.message.Data);
    return 0;
}
