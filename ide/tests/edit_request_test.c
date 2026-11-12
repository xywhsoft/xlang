#define _WIN32_WINNT 0x0601
#include "services/edit_request.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;static int probing;static size_t calls,failAt,textCreates;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"edit-request:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
extern void*(*__real___imp_xrtMalloc)(size_t);extern void*(*__real___imp_xrtCalloc)(size_t,size_t);
static int refused(void){return probing&&++calls==failAt;}
static void* xm(size_t n){return refused()?NULL:__real___imp_xrtMalloc(n);}
static void* xc(size_t n,size_t z){return refused()?NULL:__real___imp_xrtCalloc(n,z);}
void*(*__wrap___imp_xrtMalloc)(size_t)=xm;void*(*__wrap___imp_xrtCalloc)(size_t,size_t)=xc;
xls_text* __real_xlsTextCreate(const char*,size_t,size_t,enum xls_text_error*);
xls_text* __wrap_xlsTextCreate(const char* p,size_t n,size_t limit,enum xls_text_error* e){if(probing)++textCreates;return __real_xlsTextCreate(p,n,limit,e);}
typedef struct models {xide_edit_request_model* items;size_t count,calls;int fail;} models;
static int modelAt(void* user,size_t i,xide_edit_request_model* out){models* m=user;++m->calls;if(m->fail&&(size_t)m->fail==m->calls)return 0;if(i>=m->count)return 0;*out=m->items[i];return 1;}
static int emit(void* unused,const char* method,const xvalue* params){(void)unused;(void)method;(void)params;return 1;}
typedef struct reader {xide_edit_request* request;HANDLE start;const xls_text* original;int okay;} reader;
static DWORD WINAPI readScope(void* raw)
{
 reader* r=raw;if(WaitForSingleObject(r->start,10000)!=WAIT_OBJECT_0)return 1;r->okay=1;
 for(int i=0;i<2000;++i){const xls_sync* sync=xideEditRequestSync(r->request);xls_sync_binding b={0};if(!xlsSyncBindingAt(sync,0,&b)||b.identity!=11||b.revision!=7||strcmp(b.uri,"file:///main.xl")){r->okay=0;break;}xls_text* text=xlsSyncRetainText(sync,b.uri,b.version);if(text!=r->original||xlsTextSize(text)!=8u*1024u*1024u||xlsTextData(text)[0]!='a'||xlsTextData(text)[xlsTextSize(text)-1]!='\n'||xrtValueCount(xideEditRequestCatalog(r->request))!=1)r->okay=0;xlsTextRelease(text);(void)xideEditRequestInvalidated(r->request);}
 xideEditRequestRelease(r->request);return r->okay?0:1;
}
int main(void)
{
 CHECK(xideEditRequestInvalidated(NULL)==XIDE_EDIT_REQUEST_ENDED&&!xideEditRequestToken(NULL)&&!xideEditRequestId(NULL)&&!xideEditRequestMetadata(NULL));
 size_t n=8u*1024u*1024u;char* body=malloc(n+1);CHECK(body);memset(body,'a',n);body[n-1]='\n';body[n]=0;
 xls_sync* sync=xlsSyncCreateMonotonic(2);xls_sync_document doc={"file:///main.xl",body,n,11,7};CHECK(sync&&xlsSyncUpdate(sync,&doc,1,emit,NULL));free(body);xls_text* text=xlsSyncRetainText(sync,doc.uri,1);CHECK(text);
 xvalue* catalog=xrtValueObject();CHECK(catalog&&xrtValueObjectSetNew(catalog,xrtStrView("schema"),xrtValueInt(1)));
 char path[]="F:/main.xl",language[]="xlang";xide_edit_request_model items[]={{11,7,1,1,0,path,language},{12,4,1,0,2,NULL,"c"},{0,0,0,0,0,NULL,NULL}};models m={items,3,0,0};xide_model_error error={0};
 CHECK(!xideEditRequestCreate(0,2,sync,catalog,3,modelAt,&m,&error)&&error.code);CHECK(!xideEditRequestCreate(1,0,sync,catalog,3,modelAt,&m,&error)&&error.code);CHECK(!xideEditRequestCreate(1,2,NULL,catalog,3,modelAt,&m,&error)&&error.code);CHECK(!xideEditRequestCreate(1,2,sync,catalog,4097,(void*)(uintptr_t)1,NULL,&error)&&error.code);
 items[0].revision=0;CHECK(!xideEditRequestCreate(1,2,sync,catalog,3,modelAt,&m,&error));items[0].revision=7;
 for(int i=1;i<=6;++i){m.calls=0;m.fail=i;CHECK(!xideEditRequestCreate(1,2,sync,catalog,3,modelAt,&m,&error)&&error.code);}m.fail=0;
 calls=failAt=textCreates=0;probing=1;xide_edit_request* p=xideEditRequestCreate(1,23,sync,catalog,3,modelAt,&m,&error);probing=0;CHECK(p&&!textCreates&&calls);size_t allocations=calls;
 CHECK(xideEditRequestToken(p)==1&&xideEditRequestId(p)==23&&xideEditRequestModelCount(p)==3&&xideEditRequestMetadata(p)<1024u*1024u&&!xideEditRequestInvalidated(p)&&xideEditRequestCatalog(p)==catalog);
 for(size_t i=0;i<3;++i)CHECK(xideEditRequestModelMatches(p,i,items+i));
 CHECK(!xideEditRequestModelMatches(p,3,items)&&!xideEditRequestModelMatches(NULL,0,items));
 path[3]='M';language[0]='X';CHECK(!xideEditRequestModelMatches(p,0,items));path[3]='m';language[0]='x';CHECK(xideEditRequestModelMatches(p,0,items));
 for(int i=0;i<5;++i){xide_edit_request_model changed=items[0];if(i==0)++changed.identity;if(i==1)++changed.revision;if(i==2)changed.open=0;if(i==3)changed.dirty=0;if(i==4)changed.external=3;CHECK(!xideEditRequestModelMatches(p,0,&changed));}
 xls_text* shared=xlsSyncRetainText(xideEditRequestSync(p),doc.uri,1);CHECK(shared==text);xlsTextRelease(shared);
 for(size_t i=1;i<=allocations;++i){calls=0;failAt=i;probing=1;xide_edit_request* bad=xideEditRequestCreate(2,24,sync,catalog,3,modelAt,&m,&error);probing=0;CHECK(!bad&&error.code&&calls>=i);}failAt=0;
 /* Actual bound and combined metadata refusal, no need for a giant body copy. */
 xide_edit_request_model* many=calloc(4096,sizeof(*many));CHECK(many);for(size_t i=0;i<4096;++i)many[i]=(xide_edit_request_model){i+1,1,1,0,0,"F:/same-spelling.xl","xlang"};models full={many,4096,0,0};xide_edit_request* bound=xideEditRequestCreate(2,24,sync,catalog,4096,modelAt,&full,&error);CHECK(bound&&xideEditRequestModelCount(bound)==4096&&xideEditRequestMetadata(bound)<=1024u*1024u);xideEditRequestRelease(bound);
 char* longPath=malloc(65537);CHECK(longPath);memset(longPath,'p',65536);longPath[65536]=0;many[0].path=longPath;CHECK(!xideEditRequestCreate(2,24,sync,catalog,4096,modelAt,&full,&error));longPath[65535]=0;for(size_t i=0;i<4096;++i)many[i].path=longPath;CHECK(!xideEditRequestCreate(2,24,sync,catalog,4096,modelAt,&full,&error));free(longPath);free(many);
 xide_edit_request* empty=xideEditRequestCreate(3,25,sync,catalog,0,NULL,NULL,&error);CHECK(empty&&!xideEditRequestModelCount(empty));xideEditRequestRelease(empty);
 HANDLE start=CreateEventW(NULL,TRUE,FALSE,NULL),threads[4];reader readers[4];CHECK(start);
 for(int i=0;i<4;++i){readers[i]=(reader){xideEditRequestRetain(p),start,text,0};CHECK(readers[i].request);threads[i]=CreateThread(NULL,0,readScope,readers+i,0,NULL);CHECK(threads[i]);}
 xlsSyncDestroy(sync);xrtValueRelease(catalog);xlsTextRelease(text);CHECK(SetEvent(start));xideEditRequestInvalidate(p,XIDE_EDIT_REQUEST_DOCUMENT);xideEditRequestInvalidate(p,XIDE_EDIT_REQUEST_CANCELLED);CHECK(xideEditRequestInvalidated(p)==(XIDE_EDIT_REQUEST_DOCUMENT|XIDE_EDIT_REQUEST_CANCELLED));xideEditRequestInvalidate(p,0);CHECK(xideEditRequestInvalidated(p)==17);
 for(int i=0;i<4;++i){CHECK(WaitForSingleObject(threads[i],10000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(threads[i],&code)&&!code&&readers[i].okay);CloseHandle(threads[i]);}CloseHandle(start);xideEditRequestRelease(p);
 printf("PASS edit request: %u checks; %zu observed direct allocation refusals, 8MiB exact text sharing, actual4096 roster, all model fields, callback prefixes, four independent retained readers and monotonic invalidation\n",checks,allocations);return 0;
}
