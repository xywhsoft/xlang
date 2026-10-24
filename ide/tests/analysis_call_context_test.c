#include "analysis/call_context.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_call_context_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static xls_call_context* build(const char* source)
{xls_text* text=xlsTextCreate(source,strlen(source),8u*1024u*1024u,NULL);xls_call_context* c=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);xlsTextRelease(text);return c;}
static int one(const char* marked,const char* callee,size_t argument,int closed,int known,int comment,int literal,int embedded,const char* named)
{
 const char* marker=strstr(marked,"@caret@");CHECK(marker);xstrbuf text={0};CHECK(xrtStrBufAppend(&text,(xstrview){marked,(size_t)(marker-marked)}) && xrtStrBufAppend(&text,(xstrview){marker+7,strlen(marker+7)}));xls_call_context* c=build(text.Data);CHECK(c);size_t offset=(size_t)(marker-marked),bom=text.Size>=3 && !memcmp(text.Data,"\xef\xbb\xbf",3) ? 3 : 0;xls_call_site site;int result=xlsCallContextLookup(c,offset-bom,&site);
 if(!callee)CHECK(!result);else{
  CHECK(result==1 && site.calleeEnd>=site.calleeStart && site.calleeEnd<=text.Size-bom);CHECK(site.calleeEnd-site.calleeStart==strlen(callee) && !memcmp(text.Data+bom+site.calleeStart,callee,strlen(callee)));
  CHECK(site.argument==argument && site.closed==closed && site.argumentKnown==known && site.inComment==comment && site.inLiteral==literal && site.embedded==embedded);
  CHECK(site.argumentsStart<=offset-bom && offset-bom<=site.argumentsEnd);
  if(named)CHECK(site.namedEnd-site.namedStart==strlen(named) && !memcmp(text.Data+bom+site.namedStart,named,strlen(named)));else CHECK(site.namedStart==site.namedEnd);
 }
 xlsCallContextRelease(c);xrtStrBufFree(&text);return 1;
}
static int cases(void)
{
 CHECK(one("foo(@caret@", "foo",0,0,1,0,0,0,NULL));
 CHECK(one("foo(1,@caret@)","foo",1,1,1,0,0,0,NULL));
 CHECK(one("foo(1@caret@,2)","foo",0,1,1,0,0,0,NULL));
 CHECK(one("foo(1,@caret@2)","foo",1,1,1,0,0,0,NULL));
 CHECK(one("foo(1)@caret@",NULL,0,0,0,0,0,0,NULL));
 CHECK(one("@caret@foo(1)",NULL,0,0,0,0,0,0,NULL));
 CHECK(one("outer(1,inner(@caret@),3)","inner",0,1,1,0,0,0,NULL));
 CHECK(one("outer(1,inner(2),@caret@3)","outer",2,1,1,0,0,0,NULL));
 CHECK(one("foo((1,2),[3,4],{5,6},@caret@7)","foo",3,1,1,0,0,0,NULL));
 CHECK(one("foo(1 < 2,@caret@3)","foo",1,1,1,0,0,0,NULL));
 CHECK(one("foo(inner::<dict<string,list<int>>,func(int,string) as int>(1,2),@caret@3)","foo",1,1,1,0,0,0,NULL));
 CHECK(one("obj.member::<dict<string,list<int>>>(1,@caret@2)","obj.member::<dict<string,list<int>>>",1,1,1,0,0,0,NULL));
 CHECK(one("foo(dict<string,int>{1,2},@caret@3)","foo",1,1,1,0,0,0,NULL));
 CHECK(one("foo(inner::<string,@caret@", "foo",1,0,0,0,0,0,NULL));
 CHECK(one("foo(dict<string,@caret@", "foo",1,0,0,0,0,0,NULL));
 CHECK(one("foo(dict < 7,@caret@8)", "foo",1,1,0,0,0,0,NULL));
 CHECK(one("foo(// fake(a,b)\n @caret@1)","foo",0,1,1,0,0,0,NULL));
 CHECK(one("foo(/* fake(a,@caret@b) */1)","foo",0,1,1,1,0,0,NULL));
 CHECK(one("foo(\"a,b@caret@c\",2)","foo",0,1,1,0,1,0,NULL));
 CHECK(one("foo(\"a,b\",@caret@2)","foo",1,1,1,0,0,0,NULL));
 CHECK(one("foo(f\"text {inner(1,@caret@2)}\",3)","foo",0,1,1,0,1,1,NULL));
 CHECK(one("object?.member(1,@caret@2)","object?.member",1,1,1,0,0,0,NULL));
 CHECK(one("foo(value=1,@caret@other=2)","foo",1,1,1,0,0,0,NULL));
 CHECK(one("foo(value=1,other=@caret@2)","foo",1,1,1,0,0,0,"other"));
 CHECK(one("foo(value=inner(@caret@1))","inner",0,1,1,0,0,0,NULL));
 CHECK(one("(factory())(1,@caret@2)","(factory())",1,1,1,0,0,0,NULL));
 CHECK(one("array[0](1,@caret@2)","array[0]",1,1,1,0,0,0,NULL));
 CHECK(one("getObj().member(1,@caret@2)","getObj().member",1,1,1,0,0,0,NULL));
 CHECK(one("func foo(int @caret@value) as int\nend\n",NULL,0,0,0,0,0,0,NULL));
 CHECK(one("func foo(int value) as int\n foo(1,@caret@2)\nend\n","foo",1,1,1,0,0,0,NULL));
 CHECK(one("foo(1,\nend\nfunc other(...) as int\n return @caret@0\nend\n",NULL,0,0,0,0,0,0,NULL));
 CHECK(one("\xef\xbb\xbf""foo(\"😀\",名称=@caret@7)\r\n","foo",1,1,1,0,0,0,"名称"));
 CHECK(one("foo(1,@caret@2)\n\"unterminated", "foo",1,1,1,0,0,0,NULL));
 CHECK(one("foo(1,\"unterminated@caret@",NULL,0,0,0,0,0,0,NULL));
 CHECK(one("foo(func(int value) as int\n return @caret@value\nend,2)","foo",0,1,0,0,0,1,NULL));
 return 1;
}
typedef struct reader {xls_call_context* context;size_t offset;int okay;} reader;
static int32 readMany(void* user)
{reader* r=user;r->okay=1;for(unsigned i=0;i<10000;++i){xls_call_site s;if(xlsCallContextLookup(r->context,r->offset,&s)!=1 || s.argument!=2)r->okay=0;}xlsCallContextRelease(r->context);return 0;}
static int boundaries(void)
{
 const char* source="\xef\xbb\xbf""foo(\"😀\",\r\n 1,2)\n";xls_text* text=xlsTextCreate(source,strlen(source),1024,NULL);CHECK(text);xls_call_context* c=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);CHECK(c);xls_call_site s;size_t emoji=(size_t)(strstr(source,"😀")-source)-3;CHECK(xlsCallContextLookup(c,emoji+1,&s)==-1 && !s.argumentsStart);size_t lf=(size_t)(strstr(source,"\r\n")-source)-3+1;CHECK(xlsCallContextLookup(c,lf,&s)==-1 && !s.argumentsStart);CHECK(xlsCallContextLookup(c,strlen(source),&s)==-1);CHECK(xlsCallContextLookup(NULL,0,&s)==-1 && xlsCallContextLookup(c,0,NULL)==-1);
 xls_call_context_work work=xlsCallContextWork(c);CHECK(work.tokens && work.calls==1 && work.chargedBytes>xlsTextStorageBytes(text) && work.chargedBytes<work.constructionBytes);xls_call_context* exact=xlsCallContextCreate(text,work.chargedBytes,NULL,NULL,NULL,0);CHECK(!exact);/* Completed bytes exclude required scratch; a short scratch allowance rejects. */
 exact=xlsCallContextCreate(text,work.constructionBytes,NULL,NULL,NULL,0);CHECK(exact && xlsCallContextWork(exact).constructionBytes==work.constructionBytes);xlsCallContextRelease(exact);CHECK(!xlsCallContextCreate(text,work.constructionBytes-1,NULL,NULL,NULL,0));
 xmemstats before,after;xrtMemStatsGet(&before);for(unsigned i=0;i<10000;++i)CHECK(xlsCallContextLookup(c,emoji,&s)==1);xrtMemStatsGet(&after);CHECK(before.BlockAllocCalls==after.BlockAllocCalls && before.BlockFreeCalls==after.BlockFreeCalls);
 xlsTextRelease(text);reader values[4];xthread* threads[4];size_t offset=(size_t)(strstr(source,"1,2")-source)-3+2;
 for(unsigned i=0;i<4;++i){values[i]=(reader){xlsCallContextRetain(c),offset,0};threads[i]=xrtThreadCreate(readMany,&values[i],0);CHECK(threads[i]);}xlsCallContextRelease(c);
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && values[i].okay);xrtThreadDestroy(threads[i]);}
 xlsCallContextRelease(NULL);CHECK(!xlsCallContextText(NULL) && !xlsCallContextWork(NULL).tokens);return 1;
}
typedef struct cancel_state {size_t calls,at;} cancel_state;
static int stop(void* user){cancel_state* c=user;return ++c->calls==c->at;}
static int lifetime(void)
{
 xls_call_context* held=NULL;
 for(unsigned i=0;i<120;++i){xstrbuf text={0};CHECK(xrtStrBufAppendFormat(&text,"\xef\xbb\xbf""// 中文😀%u\r\nouter(1,inner::<dict<string,list<int>>>(%u,名称=%u),2)\r\n",i,i,i));xls_text* input=xlsTextCreate(text.Data,text.Size,1024*1024,NULL);CHECK(input);xls_call_context* c=xlsCallContextCreate(input,64u*1024u*1024u,NULL,NULL,NULL,0);CHECK(c);xls_call_site s;size_t offset=(size_t)(strstr(text.Data,"名称=")-text.Data)-3+strlen("名称=");CHECK(xlsCallContextLookup(c,offset,&s)==1 && s.argument==1 && s.argumentKnown && s.closed && s.namedEnd-s.namedStart==strlen("名称"));CHECK(s.calleeEnd-s.calleeStart==strlen("inner::<dict<string,list<int>>>") && !memcmp(text.Data+3+s.calleeStart,"inner::<dict<string,list<int>>>",s.calleeEnd-s.calleeStart));CHECK(xlsCallContextWork(c).genericProofs==1);
  xls_text_edit edit={.text=" ",.length=1,.start={0,0},.end={0,0},.hasRange=1};xls_text* changed=xlsTextApply(input,&edit,1,1024*1024,NULL);CHECK(changed);xls_call_context* current=xlsCallContextCreate(changed,64u*1024u*1024u,NULL,NULL,NULL,0);CHECK(current);xls_call_site shifted;CHECK(xlsCallContextLookup(current,offset+4,&shifted)==1 && shifted.argument==1);/* Space precedes BOM,which is then ordinary source bytes. */
  xlsCallContextRelease(current);xlsTextRelease(changed);xlsTextRelease(input);xrtStrBufFree(&text);if(held){CHECK(xlsCallContextText(held) && xlsCallContextWork(held).calls==2);xlsCallContextRelease(held);}held=c;
 }
 CHECK(held && xlsCallContextText(held));xlsCallContextRelease(held);
 xls_text* text=xlsTextCreate("foo(1,2)",8,1024,NULL);CHECK(text);cancel_state c={0};xls_call_context* index=xlsCallContextCreate(text,64u*1024u*1024u,stop,&c,NULL,0);CHECK(index);size_t calls=c.calls;xlsCallContextRelease(index);CHECK(calls>10);
 for(size_t at=1;at<=calls;++at){c=(cancel_state){0,at};char error[128]={0};CHECK(!xlsCallContextCreate(text,64u*1024u*1024u,stop,&c,error,sizeof(error)) && strstr(error,"cancelled"));}
 CHECK(!xlsCallContextCreate(text,0,NULL,NULL,NULL,0) && !xlsCallContextCreate(NULL,SIZE_MAX,NULL,NULL,NULL,0));xlsTextRelease(text);
 xstrbuf deep={0};for(unsigned i=0;i<257;++i)CHECK(xrtStrBufAppend(&deep,(xstrview){"foo(",4}));index=build(deep.Data);CHECK(!index);xrtStrBufFree(&deep);return 1;
}
typedef struct allocator_state {xallocator original;xatomic64 calls;uint64 failAt;int recording;} allocator_state;
static allocator_state allocation;
static int fail(void){return allocation.recording && xrtAtomic64FetchAdd(&allocation.calls,1,XMEMORY_RELAXED)+1==allocation.failAt;}
static void* trackedAlloc(void* user,size_t size){(void)user;return fail() ? NULL : allocation.original.Alloc(allocation.original.Context,size);}
static void* trackedRealloc(void* user,void* p,size_t size){(void)user;return fail() ? NULL : allocation.original.Realloc(allocation.original.Context,p,size);}
static void trackedFree(void* user,void* p){(void)user;allocation.original.Free(allocation.original.Context,p);}
static uint64 live(void){xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int faults(void)
{
 xstrbuf source={0};for(unsigned i=0;i<1200;++i)CHECK(xrtStrBufAppendFormat(&source,"foo(%u,2)\n",i));xls_text* text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);CHECK(text);uint64 baseline=live();allocation.recording=1;allocation.failAt=0;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);xls_call_context* index=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);allocation.recording=0;uint64 calls=xrtAtomic64Load(&allocation.calls,XMEMORY_RELAXED);CHECK(index && calls);xlsCallContextRelease(index);CHECK(live()==baseline);
 for(uint64 at=1;at<=calls;++at){allocation.recording=1;allocation.failAt=at;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);index=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);allocation.recording=0;CHECK(!index);CHECK(live()==baseline);index=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);CHECK(index && xlsCallContextWork(index).calls==1200);xlsCallContextRelease(index);CHECK(live()==baseline);}
 allocation.failAt=0;printf("Call context backing allocation faults=%llu,each balanced and recovered\n",(unsigned long long)calls);xlsTextRelease(text);xrtStrBufFree(&source);return 1;
}
static int benchmark(void)
{
 for(unsigned count=1000;count<=10000;count*=10){
  xstrbuf body={0};for(unsigned i=0;i<count;++i)if(!xrtStrBufAppendFormat(&body,"foo(%u,inner(2,3),4)\n",i))return 1;xls_text* text=xlsTextCreate(body.Data,body.Size,8u*1024u*1024u,NULL);if(!text)return 1;
  uint64 began=xrtClock();xls_call_context* c=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);uint64 build=xrtClock()-began;if(!c)return 1;
  size_t offset=(size_t)(strrchr(body.Data,'4')-body.Data);xls_call_site site;began=xrtClock();for(unsigned i=0;i<100000;++i)if(xlsCallContextLookup(c,offset,&site)!=1 || site.argument!=2 || !site.closed || !site.argumentKnown)return 1;uint64 query=xrtClock()-began;
  xls_call_context_work work=xlsCallContextWork(c);printf("{\"lines\":%u,\"tokens\":%zu,\"calls\":%zu,\"buildUs\":%llu,\"queryBatchUs\":%llu,\"queries\":100000,\"chargedBytes\":%zu,\"constructionBytes\":%zu}\n",count,work.tokens,work.calls,(unsigned long long)build,(unsigned long long)query,work.chargedBytes,work.constructionBytes);xlsCallContextRelease(c);xlsTextRelease(text);xrtStrBufFree(&body);
 }
 return 0;
}
static int run(void){CHECK(cases());CHECK(boundaries());CHECK(lifetime());CHECK(faults());return 1;}
int main(int argc,char** argv){if(argc==2 && !strcmp(argv[1],"--benchmark"))return benchmark();xrtAtomic64Init(&allocation.calls,0);xrtGetAllocator(&allocation.original);xallocator custom={&allocation,trackedAlloc,trackedRealloc,trackedFree};if(!xrtSetAllocator(&custom))return 1;xrtMemStatsEnable(true);if(!run())return 1;printf("analysis_call_context_test passed: %u checks\n",checks);return 0;}
