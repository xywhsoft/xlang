#include "services/rename_candidate.h"
#include "protocol/input_state.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"rename-candidate:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xatomic32 frameMode=XRT_ATOMIC32_INIT(0),frameCalls=XRT_ATOMIC32_INIT(0),mutationOkay=XRT_ATOMIC32_INIT(0);
static xatomic64 frameThread=XRT_ATOMIC64_INIT(0);
static xevent *entered,*resume;static const char *mutationPath,*mutationBody;
static const xvalue* get(const xvalue* v,const char* k){return xrtValueType(v)==XVALUE_OBJECT?xrtValueObjectGet(v,xrtStrView(k)):NULL;}
static int put(xvalue* v,const char* k,xvalue* item){return item && xrtValueObjectSetNew(v,xrtStrView(k),item);}
char* __real_xideLspFrame(const xvalue*,size_t*,xide_model_error*);
char* __wrap_xideLspFrame(const xvalue* value,size_t* bytes,xide_model_error* error)
{
 char* frame=__real_xideLspFrame(value,bytes,error);if(!frame)return NULL;
 if(get(get(value,"result"),"binding") || get(get(value,"result"),"documentChanges") || get(get(value,"result"),"changes")){
  xrtAtomic32FetchAdd(&frameCalls,1,XMEMORY_RELAXED);xrtAtomic64Store(&frameThread,xrtThreadCurrentId(),XMEMORY_RELEASE);
  uint32 mode=xrtAtomic32Load(&frameMode,XMEMORY_ACQUIRE);
  if(mode==1)xrtAtomic32Store(&mutationOkay,xrtFileWriteAtomic(mutationPath,(xbytesview){(const unsigned char*)mutationBody,strlen(mutationBody)}),XMEMORY_RELEASE);
  if(mode==2){xrtEventSet(entered);xrtEventWait(resume);}
  if(mode==3){free(frame);*bytes=0;return NULL;}
 }
 return frame;
}
static int await(xide_rename_candidate_query* q,int expected,int code)
{
 CHECK(q);int result,actual=0;char error[512]={0};
 for(unsigned i=0;;++i){result=xideRenameCandidateResult(q,&actual,error,sizeof(error));if(result)break;if(i==10000){CHECK(0);}xrtSleep(1);}
 if(result!=expected || actual!=code)fprintf(stderr,"result=%d code=%d error=%s\n",result,actual,error);
 CHECK(result==expected && actual==code);return 1;
}
static int quiet(xide_rename_candidates* service)
{for(unsigned i=0;!xideRenameCandidatesIdle(service);++i){CHECK(i<10000);xrtSleep(1);}return 1;}
static int run(void)
{
 char* dir=xrtPathAbs("build/native-rename-candidate/源码😀");CHECK(dir && xrtDirCreateAll(dir));
 char *path=xrtPathJoin(dir,"single.xl"),*unrelated=xrtPathJoin(dir,"unrelated.xl"),*compiler=xrtPathJoin(dir,"absent-xl.exe");
 const char *body="func main(...) as int\n int amount = 0\n return amount\nend\n",*unused="func main(...) as int\n return 0\nend\n";
 CHECK(path && unrelated && compiler && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)}) && xrtFileWriteAtomic(unrelated,(xbytesview){(const unsigned char*)unused,strlen(unused)}));
 char* uri=xideLspPathToUri(path,NULL);xls_text* text=xlsTextCreate(body,strlen(body),strlen(body),NULL);CHECK(uri && text);
 xls_analysis_document docs[]={{uri,path,text,27,UINT32_MAX}};xls_input_state_item item={.version=INT_MAX};char digest[65];CHECK(xlsInputStateUri(uri,item.uri) && xlsInputStateDigest(&item,1,digest));
 xvalue* catalog=xrtJsonParse(xrtStrView("{\"schema\":1,\"selectedProject\":\"\",\"packagePath\":null,\"projects\":[]}"));xide_analysis_catalog* prepared=xideAnalysisCatalogCreate(catalog,NULL,0);
 xvalue *params=xrtJsonParse(xrtStrView("{\"textDocument\":{\"version\":2147483647},\"position\":{\"line\":1,\"character\":6},\"newName\":\"renamed_amount\",\"xlang\":{\"schema\":1,\"token\":\"0000000000000011\",\"scopeCount\":2,\"inputState\":{\"documents\":1},\"standaloneRoots\":[]}}"));CHECK(catalog && prepared && params);
 xvalue *extension=(xvalue*)get(params,"xlang"),*roots=(xvalue*)get(extension,"standaloneRoots");
 CHECK(put((xvalue*)get(params,"textDocument"),"uri",xrtValueString(xrtStrView(uri))) && put(extension,"catalog",xrtValueRetain(catalog)) && put((xvalue*)get(extension,"inputState"),"digest",xrtValueString(xrtStrView(digest))) && xrtValueArrayAppendNew(roots,xrtValueString(xrtStrView(path))) && xrtValueArrayAppendNew(roots,xrtValueString(xrtStrView(unrelated))));
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* references=xideProjectReferencesCreate(cache,compiler,NULL);xide_rename_candidates* service=xideRenameCandidatesCreate(references);CHECK(cache && references && service && xideRenameCandidatesIdle(service));
 CHECK(!xideRenameCandidateSubmit(service,0,params,catalog,prepared,docs,1));
 CHECK(!xideRenameCandidateSubmit(service,1,params,catalog,prepared,NULL,0));
 xide_rename_candidate_query* q=xideRenameCandidateSubmit(service,1,params,catalog,prepared,docs,1);CHECK(await(q,1,0));
 size_t bytes=0;char* frame=xideRenameCandidateTakeFrame(q,&bytes);CHECK(frame && bytes>100 && !xideRenameCandidateTakeFrame(q,&(size_t){0}));
 size_t header=0;while(header+4<=bytes && memcmp(frame+header,"\r\n\r\n",4))++header;CHECK(header+4<bytes);header+=4;
 xvalue* result=xrtJsonParse((xstrview){frame+header,bytes-header});CHECK(result && xrtValueType(get(result,"id"))==XVALUE_INT && get(get(result,"result"),"candidate"));xrtValueRelease(result);free(frame);xideRenameCandidateDestroy(q);CHECK(quiet(service));
 CHECK(xrtAtomic64Load(&frameThread,XMEMORY_ACQUIRE)!=xrtThreadCurrentId() && xrtAtomic32Load(&frameCalls,XMEMORY_ACQUIRE)==1);
 /* Mutate an unaffected disk provider AFTER actual response encoding. The
  * original whole-source identity sweep must reject even identical bytes. */
 mutationPath=unrelated;mutationBody=unused;xrtAtomic32Store(&frameMode,1,XMEMORY_RELEASE);
 q=xideRenameCandidateSubmit(service,2,params,catalog,prepared,docs,1);CHECK(await(q,-1,-32801));CHECK(xrtAtomic32Load(&mutationOkay,XMEMORY_ACQUIRE) && !xideRenameCandidateTakeFrame(q,&bytes));xideRenameCandidateDestroy(q);CHECK(quiet(service));
 xrtAtomic32Store(&frameMode,3,XMEMORY_RELEASE);q=xideRenameCandidateSubmit(service,3,params,catalog,prepared,docs,1);CHECK(await(q,-1,-32603) && !xideRenameCandidateTakeFrame(q,&bytes));xideRenameCandidateDestroy(q);CHECK(quiet(service));
 entered=xrtEventCreate(false,false);resume=xrtEventCreate(false,false);CHECK(entered && resume);xrtAtomic32Store(&frameMode,2,XMEMORY_RELEASE);
 q=xideRenameCandidateSubmit(service,4,params,catalog,prepared,docs,1);CHECK(q && xrtEventWaitFor(entered,XIDE_XRT_TIMEOUT_MS(10000))==XWAIT_OK);
 xideRenameCandidateCancel(q);CHECK(!xideRenameCandidatesIdle(service) && !xideRenameCandidateTakeFrame(q,&bytes));xrtEventSet(resume);CHECK(quiet(service) && await(q,-1,-32800));xideRenameCandidateDestroy(q);
 xrtEventDestroy(entered);xrtEventDestroy(resume);entered=resume=NULL;xrtAtomic32Store(&frameMode,0,XMEMORY_RELEASE);
 /* No borrowed caller text/catalog/DOM survives submission. */
 q=xideRenameCandidateSubmit(service,5,params,catalog,prepared,docs,1);CHECK(q);xrtValueRelease(params);xrtValueRelease(catalog);xideAnalysisCatalogDestroy(prepared);xlsTextRelease(text);free(uri);
 CHECK(await(q,1,0));frame=xideRenameCandidateTakeFrame(q,&bytes);CHECK(frame && bytes);xideRenameCandidateDestroy(q);CHECK(quiet(service));xideRenameCandidatesDestroy(service);xideProjectReferencesDestroy(references);xideToolchainsDestroy(cache);CHECK(!memcmp(frame,"Content-Length:",15));free(frame);
 size_t size=0;unsigned char* disk=xrtFileReadAll(path,&size);CHECK(disk && size==strlen(body) && !memcmp(disk,body,size));xrtFree(disk);
 xrtFree(path);xrtFree(unrelated);xrtFree(compiler);xrtFree(dir);return 1;
}
static int standard(void)
{
 char* dir=xrtPathAbs("build/native-standard-rename/源码😀");CHECK(dir && xrtDirCreateAll(dir));
 char *path=xrtPathJoin(dir,"single.xl"),*dependency=xrtPathJoin(dir,"dependency.xl"),*compiler=xrtPathJoin(dir,"absent-xl.exe");
 const char *body="import dependency\nfunc main(...) as int\n int amount = 0\n return amount\nend\n",*other="export func value() as int\n return 0\nend\n";
 CHECK(path && dependency && compiler && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)}) && xrtFileWriteAtomic(dependency,(xbytesview){(const unsigned char*)other,strlen(other)}));
 char* uri=xideLspPathToUri(path,NULL);xls_text* text=xlsTextCreate(body,strlen(body),strlen(body),NULL);CHECK(uri && text);
 xls_analysis_document doc={uri,path,text,88,0};xvalue* params=xrtJsonParse(xrtStrView("{\"textDocument\":{},\"position\":{\"line\":2,\"character\":6},\"newName\":\"renamed_amount\"}"));
 CHECK(params && put((xvalue*)get(params,"textDocument"),"uri",xrtValueString(xrtStrView(uri))));
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* references=xideProjectReferencesCreate(cache,compiler,NULL);xide_rename_candidates* service=xideRenameCandidatesCreate(references);CHECK(cache && references && service);
 xvalue* id=xrtValueString(xrtStrView("typed-😀"));CHECK(id && !xideRenameStandardSubmit(service,NULL,1,params,NULL,&doc,1));
 xrtAtomic32Store(&frameMode,0,XMEMORY_RELEASE);
 for(int format=0;format<2;++format){xide_rename_candidate_query* q=xideRenameStandardSubmit(service,id,format,params,NULL,&doc,1);CHECK(await(q,1,0));size_t bytes=0;char* frame=xideRenameCandidateTakeFrame(q,&bytes);CHECK(frame && bytes);size_t at=0;while(at+4<=bytes && memcmp(frame+at,"\r\n\r\n",4))++at;CHECK(at+4<bytes);at+=4;xvalue* result=xrtJsonParse((xstrview){frame+at,bytes-at});CHECK(result && xrtValueEqual(get(result,"id"),id) && xrtValueCount(get(result,"result"))==1 && get(get(result,"result"),format ? "documentChanges" : "changes"));xrtValueRelease(result);free(frame);xideRenameCandidateDestroy(q);CHECK(quiet(service));}
 mutationPath=dependency;mutationBody=other;xrtAtomic32Store(&frameMode,1,XMEMORY_RELEASE);xide_rename_candidate_query* q=xideRenameStandardSubmit(service,id,1,params,NULL,&doc,1);CHECK(await(q,-1,-32801) && xrtAtomic32Load(&mutationOkay,XMEMORY_ACQUIRE));xideRenameCandidateDestroy(q);CHECK(quiet(service));
 xrtAtomic32Store(&frameMode,3,XMEMORY_RELEASE);q=xideRenameStandardSubmit(service,id,0,params,NULL,&doc,1);CHECK(await(q,-1,-32603));xideRenameCandidateDestroy(q);CHECK(quiet(service));
 entered=xrtEventCreate(false,false);resume=xrtEventCreate(false,false);CHECK(entered && resume);xrtAtomic32Store(&frameMode,2,XMEMORY_RELEASE);q=xideRenameStandardSubmit(service,id,1,params,NULL,&doc,1);CHECK(q && xrtEventWaitFor(entered,XIDE_XRT_TIMEOUT_MS(10000))==XWAIT_OK);xideRenameCandidateCancel(q);CHECK(!xideRenameCandidatesIdle(service));xrtEventSet(resume);CHECK(quiet(service) && await(q,-1,-32800));xideRenameCandidateDestroy(q);xrtEventDestroy(entered);xrtEventDestroy(resume);entered=resume=NULL;
 xrtAtomic32Store(&frameMode,0,XMEMORY_RELEASE);q=xideRenameStandardSubmit(service,id,1,params,NULL,&doc,1);CHECK(q);xrtValueRelease(id);xrtValueRelease(params);xlsTextRelease(text);free(uri);CHECK(await(q,1,0));xideRenameCandidateDestroy(q);CHECK(quiet(service));
 CHECK(xrtAtomic64Load(&frameThread,XMEMORY_ACQUIRE)!=xrtThreadCurrentId());xideRenameCandidatesDestroy(service);xideProjectReferencesDestroy(references);xideToolchainsDestroy(cache);size_t size=0;unsigned char* disk=xrtFileReadAll(path,&size);CHECK(disk && size==strlen(body) && !memcmp(disk,body,size));xrtFree(disk);xrtFree(path);xrtFree(dependency);xrtFree(compiler);xrtFree(dir);return 1;
}
int main(void){if(!run() || !standard())return 1;printf("PASS rename candidate orchestration: %u checks; actual worker frame, original post-frame disk identity, encoding failure, cancellation versus physical quiescence, detached inputs and read-only native candidate\n",checks);return 0;}
