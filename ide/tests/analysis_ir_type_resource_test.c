#include "analysis/frontend_internal.h"
#include "analysis/frontend_ir_memo.h"
#include "analysis/check.h"
#include "analysis/check_ir.h"
#include <stdio.h>
#include <string.h>

/* Observe external XRT allocations/API calls, not XRT's private allocator.
 * All injection state is thread-local; successful output is compared to the
 * complete, non-injected semantic check, never to another failed build. */
static _Thread_local size_t calls,at,denied,live,checks;
static _Thread_local int active,overflow;
static _Thread_local void* pointers[4096];
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"type-resource:%d:%s (at=%zu calls=%zu)\n",__LINE__,#v,at,calls);return 0;}}while(0)
static int refuse(void){if(active&&++calls==at){++denied;return 1;}return 0;}
static void remember(void* p)
{
 if(!active||!p)return;
 for(size_t i=0;i<4096;++i)if(pointers[i]==p)return;
 for(size_t i=0;i<4096;++i)if(!pointers[i]){pointers[i]=p;++live;return;}
 overflow=1;
}
static void forget(void* p)
{if(p)for(size_t i=0;i<4096;++i)if(pointers[i]==p){pointers[i]=NULL;--live;return;}}
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);void* __real_xrtRealloc(void*,size_t);void __real_xrtFree(void*);
char* __real_xrtStrDup(const char*);char* __real_xrtStrDupN(const char*,size_t);
void* __wrap_xrtMalloc(size_t n){if(refuse())return NULL;void* p=__real_xrtMalloc(n);remember(p);return p;}
void* __wrap_xrtCalloc(size_t n,size_t w){if(refuse())return NULL;void* p=__real_xrtCalloc(n,w);remember(p);return p;}
void* __wrap_xrtRealloc(void* p,size_t n){if(refuse())return NULL;void* q=__real_xrtRealloc(p,n);if(q){forget(p);remember(q);}return q;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
char* __wrap_xrtStrDup(const char* s){if(refuse())return NULL;char* p=__real_xrtStrDup(s);remember(p);return p;}
char* __wrap_xrtStrDupN(const char* s,size_t n){if(refuse())return NULL;char* p=__real_xrtStrDupN(s,n);remember(p);return p;}
bool __real_xrtArrayPush(xarray*,const void*);bool __real_xrtArrayResize(xarray*,size_t);
bool __real_xrtMapSet(xmap*,xbytesview,const void*);bool __real_xrtStrBufReserve(xstrbuf*,size_t);
bool __wrap_xrtArrayPush(xarray* a,const void* p){return !refuse()&&__real_xrtArrayPush(a,p);}
bool __wrap_xrtArrayResize(xarray* a,size_t n){return !refuse()&&__real_xrtArrayResize(a,n);}
bool __wrap_xrtMapSet(xmap* m,xbytesview k,const void* p){return !refuse()&&__real_xrtMapSet(m,k,p);}
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n){return !refuse()&&__real_xrtStrBufReserve(b,n);}
static char* dump(const XL6_IrModule* m)
{
 XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;xl6IrDumpModuleJson(&out,m);
 char* result=xl6OutputHasFailed(&out)?NULL:xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return result;
}
static int resourceError(const XL6_DiagList* d)
{for(size_t i=0;i<d->count;++i)if(strstr(d->items[i].message,"resource failure constructing native IR"))return 1;return 0;}
static int build(XL6_IrModule* m,const XL6_AstNode* root,XL6_DiagList* d,unsigned mode,const xls_ir_memo* old,xls_ir_memo** next)
{
 xls_ir_memo_work work;
 return mode?xlsFrontendIrBuildMemo(m,root,d,mode==2?old:NULL,next,&work,32u*1024u*1024u):xl6IrBuildModuleFromAst(m,root,d);
}
static int complete(XL6_IrModule* m,XL6_DiagList* d,const char* expected)
{
 CHECK(!d->count&&xl6IrVerifyModule(m,d)&&!d->count);char* actual=dump(m);CHECK(actual);
 int equal=!strcmp(actual,expected);xrtFree(actual);CHECK(equal);return 1;
}
typedef struct reader {const XL6_AstNode* root;const char* expected;size_t fail,checked;int okay;} reader;
static int32 readThread(void* opaque)
{
 reader* r=opaque;
 for(unsigned i=0;i<24;++i){
  XL6_IrModule m;xl6IrModuleInit(&m);XL6_DiagList d;xl6DiagInit(&d);
  active=1;calls=denied=0;at=(i&1)?0:r->fail;int okay=xl6IrBuildModuleFromAst(&m,r->root,&d);active=0;
  if(at){CHECK(denied==1&&!okay&&resourceError(&d));}else CHECK(okay&&complete(&m,&d,r->expected));
  xl6IrModuleFree(&m);xl6DiagFree(&d);CHECK(!live&&!overflow);
 }
 r->okay=1;r->checked=checks;return 0;
}
static int exercise(const char* path,const char* source,size_t* rejections)
{
 xls_semantic_input input={path,source,strlen(source)};char error[256];
 xls_semantic_check* ref=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
 CHECK(ref&&xlsSemanticCheckInfo(ref).accepted);const XL6_AstNode* root=xlsSemanticCheckAst(ref);
 char* expected=dump(xlsSemanticCheckIr(ref));CHECK(expected);xls_ir_memo* old=NULL;
 XL6_IrModule seed;xl6IrModuleInit(&seed);XL6_DiagList sd;xl6DiagInit(&sd);
 CHECK(build(&seed,root,&sd,1,NULL,&old)&&old&&complete(&seed,&sd,expected));xl6IrModuleFree(&seed);xl6DiagFree(&sd);
 size_t requiredAt=0;
 for(unsigned mode=0;mode<3;++mode){
  XL6_IrModule m;XL6_DiagList d;xls_ir_memo* next=NULL;xl6IrModuleInit(&m);xl6DiagInit(&d);
  active=1;calls=at=denied=0;int okay=build(&m,root,&d,mode,old,&next);active=0;
  CHECK(okay&&complete(&m,&d,expected));size_t count=calls;CHECK(count>10&&count<4096);
  xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&m);xl6DiagFree(&d);CHECK(!live&&!overflow);
  for(size_t fail=1;fail<=count;++fail){
   xl6IrModuleInit(&m);xl6DiagInit(&d);next=NULL;active=1;calls=denied=0;at=fail;
   okay=build(&m,root,&d,mode,old,&next);active=0;CHECK(denied==1&&!overflow);
   if(okay)CHECK(complete(&m,&d,expected));else{CHECK(!next);if(resourceError(&d)){++*rejections;if(!mode&&!requiredAt)requiredAt=fail;CHECK(!m.nodes&&!m.nodeCount&&!m.types.items);}}
   xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&m);xl6DiagFree(&d);CHECK(!live);
  }
  /* A failure must not poison the next request on the same owner thread. */
  xl6IrModuleInit(&m);xl6DiagInit(&d);next=NULL;CHECK(build(&m,root,&d,mode,old,&next)&&complete(&m,&d,expected));
  xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&m);xl6DiagFree(&d);
  printf("type-resource mode%u: %zu allocation/API rejection points\n",mode,count);
 }
 CHECK(requiredAt);
 reader readers[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){readers[i]=(reader){root,expected,i&1?0:requiredAt,0,0};threads[i]=xrtThreadCreate(readThread,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].okay);checks+=readers[i].checked;xrtThreadDestroy(threads[i]);}
 CHECK(xlsSemanticCheckValidate(ref));char* held=dump(xlsSemanticCheckIr(ref));CHECK(held&&!strcmp(held,expected));xrtFree(held);
 xlsFrontendIrMemoDestroy(old);xlsSemanticCheckDestroy(ref);xrtFree(expected);return 1;
}
int main(void)
{
 const char* cases[]={
 "func stable(int count) as int\n string value=\"outer\"\n try\n while count>0\n string item=\"item\"\n count=count-1\n if count>2\n continue\n end\n break\n end\n return count+2\n finally\n string final=\"final\"\n end\nend\nfunc main(...) as int\n return stable(4)\nend\n",
 "func closure() as func() as int\n array values=[41]\n return func() as int\n return int(values[0])\n end\nend\n",
 "enum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\n"};
 char* path=xrtPathAbs("type-resource-中文.xl");if(!path)return 1;size_t rejections=0;
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i)if(!exercise(path,cases[i],&rejections))return 1;
 xrtFree(path);if(!rejections)return 1;
 printf("analysis_ir_type_resource_test passed: %zu checks;%zu required type failures rejected;direct/cold/hot,four threads,full JSON/native verifier,retired outputs\n",checks,rejections);return 0;
}
