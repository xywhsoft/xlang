#include "analysis/frontend_completion_pool.h"
#include "analysis/frontend_context.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static size_t seen,deny,live,lastBytes;static int active;static void* pointers[8192];static unsigned checks;
static int refused(void){return active && ++seen==deny;}
static void remember(void* p){if(p && active){for(size_t i=0;i<8192;++i)if(pointers[i]==p)return;for(size_t i=0;i<8192;++i)if(!pointers[i]){pointers[i]=p;++live;break;}}}
static void forget(void* p){if(p)for(size_t i=0;i<8192;++i)if(pointers[i]==p){pointers[i]=NULL;--live;break;}}
bool __real_xrtStrBufAppend(xstrbuf*,xstrview);void __real_xrtStrBufFree(xstrbuf*);void* __real_xrtMalloc(size_t);
void* __real_xrtRealloc(void*,size_t);char* __real_xrtStrDup(const char*);char* __real_xrtStrDupN(const char*,size_t);void __real_xrtFree(void*);
char* __wrap_xrtStrDup(const char* s){if(refused())return NULL;char* p=__real_xrtStrDup(s);remember(p);return p;}
char* __wrap_xrtStrDupN(const char* s,size_t n){if(refused())return NULL;char* p=__real_xrtStrDupN(s,n);remember(p);return p;}
void* __wrap_xrtRealloc(void* p,size_t n){if(refused())return NULL;void* q=__real_xrtRealloc(p,n);if(q){forget(p);remember(q);}return q;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
bool __wrap_xrtStrBufAppend(xstrbuf* b,xstrview value){if(refused())return false;char* old=b->Data;bool okay=__real_xrtStrBufAppend(b,value);if(b->Data!=old){forget(old);remember(b->Data);}return okay;}
void __wrap_xrtStrBufFree(xstrbuf* b){forget(b->Data);__real_xrtStrBufFree(b);}
void* __wrap_xrtMalloc(size_t size){if(refused())return NULL;void* p=__real_xrtMalloc(size);remember(p);return p;}
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"pool producer line%d: %s seen=%zu deny=%zu live=%zu\n",__LINE__,#v,seen,deny,live);return 0;}}while(0)
static int sample(size_t budget,size_t refusal,int expect,int matching)
{
 XL6_CompletionQuery q={.prefix=xrtStrDup(matching ? "beta" : "alpha"),.reached=1,.available=1};xls_completion_pool pool={0};CHECK(q.prefix);xls_completion_pool_scope scope;
 char* canonical=xrtStrDup("int");CHECK(canonical);uintptr_t originalAddress=(uintptr_t)canonical;xlsCompletionPoolBegin(&scope,&q,budget);seen=0;deny=refusal;active=1;
 CHECK(xl6CompletionAdd(&q,"beta","variable",7,NULL,canonical));CHECK(!q.error && q.count==(size_t)matching && q.available && q.reached==1);
 int okay=xlsCompletionPoolEnd(&scope,&pool);CHECK(okay==expect);
 if(okay)lastBytes=pool.admittedBytes;
 if(okay)CHECK(pool.query.count==1 && !strcmp(pool.query.items[0].name,"beta") && !strcmp(pool.query.items[0].canonical,"int") && pool.query.items[0].symbolId==7 &&
     (uintptr_t)pool.query.items[0].canonical!=originalAddress && pool.strings.Data && pool.strings.Size==9 && pool.query.capacity==1);
 if(matching)CHECK(q.items[0].canonical==canonical && !strcmp(q.items[0].canonical,"int"));
 xlsCompletionPoolFree(&pool);xl6CompletionFree(&q);CHECK(!live);active=0;return 1;
}
static int nested(void)
{
 XL6_CompletionQuery a={.reached=1,.available=1},b={.reached=1,.available=1};xls_completion_pool ap={0},bp={0};xls_completion_pool_scope outer,inner;
 xlsCompletionPoolBegin(&outer,&a,1024*1024);CHECK(xl6CompletionAdd(&a,"alpha","variable",1,NULL,NULL));
 xlsCompletionPoolBegin(&inner,&b,1024*1024);CHECK(xl6CompletionAdd(&b,"beta","variable",2,NULL,NULL));
 CHECK(!xlsCompletionPoolEnd(&outer,&ap) && !ap.query.count);CHECK(xlsCompletionPoolEnd(&inner,&bp));
 CHECK(xl6CompletionAdd(&a,"after","variable",3,NULL,NULL));CHECK(xlsCompletionPoolEnd(&outer,&ap));CHECK(ap.query.count==2 && bp.query.count==1 && a.count==2 && b.count==1);
 xl6CompletionFree(&a);xl6CompletionFree(&b);xlsCompletionPoolFree(&ap);xlsCompletionPoolFree(&bp);return 1;
}
static int privateFilter(void)
{
 XL6_CompletionQuery q={.reached=1,.available=1};xls_completion_pool pool={0};xls_completion_pool_scope scope;
 char* canonical=xrtStrDup("int");CHECK(canonical);xlsCompletionPoolBegin(&scope,&q,1024*1024);seen=deny=0;active=1;
 CHECK(xl6CompletionAdd(&q,"__xl6_private","function",7,NULL,canonical));CHECK(!seen && !q.count && !q.error);
 CHECK(xlsCompletionPoolEnd(&scope,&pool) && !pool.query.count && !pool.query.capacity && !pool.query.bytes);
 xlsCompletionPoolFree(&pool);xl6CompletionFree(&q);CHECK(!live);active=0;return 1;
}
static int zeroShieldsOuter(void)
{
 XL6_CompletionQuery q={.reached=1,.available=1};xls_completion_pool pool={0};xls_completion_pool_scope outer,inner;
 xlsCompletionPoolBegin(&outer,&q,1024*1024);xlsCompletionPoolBegin(&inner,&q,0);
 CHECK(xl6CompletionAdd(&q,"hiddenFromPool","variable",1,NULL,NULL));CHECK(!xlsCompletionPoolEnd(&inner,&pool));
 CHECK(xl6CompletionAdd(&q,"captured","variable",2,NULL,NULL));CHECK(xlsCompletionPoolEnd(&outer,&pool));
 CHECK(q.count==2 && pool.query.count==1 && !strcmp(pool.query.items[0].name,"captured"));xlsCompletionPoolFree(&pool);xl6CompletionFree(&q);return 1;
}
static int contextAbort(void)
{
 xls_frontend_context_scope context;xlsFrontendContextBegin(&context,NULL);
 XL6_CompletionQuery q={.reached=1,.available=1};xls_completion_pool pool={0};xls_completion_pool_scope scope;
 xlsCompletionPoolBegin(&scope,&q,1024*1024);CHECK(xl6CompletionAdd(&q,"temporary","variable",1,NULL,NULL));
 CHECK(!xlsFrontendContextEnd(&context) && !scope.slot && scope.failed);
 CHECK(!xlsCompletionPoolEnd(&scope,&pool) && !pool.query.count && q.count==1 && !strcmp(q.items[0].name,"temporary"));
 CHECK(xlsFrontendNamespaceReady());xl6CompletionFree(&q);return 1;
}
static int nativeFailure(size_t refusal)
{
 XL6_CompletionQuery q={.prefix=xrtStrDup("beta"),.reached=1,.available=1};xls_completion_pool pool={0};xls_completion_pool_scope scope;
 char* canonical=xrtStrDup("int");CHECK(q.prefix && canonical);xlsCompletionPoolBegin(&scope,&q,1024*1024);seen=0;deny=refusal;active=1;
 CHECK(!xl6CompletionAdd(&q,"beta","variable",7,NULL,canonical) && q.error);
 CHECK(!xlsCompletionPoolEnd(&scope,&pool) && !pool.query.count);
 xlsCompletionPoolFree(&pool);xl6CompletionFree(&q);CHECK(!live);active=0;return 1;
}
static int largeAndLimits(void)
{
 XL6_CompletionQuery q={.prefix=xrtStrDup("absent"),.reached=1,.available=1};xls_completion_pool pool={0};xls_completion_pool_scope scope;CHECK(q.prefix);
 char text[5001];memset(text,'T',5000);text[5000]=0;char* canonical=xrtStrDup(text);CHECK(canonical);
 xlsCompletionPoolBegin(&scope,&q,64*1024);CHECK(xl6CompletionAdd(&q,"large","variable",7,NULL,canonical));
 CHECK(xlsCompletionPoolEnd(&scope,&pool) && pool.query.count==1 && !strcmp(pool.query.items[0].canonical,text));
 CHECK(pool.strings.Size==5007 && pool.strings.Capacity>=pool.strings.Size && pool.admittedBytes<=64*1024);
 xlsCompletionPoolFree(&pool);CHECK(!pool.strings.Data && !pool.strings.Size);
 xlsCompletionPoolBegin(&scope,&q,1024*1024);
 for(unsigned i=0;i<=XL6_COMPLETION_ITEMS;++i){char name[32];snprintf(name,sizeof(name),"candidate%u",i);CHECK(xl6CompletionAdd(&q,name,"variable",i,NULL,NULL));}
 CHECK(!q.error && !q.count && !xlsCompletionPoolEnd(&scope,&pool));xlsCompletionPoolFree(&pool);
 char longName[514];memset(longName,'x',513);longName[513]=0;xlsCompletionPoolBegin(&scope,&q,1024*1024);
 CHECK(xl6CompletionAdd(&q,longName,"variable",1,NULL,NULL) && !q.error && !q.count && !xlsCompletionPoolEnd(&scope,&pool));
 xlsCompletionPoolFree(&pool);xl6CompletionFree(&q);return 1;
}
int main(void)
{
 xls_frontend_context_scope context;xlsFrontendContextBegin(&context,NULL);
 if(!sample(SIZE_MAX,0,1,0))return 1;
 const size_t exact=lastBytes;
 if(!sample(exact,0,1,0))return 1;
 size_t operations=seen;
 if(operations!=4)return 2;
 for(size_t i=1;i<=operations;++i)if(!sample(exact,i,0,0))return 1;
 if(!sample(exact,0,1,1) || seen!=6)return 2;
 for(size_t i=1;i<=3;++i)if(!sample(exact,i,0,1))return 1;
 if(!sample(exact,6,0,1))return 1;
 if(!sample(0,0,0,0) || !sample(1,0,0,0) || !sample(exact-1,0,0,0) || !sample(exact,0,1,0) || !nested() || !privateFilter() || !zeroShieldsOuter() || !contextAbort() || !nativeFailure(4) || !nativeFailure(5) || !largeAndLimits())return 1;
 if(!xlsFrontendContextEnd(&context) || !sample(exact,0,0,0))return 1;
 printf("completion pool producer passed: %u checks;4 buffer producer allocator refusal positions;private names allocate0,canonical copy/native ownership,0/tiny/exact-1/exact budget,nested/zero/abort/outside-context scopes,original filtered response intact\n",checks);return 0;
}
