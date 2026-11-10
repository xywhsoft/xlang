#include "services/semantic_color_result.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Actual allocation failures in the production decoder, not a model. */
static size_t allocations,failAt,live;static void* held[32];static int badFree;
static void* allocate(size_t count,size_t size)
{
    if(++allocations==failAt)return NULL;
    void* p=xrtCalloc(count,size);if(!p)return NULL;
    for(size_t i=0;i<32;++i)if(!held[i]){held[i]=p;++live;return p;}
    badFree=1;return p;
}
static void release(void* p)
{
    if(p){size_t i=0;for(;i<32;++i)if(held[i]==p){held[i]=NULL;--live;break;}if(i==32)badFree=1;}
    xrtFree(p);
}
#define xrtCalloc allocate
#define xrtFree release
#include "services/semantic_color_result.c"
#undef xrtCalloc
#undef xrtFree

static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_color_fault_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static xvalue* parse(const char* s){return xrtJsonParse(xrtStrView(s));}
typedef struct cancellation {size_t calls,at;} cancellation;
static int stopAt(void* user){cancellation* c=user;return ++c->calls==c->at;}
static int verify(void)
{
    xide_color_legend legend={.typeCount=1,.modifierCount=0,.types={XIDE_COLOR_VARIABLE},.delta=1};
    xls_text* text=xlsTextCreate("alpha beta\n",11,4096,NULL);CHECK(text);
    xvalue *full=parse("{\"resultId\":\"base\",\"data\":[0,0,5,0,0,0,6,4,0,0]}"),
        *delta=parse("{\"resultId\":\"delta\",\"edits\":[{\"start\":6,\"deleteCount\":1,\"data\":[6]}]}");CHECK(full && delta);
    enum xide_color_error error;xide_color_result* base=xideColorDecode(text,&legend,full,NULL,0,NULL,NULL,&error);CHECK(base && !error && live==4);
    uint32_t words[10];memcpy(words,base->words,sizeof(words));size_t retained=live;
    for(int kind=0;kind<2;++kind){
        const xvalue* value=kind?delta:full;size_t points=0;
        for(size_t failure=0;failure<=points;++failure){
            allocations=0;failAt=failure;
            xide_color_result* r=xideColorDecode(text,&legend,value,kind?base:NULL,0,NULL,NULL,&error);
            if(!failure){points=allocations;CHECK(r && !error && points==(kind?5u:4u));}
            else CHECK(!r && error==XIDE_COLOR_MEMORY);
            xideColorResultRelease(r);CHECK(live==retained && !badFree && !memcmp(words,base->words,sizeof(words)));
        }
        failAt=allocations=0;
        cancellation c={0};xide_color_result* r=xideColorDecode(text,&legend,value,kind?base:NULL,0,stopAt,&c,&error);CHECK(r && !error);size_t polls=c.calls;xideColorResultRelease(r);CHECK(polls>=4 && live==retained);
        for(size_t at=1;at<=polls;++at){c=(cancellation){0,at};r=xideColorDecode(text,&legend,value,kind?base:NULL,0,stopAt,&c,&error);CHECK(!r && error==XIDE_COLOR_CANCELLED && live==retained && !badFree);}
        for(size_t budget=1;budget<1024;++budget){
            r=xideColorDecode(text,&legend,value,kind?base:NULL,budget,NULL,NULL,&error);
            if(r)CHECK(!error && r->peakBytes<=budget && r->bytes<=r->peakBytes);
            else CHECK(error==XIDE_COLOR_LIMIT);
            xideColorResultRelease(r);CHECK(live==retained && !badFree);
        }
    }
    CHECK(!xideColorDecode(text,&legend,full,NULL,COLOR_BUDGET+1u,NULL,NULL,&error) && error==XIDE_COLOR_LIMIT);
    xide_color_legend bad=legend;bad.types[0]=XIDE_COLOR_ROLE_COUNT;CHECK(!xideColorDecode(text,&bad,full,NULL,0,NULL,NULL,&error) && error==XIDE_COLOR_ARGUMENT);
    bad=legend;bad.modifierCount=2;bad.modifiers[0]=bad.modifiers[1]=1;CHECK(!xideColorDecode(text,&bad,full,NULL,0,NULL,NULL,&error) && error==XIDE_COLOR_ARGUMENT);
    CHECK(!xideColorDecode(NULL,&legend,full,NULL,0,NULL,NULL,&error) && error==XIDE_COLOR_ARGUMENT);
    xideColorResultRelease(base);CHECK(!live && !badFree);xrtValueRelease(full);xrtValueRelease(delta);xlsTextRelease(text);

    /* 65,536 tokens, with immutable ranges and O(log N) exact-span lookup. */
    const size_t count=65536;char* code=xrtCalloc(count*2+1,1);CHECK(code);for(size_t i=0;i<count;++i){code[i*2]='x';code[i*2+1]=' ';}
    text=xlsTextCreate(code,count*2,count*2+1,NULL);CHECK(text);full=parse("{\"data\":[]}");CHECK(full);xvalue* data=xrtValueObjectGet(full,xrtStrView("data"));
    for(size_t i=0;i<count;++i){uint32_t row[5]={0,i?2:0,1,0,0};for(size_t j=0;j<5;++j)CHECK(xrtValueArrayAppendNew(data,xrtValueInt(row[j])));}
    base=xideColorDecode(text,&legend,full,NULL,0,NULL,NULL,&error);CHECK(base && !error && base->count==count && base->scanned==count*2-1 && base->bytes==base->peakBytes);
    for(size_t i=0;i<count;i+=127)CHECK(xideColorResultFind(base,(int)i*2,(int)i*2+1)==base->ranges+i);
    for(size_t j=0;j<5;++j)CHECK(xrtValueArrayAppendNew(data,xrtValueInt(0)));
    CHECK(!xideColorDecode(text,&legend,full,NULL,0,NULL,NULL,&error) && error==XIDE_COLOR_LIMIT);
    xideColorResultRelease(base);xrtValueRelease(full);xlsTextRelease(text);xrtFree(code);CHECK(!live && !badFree);

    /* Unaligned four-byte UTF-8 would never hit an absolute-offset %4096 poll. */
    size_t n=32768;code=xrtCalloc(n*4+5,1);CHECK(code);memcpy(code,"abc",3);for(size_t i=0;i<n;++i)memcpy(code+3+i*4,"😀",4);code[3+n*4]='x';
    text=xlsTextCreate(code,n*4+4,n*4+5,NULL);CHECK(text);
    color_cursor cursor={(const unsigned char*)xlsTextData(text),xlsTextSize(text),0,0,0,0};cancellation stop={0,2};
    CHECK(advance(&cursor,0,(uint32_t)(3+n*2),stopAt,&stop)==XIDE_COLOR_CANCELLED && cursor.at>4096 && cursor.at<16384 && stop.calls==2);
    char json[128];snprintf(json,sizeof(json),"{\"data\":[0,%u,1,0,0]}",(unsigned)(3+n*2));full=parse(json);CHECK(full);
    base=xideColorDecode(text,&legend,full,NULL,0,NULL,NULL,&error);CHECK(base && !error && base->ranges[0].start==(int)(3+n*4) && base->scanned==n*4+4);
    stop=(cancellation){0};xide_color_result* r=xideColorDecode(text,&legend,full,NULL,0,stopAt,&stop,&error);CHECK(r && !error && stop.calls>30);size_t polls=stop.calls;xideColorResultRelease(r);
    for(size_t at=1;at<=polls;++at){stop=(cancellation){0,at};r=xideColorDecode(text,&legend,full,NULL,0,stopAt,&stop,&error);CHECK(!r && error==XIDE_COLOR_CANCELLED && live==3 && !badFree);}
    xideColorResultRelease(base);xrtValueRelease(full);xlsTextRelease(text);xrtFree(code);CHECK(!live && !badFree);
    return 1;
}
int main(void)
{if(!verify())return 1;printf("semantic_color_fault_test passed: %u checks; real allocation/cancel failures, budgets, 65536 tokens and unaligned Unicode scan.\n",checks);return 0;}
