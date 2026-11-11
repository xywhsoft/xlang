#include "workspace_edit_fixture.h"
static int tracking;static size_t calls,deny=SIZE_MAX,singleFault;
static int refused(void){if(!tracking)return 0;return ++calls==deny;}
void* __real_malloc(size_t);void* __wrap_malloc(size_t n){return refused()?NULL:__real_malloc(n);}
void* __real_calloc(size_t,size_t);void* __wrap_calloc(size_t n,size_t s){return refused()?NULL:__real_calloc(n,s);}
void* __real_xrtMalloc(size_t);void* __wrap_xrtMalloc(size_t n){return refused()?NULL:__real_xrtMalloc(n);}
void* __real_xrtCalloc(size_t,size_t);void* __wrap_xrtCalloc(size_t n,size_t s){return refused()?NULL:__real_xrtCalloc(n,s);}
bool __real_xrtArrayResize(xarray*,size_t);bool __wrap_xrtArrayResize(xarray* a,size_t n){return refused()?false:__real_xrtArrayResize(a,n);}
bool __real_xrtStrBufReserve(xstrbuf*,size_t);bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n){return refused()?false:__real_xrtStrBufReserve(b,n);}
xls_text* __real_xlsTextCreate(const char*,size_t,size_t,enum xls_text_error*);
xls_text* __wrap_xlsTextCreate(const char* s,size_t n,size_t limit,enum xls_text_error* e){if(refused()){if(e)*e=XLS_TEXT_MEMORY;return NULL;}return __real_xlsTextCreate(s,n,limit,e);}
typedef struct stopping {size_t calls,at;} stopping;
static int stopAt(void* user){stopping* s=user;return ++s->calls==s->at;}
static int faults(void)
{
 xrtMemStatsEnable(true);CHECK(xrtMemStatsEnabled());xls_text* before=xlsTextCreate("abc\n",4,1024,NULL);CHECK(before);xide_workspace_edit_input input={"file:///D:/edit/fault.xl",before,1,9,1,41};xvalue* value=oneEdit(input.uri,1,41,"替换😀\r\n");CHECK(value);enum xide_workspace_edit_error why;
 if(singleFault){
  uint64_t base=editBlocks();calls=0;deny=singleFault;tracking=1;xide_workspace_edit* result=xideWorkspaceEditPrepare(value,&input,1,0,0,NULL,NULL,&why);tracking=0;
  CHECK(calls>=singleFault);
  if(result){xide_workspace_edit_document d;CHECK(!why && xideWorkspaceEditDocumentAt(result,0,&d) && !strcmp(xlsTextData(d.after),"替换😀\nbc\n"));xideWorkspaceEditRelease(result);}
  else CHECK(why!=XIDE_WORKSPACE_EDIT_OK);
  CHECK(editBlocks()==base && !strcmp(xlsTextData(before),"abc\n"));xrtValueRelease(value);xlsTextRelease(before);return 1;
 }
 calls=0;tracking=1;xide_workspace_edit* p=xideWorkspaceEditPrepare(value,&input,1,0,0,NULL,NULL,&why);tracking=0;size_t observed=calls;CHECK(p && !why && observed>10);xideWorkspaceEditRelease(p);
 /* The harness restarts this executable for every cold allocation gate. A
  * same-process loop changes allocator/cache state and cannot cover cold gates. */
 deny=SIZE_MAX;stopping s={0,SIZE_MAX};p=xideWorkspaceEditPrepare(value,&input,1,0,0,stopAt,&s,&why);CHECK(p && !why && s.calls>5);xideWorkspaceEditRelease(p);size_t polls=s.calls;
 for(size_t n=1;n<=polls;++n){uint64_t base=editBlocks();s=(stopping){0,n};p=xideWorkspaceEditPrepare(value,&input,1,0,0,stopAt,&s,&why);CHECK(!p && why==XIDE_WORKSPACE_EDIT_CANCELLED && editBlocks()==base && !strcmp(xlsTextData(before),"abc\n"));}
 xrtValueRelease(value);xlsTextRelease(before);printf("fault coverage: %zu observed allocation gates,%zu cancellation polls\n",observed,polls);return 1;
}
typedef struct prepared_reader {xide_workspace_edit* plan;int okay;} prepared_reader;
static int32 readPrepared(void* user)
{
 prepared_reader* r=user;r->okay=1;
 for(size_t i=0;i<1000;++i){xide_workspace_edit_document d;xide_workspace_edit_info info;if(!xideWorkspaceEditInfo(r->plan,&info) || info.documents!=1 || !xideWorkspaceEditDocumentAt(r->plan,0,&d) || strcmp(xlsTextData(d.before),"abc") || strcmp(xlsTextData(d.after),"Abc")){r->okay=0;break;}}
 xideWorkspaceEditRelease(r->plan);return r->okay?0:1;
}
static int readers(void)
{
 xls_text* text=xlsTextCreate("abc",3,1024,NULL);CHECK(text);xide_workspace_edit_input input={"file:///D:/edit/reader.xl",text,1,2,1,3};xvalue* value=oneEdit(input.uri,1,3,"A");CHECK(value);xide_workspace_edit* plan=xideWorkspaceEditPrepare(value,&input,1,0,0,NULL,NULL,NULL);CHECK(plan);xlsTextRelease(text);xrtValueRelease(value);
 prepared_reader states[4];void* threads[4];for(size_t i=0;i<4;++i){states[i]=(prepared_reader){xideWorkspaceEditRetain(plan),0};threads[i]=xrtThreadCreate(readPrepared,states+i,0);CHECK(threads[i]);}xideWorkspaceEditRelease(plan);
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && states[i].okay);xrtThreadDestroy(threads[i]);}return 1;
}
int main(int argc,char** argv){if(argc==2){for(const char* p=argv[1];*p;++p){if(*p<'0' || *p>'9' || singleFault>100000)return 1;singleFault=singleFault*10+(unsigned)(*p-'0');}if(!singleFault)return 1;}else if(argc!=1)return 1;if(!faults() || (!singleFault && !readers()))return 1;printf("workspace_edit_fault_test passed: %u checks; cold allocation gate%zu or cancellation polls and four detached readers, complete cleanup and immutable inputs\n",checks,singleFault);return 0;}
