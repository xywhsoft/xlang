#include "services/semantic_color_result.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_color_result_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static xvalue* parse(const char* s){return xrtJsonParse(xrtStrView(s));}
static const char* provider="{\"legend\":{\"tokenTypes\":[\"function\",\"variable\",\"class\"],\"tokenModifiers\":[\"readonly\",\"declaration\",\"async\"]},\"full\":{\"delta\":true}}";
static int makeLegend(xide_color_legend* l)
{xvalue* v=parse(provider);int okay=v && xideColorLegendParse(v,l);xrtValueRelease(v);return okay;}
static xide_color_result* decode(const xls_text* t,const xide_color_legend* l,const char* json,const xide_color_result* base,enum xide_color_error* e)
{xvalue* v=parse(json);if(!v)return NULL;xide_color_result* r=xideColorDecode(t,l,v,base,0,NULL,NULL,e);xrtValueRelease(v);return r;}
static int reject(const xls_text* t,const xide_color_legend* l,const char* json,const xide_color_result* base,enum xide_color_error expected)
{enum xide_color_error e=XIDE_COLOR_OK;xide_color_result* r=decode(t,l,json,base,&e);CHECK(!r && e==expected);return 1;}
static int legends(void)
{
    xide_color_legend l;CHECK(makeLegend(&l));CHECK(l.delta && l.typeCount==3 && l.modifierCount==3 && l.types[0]==XIDE_COLOR_FUNCTION && l.modifiers[0]==XIDE_COLOR_READONLY);
    const char* bad[]={"null","{}","{\"full\":false}","{\"full\":1}",
      "{\"full\":true,\"legend\":{\"tokenTypes\":[],\"tokenModifiers\":[]}}",
      "{\"full\":true,\"legend\":{\"tokenTypes\":[\"function\",\"function\"],\"tokenModifiers\":[]}}",
      "{\"full\":true,\"legend\":{\"tokenTypes\":[\"function\"],\"tokenModifiers\":[\"async\",\"async\"]}}",
      "{\"full\":true,\"legend\":{\"tokenTypes\":[\"unknown\"],\"tokenModifiers\":[]}}",
      "{\"full\":true,\"legend\":{\"tokenTypes\":[\"class\"],\"tokenModifiers\":[\"static\"]}}",
      "{\"full\":{\"delta\":null},\"legend\":{\"tokenTypes\":[\"function\"],\"tokenModifiers\":[]}}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){xvalue* v=parse(bad[i]);CHECK(v);memset(&l,0xff,sizeof(l));CHECK(!xideColorLegendParse(v,&l));xide_color_legend zero={0};CHECK(!memcmp(&l,&zero,sizeof(l)));xrtValueRelease(v);}
    xvalue* v=parse("{\"full\":true,\"legend\":{\"tokenTypes\":[\"class\"],\"tokenModifiers\":[]}}");CHECK(xideColorLegendParse(v,&l) && !l.delta && l.typeCount==1);xrtValueRelease(v);
    CHECK(!xideColorLegendParse(NULL,NULL));return 1;
}
static int ranges(void)
{
    const char* source="\xef\xbb\xbf" "A😀中\r\nend\rlast\n";
    xls_text* t=xlsTextCreate(source,strlen(source),4096,NULL);CHECK(t);xide_color_legend l;CHECK(makeLegend(&l));enum xide_color_error e;
    xide_color_result* r=decode(t,&l,"{\"resultId\":\"α😀\",\"data\":[0,2,2,0,6,0,2,1,1,1,1,0,3,2,0,1,0,4,0,0]}",NULL,&e);
    CHECK(r && e==XIDE_COLOR_OK && xideColorResultCount(r)==4 && xideColorResultWordCount(r)==20);
    const xide_color_range* rows=xideColorResultRanges(r);
    CHECK(rows[0].start==4 && rows[0].end==8 && rows[0].role==XIDE_COLOR_FUNCTION && rows[0].modifiers==(XIDE_COLOR_DECLARATION|XIDE_COLOR_ASYNC));
    CHECK(rows[1].start==8 && rows[1].end==11 && rows[1].role==XIDE_COLOR_VARIABLE && rows[1].modifiers==XIDE_COLOR_READONLY);
    CHECK(rows[2].start==13 && rows[2].end==16 && rows[2].role==XIDE_COLOR_CLASS && rows[3].start==17 && rows[3].end==21);
    CHECK(!strcmp(xideColorResultId(r),"α😀") && xideColorResultScannedBytes(r)==21);
    CHECK(xideColorResultFind(r,4,8)==rows && !xideColorResultFind(r,4,7) && !xideColorResultFind(r,5,8) && !xideColorResultFind(r,-1,8) && !xideColorResultFind(r,8,8));
    CHECK(xideColorResultBytes(r)<=xideColorResultPeakBytes(r));
    xide_color_result* retained=xideColorResultRetain(r);CHECK(retained==r);xideColorResultRelease(r);CHECK(xideColorResultCount(retained)==4);xideColorResultRelease(retained);
    const char* bad[]={"{}","[]","1","{\"data\":null}","{\"data\":[0]}","{\"data\":[],\"edits\":[]}",
      "{\"data\":[0,0,1,3,0]}","{\"data\":[0,0,1,0,8]}","{\"data\":[0,0,1,0,-1]}","{\"data\":[0,0,1,0,1.0]}",
      "{\"data\":[0,0,1,0,true]}","{\"data\":[0,0,1,0,2147483648]}","{\"resultId\":null,\"data\":[]}",
      "{\"resultId\":\"\",\"data\":[]}","{\"resultId\":\"a\\u0000b\",\"data\":[]}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i)CHECK(reject(t,&l,bad[i],NULL,XIDE_COLOR_WIRE));
    const char* coords[]={"{\"data\":[0,3,1,0,0]}","{\"data\":[0,2,1,0,0]}","{\"data\":[0,5,1,0,0]}",
      "{\"data\":[0,1,0,0,0]}","{\"data\":[0,1,2147483647,0,0]}","{\"data\":[0,1,4,0,0,0,1,1,0,0]}",
      "{\"data\":[2147483647,0,1,0,0,1,0,1,0,0]}","{\"data\":[0,2147483647,1,0,0]}","{\"data\":[3,0,1,0,0]}"};
    for(size_t i=0;i<sizeof(coords)/sizeof(*coords);++i)CHECK(reject(t,&l,coords[i],NULL,XIDE_COLOR_COORDINATE));
    for(int i=0;i<2;++i){r=decode(t,&l,i?"null":"{\"data\":[]}",NULL,&e);CHECK(r && !e && !xideColorResultCount(r) && !xideColorResultId(r));xideColorResultRelease(r);}
    CHECK(!xideColorResultCount(NULL) && !xideColorResultRanges(NULL) && !xideColorResultWords(NULL) && !xideColorResultBytes(NULL) && !xideColorResultPeakBytes(NULL) && !xideColorResultFind(NULL,0,1));
    xlsTextRelease(t);return 1;
}
static int deltas(void)
{
    xls_text* t=xlsTextCreate("alpha beta\n",11,4096,NULL);CHECK(t);xide_color_legend l;CHECK(makeLegend(&l));enum xide_color_error e;
    xide_color_result* base=decode(t,&l,"{\"resultId\":\"one\",\"data\":[0,0,5,0,0,0,6,4,1,1]}",NULL,&e);CHECK(base && !e);
    uint32_t original[10];memcpy(original,xideColorResultWords(base),sizeof(original));
    /* Deliberately reversed, partial token edits; all offsets address old data. */
    xide_color_result* next=decode(t,&l,"{\"resultId\":\"two\",\"edits\":[{\"start\":9,\"deleteCount\":1,\"data\":[2]},{\"start\":3,\"deleteCount\":1,\"data\":[2]}]}",base,&e);
    CHECK(next && !e && xideColorResultCount(next)==2 && !strcmp(xideColorResultId(next),"two"));
    CHECK(xideColorResultRanges(next)[0].role==XIDE_COLOR_CLASS && xideColorResultRanges(next)[1].modifiers==XIDE_COLOR_DECLARATION);
    CHECK(!memcmp(original,xideColorResultWords(base),sizeof(original)));xideColorResultRelease(next);
    next=decode(t,&l,"{\"edits\":[]}",base,&e);CHECK(next && !e && !xideColorResultId(next) && !memcmp(original,xideColorResultWords(next),sizeof(original)));xideColorResultRelease(next);
    /* A delta request may legally receive a complete replacement. */
    next=decode(t,&l,"{\"data\":[0,6,4,0,0]}",base,&e);CHECK(next && !e && xideColorResultRanges(next)[0].start==6);xideColorResultRelease(next);
    next=decode(t,&l,"{\"edits\":[{\"start\":0,\"deleteCount\":10}]}",base,&e);CHECK(next && !e && !xideColorResultCount(next));xideColorResultRelease(next);
    /* Ordered inserts at the same offset are allowed before the replacement. */
    next=decode(t,&l,"{\"edits\":[{\"start\":0,\"deleteCount\":0,\"data\":[0,0]},{\"start\":0,\"deleteCount\":0,\"data\":[5]},{\"start\":0,\"deleteCount\":10,\"data\":[2,0]}]}",base,&e);
    CHECK(next && !e && xideColorResultCount(next)==1 && xideColorResultRanges(next)[0].role==XIDE_COLOR_CLASS);xideColorResultRelease(next);
    const char* bad[]={"{\"edits\":null}","{\"edits\":[{}]}","{\"edits\":[{\"start\":11,\"deleteCount\":0}]}",
      "{\"edits\":[{\"start\":9,\"deleteCount\":2}]}","{\"edits\":[{\"start\":-1,\"deleteCount\":0}]}",
      "{\"edits\":[{\"start\":0,\"deleteCount\":1.0}]}","{\"edits\":[{\"start\":0,\"deleteCount\":0,\"data\":null}]}",
      "{\"edits\":[{\"start\":0,\"deleteCount\":1}]}",
      "{\"edits\":[{\"start\":0,\"deleteCount\":5},{\"start\":4,\"deleteCount\":1}]}",
      "{\"edits\":[{\"start\":0,\"deleteCount\":10},{\"start\":0,\"deleteCount\":0,\"data\":[0,0,5,0,0]}]}",
      "{\"edits\":[{\"start\":0,\"deleteCount\":1,\"data\":[null]}]}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){CHECK(reject(t,&l,bad[i],base,XIDE_COLOR_WIRE));CHECK(!memcmp(original,xideColorResultWords(base),sizeof(original)));}
    CHECK(reject(t,&l,"{\"edits\":[]}",NULL,XIDE_COLOR_BASE));
    xide_color_legend changed=l;changed.types[0]=XIDE_COLOR_METHOD;CHECK(reject(t,&changed,"{\"edits\":[]}",base,XIDE_COLOR_BASE));
    changed=l;changed.delta=0;CHECK(reject(t,&changed,"{\"edits\":[]}",base,XIDE_COLOR_BASE));
    next=decode(t,&l,"{\"data\":[]}",NULL,&e);CHECK(next && reject(t,&l,"{\"edits\":[]}",next,XIDE_COLOR_BASE));xideColorResultRelease(next);
    xideColorResultRelease(base);xlsTextRelease(t);return 1;
}
typedef struct gate {xatomic32 entered,hold,wakes;} gate;
static void gatedWake(void* user)
{
    gate* g=user;xrtAtomic32FetchAdd(&g->wakes,1,XMEMORY_RELAXED);xrtAtomic32Store(&g->entered,1,XMEMORY_RELEASE);
    for(int i=0;i<5000 && xrtAtomic32Load(&g->hold,XMEMORY_ACQUIRE);++i)xrtSleep(1);
}
typedef struct drop_state {xatomic32 calls,thread;} drop_state;
static void drop(ptr handle,ptr user)
{drop_state* s=user;if(handle==s){xrtAtomic32Store(&s->thread,(int32)GetCurrentThreadId(),XMEMORY_RELAXED);xrtAtomic32FetchAdd(&s->calls,1,XMEMORY_RELEASE);}}
static xvalue* disposable(drop_state* s)
{static const xvaluehandleops ops={.Drop=drop};ptr owned=s;return xrtValueHandleTake(&owned,&ops,s);}
static int waitFlag(xatomic32* flag){for(int i=0;i<5000;++i){if(xrtAtomic32Load(flag,XMEMORY_ACQUIRE))return 1;xrtSleep(1);}return 0;}
static int waitAnswer(xide_color_decoder* d,xide_color_completion* out)
{for(int i=0;i<5000;++i){if(xideColorDecoderTake(d,out))return 1;xrtSleep(1);}return 0;}
static int worker(void)
{
    gate g={0};xide_color_decoder* d=xideColorDecoderCreate(gatedWake,&g);CHECK(d);xide_color_legend l;CHECK(makeLegend(&l));
    xls_text* text=xlsTextCreate("alpha",5,4096,NULL);CHECK(text);xvalue* m=parse("{\"id\":\"colors:1\",\"result\":{\"resultId\":\"one\",\"data\":[0,0,5,0,0]}}");CHECK(m);
    xrtAtomic32Store(&g.hold,1,XMEMORY_RELEASE);CHECK(xideColorDecoderSubmit(d,1,m,xlsTextRetain(text),&l,NULL,0));CHECK(waitFlag(&g.entered));
    /* Completed result is invalidated without disposal or a join on this owner. */
    xideColorDecoderCancel(d);xide_color_completion out={0};CHECK(!xideColorDecoderTake(d,&out));
    drop_state discarded={0},rejected={0};xvalue* trash=disposable(&discarded);CHECK(trash && xideColorDecoderDiscard(d,trash));
    xvalue* overflow=disposable(&rejected);CHECK(overflow && !xideColorDecoderDiscard(d,overflow));CHECK(!xrtAtomic32Load(&rejected.calls,XMEMORY_ACQUIRE));xrtValueRelease(overflow);CHECK(xrtAtomic32Load(&rejected.calls,XMEMORY_ACQUIRE)==1);
    m=parse("{\"result\":null}");CHECK(m);CHECK(!xideColorDecoderSubmit(d,2,m,text,&l,NULL,0));xrtValueRelease(m);
    CHECK(!xrtAtomic32Load(&discarded.calls,XMEMORY_ACQUIRE));xrtAtomic32Store(&g.hold,0,XMEMORY_RELEASE);CHECK(waitFlag(&discarded.calls));
    CHECK((DWORD)xrtAtomic32Load(&discarded.thread,XMEMORY_RELAXED)!=GetCurrentThreadId());
    for(int i=0;i<5000;++i){m=parse("{\"result\":{\"resultId\":\"new\",\"data\":[0,0,5,0,0]}}");xls_text* owned=xlsTextRetain(text);if(xideColorDecoderSubmit(d,2,m,owned,&l,NULL,0))break;xrtValueRelease(m);xlsTextRelease(owned);m=NULL;xrtSleep(1);}CHECK(m);
    CHECK(waitAnswer(d,&out) && out.token==2 && !out.error && out.result && xideColorResultCount(out.result)==1);
    xide_color_result* base=out.result;
    m=parse("{\"result\":{\"edits\":[]}}");CHECK(m && xideColorDecoderSubmit(d,3,m,xlsTextRetain(text),&l,xideColorResultRetain(base),0));
    CHECK(waitAnswer(d,&out) && out.token==3 && !out.error && out.result && !xideColorResultId(out.result));
    CHECK(xideColorDecoderRetire(d,out.result));
    m=parse("{\"result\":null}");CHECK(m && !xideColorDecoderSubmit(d,3,m,text,&l,NULL,0));xrtValueRelease(m);
    m=parse("{\"error\":{\"code\":-32801}}");CHECK(m && xideColorDecoderSubmit(d,4,m,xlsTextRetain(text),&l,NULL,0));CHECK(waitAnswer(d,&out) && out.token==4 && out.error==XIDE_COLOR_REMOTE && !out.result);
    m=parse("{\"result\":{\"data\":[0]}}");CHECK(m && xideColorDecoderSubmit(d,5,m,xlsTextRetain(text),&l,NULL,0));CHECK(waitAnswer(d,&out) && out.error==XIDE_COLOR_WIRE && !out.result);
    xideColorDecoderCancel(d);CHECK(!xideColorDecoderTake(d,&out));xideColorDecoderDestroy(d);xideColorResultRelease(base);xlsTextRelease(text);
    CHECK(!xideColorDecoderSubmit(NULL,1,NULL,NULL,&l,NULL,0) && !xideColorDecoderDiscard(NULL,NULL) && !xideColorDecoderRetire(NULL,NULL) && !xideColorDecoderTake(NULL,&out));xideColorDecoderCancel(NULL);xideColorDecoderDestroy(NULL);return 1;
}
static int protocol(const char* path)
{
    FILE* f=fopen(path,"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));long size=ftell(f);CHECK(size>0 && size<=16*1024*1024);rewind(f);
    char* bytes=xrtCalloc((size_t)size+1,1);CHECK(bytes);CHECK(fread(bytes,1,(size_t)size,f)==(size_t)size);CHECK(!fclose(f));
    xvalue* batch=xrtJsonParse((xstrview){bytes,(size_t)size});xrtFree(bytes);CHECK(batch);
    xide_color_legend legend;CHECK(xideColorLegendParse(xrtValueObjectGet(batch,xrtStrView("provider")),&legend));
    const xvalue* cases=xrtValueObjectGet(batch,xrtStrView("cases"));CHECK(cases && xrtValueType(cases)==XVALUE_ARRAY && xrtValueCount(cases)>0 && xrtValueCount(cases)<=32);
    xide_color_result* base=NULL;
    for(size_t i=0;i<xrtValueCount(cases);++i){
        const xvalue* item=xrtValueArrayGet(cases,i);xstrview source={0};CHECK(xrtValueGetString(xrtValueObjectGet(item,xrtStrView("text")),&source) && !memchr(source.Data,0,source.Size));
        xls_text* text=xlsTextCreate(source.Data,source.Size,8u*1024u*1024u,NULL);CHECK(text);enum xide_color_error e;
        xide_color_result* r=xideColorDecode(text,&legend,xrtValueObjectGet(item,xrtStrView("result")),base,0,NULL,NULL,&e);CHECK(r && !e);
        const xvalue* expected=xrtValueObjectGet(item,xrtStrView("expected"));CHECK(expected && xrtValueType(expected)==XVALUE_ARRAY && xrtValueCount(expected)==xideColorResultCount(r));
        const xide_color_range* ranges=xideColorResultRanges(r);
        for(size_t j=0;j<xideColorResultCount(r);++j){
            const xvalue* row=xrtValueArrayGet(expected,j);CHECK(row && xrtValueCount(row)==4);int64 fields[4]={0};
            for(size_t k=0;k<4;++k)CHECK(xrtValueGetInt(xrtValueArrayGet(row,k),fields+k));
            CHECK(ranges[j].start==fields[0] && ranges[j].end==fields[1] && ranges[j].role==(unsigned)fields[2] && ranges[j].modifiers==(unsigned)fields[3]);
        }
        const xvalue* raw=xrtValueObjectGet(item,xrtStrView("words"));CHECK(raw && xrtValueCount(raw)==xideColorResultWordCount(r));
        for(size_t j=0;j<xideColorResultWordCount(r);++j){int64 n=0;CHECK(xrtValueGetInt(xrtValueArrayGet(raw,j),&n) && n==xideColorResultWords(r)[j]);}
        xideColorResultRelease(base);base=r;xlsTextRelease(text);
    }
    xideColorResultRelease(base);xrtValueRelease(batch);return 1;
}
int main(int argc,char** argv)
{
    if(argc==3 && !strcmp(argv[1],"--protocol")){
        if(!protocol(argv[2]))return 1;
        printf("semantic_color_result_test native protocol passed: %u checks.\n",checks);return 0;
    }
    if(!legends() || !ranges() || !deltas() || !worker())return 1;
    printf("semantic_color_result_test passed: %u checks; strict legends, full/unsorted integer delta, UTF16/BOM/CRLF, exact span lookup, bounded background decode and retirement.\n",checks);return 0;
}
