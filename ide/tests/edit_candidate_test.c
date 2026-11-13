#include "services/edit_candidate.h"
#include "services/analysis_catalog.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;static xide_language_client* client;
#ifdef XIDE_CANDIDATE_FAULT_TEST
static int probing;static DWORD probeThread;static size_t probeCalls,probeFail;
static int denyTouch,touchActive;static size_t failedTouches;
wchar_t* __real_xidePathNativeWide(const char*);
wchar_t* __wrap_xidePathNativeWide(const char* path){if(touchActive && GetCurrentThreadId()==probeThread){++failedTouches;return NULL;}return __real_xidePathNativeWide(path);}
int __real_xideRenameEvidenceTouches(const xide_rename_evidence*,const char*);
int __wrap_xideRenameEvidenceTouches(const xide_rename_evidence* evidence,const char* path){touchActive=denyTouch;int result=__real_xideRenameEvidenceTouches(evidence,path);touchActive=0;return result;}
static int denied(void){return probing && GetCurrentThreadId()==probeThread && ++probeCalls==probeFail;}
#ifdef XLS_COMPILER_XRT
bool __real_xrtMapInit(xmap*,size_t);ptr __real_xrtMapGetOrAdd(xmap*,xbytesview,bool*);
bool __wrap_xrtMapInit(xmap* m,size_t n){return denied()?false:__real_xrtMapInit(m,n);}
ptr __wrap_xrtMapGetOrAdd(xmap* m,xbytesview k,bool* added){if(denied()){if(added)*added=false;return NULL;}return __real_xrtMapGetOrAdd(m,k,added);}
#else
extern bool(*__real___imp_xrtMapInit)(xmap*,size_t);extern ptr(*__real___imp_xrtMapGetOrAdd)(xmap*,xbytesview,bool*);
static bool mapInit(xmap* m,size_t n){return denied()?false:__real___imp_xrtMapInit(m,n);}
static ptr mapEntry(xmap* m,xbytesview k,bool* added){if(denied()){if(added)*added=false;return NULL;}return __real___imp_xrtMapGetOrAdd(m,k,added);}
bool(*__wrap___imp_xrtMapInit)(xmap*,size_t)=mapInit;ptr(*__wrap___imp_xrtMapGetOrAdd)(xmap*,xbytesview,bool*)=mapEntry;
#endif
#endif
static int failed(int line,const char* message){fprintf(stderr,"edit-candidate:%d:%s\n",line,message);if(client){xideLanguageClientStop(client);client=NULL;}return 0;}
#define CHECK(v) do{++checks;if(!(v))return failed(__LINE__,#v);}while(0)
static const xvalue* get(const xvalue* v,const char* k){return xrtValueIs(v,XVALUE_OBJECT)?xrtValueObjectGet(v,xrtStrView(k)):NULL;}
static int put(xvalue* v,const char* k,xvalue* item){return item && xrtValueObjectSetNew(v,xrtStrView(k),item);}
static int emit(void* user,const char* method,const xvalue* params)
{(void)user;xvalue* root=xrtValueObject();int okay=root && put(root,"jsonrpc",xrtValueString(xrtStrView("2.0"))) && put(root,"method",xrtValueString(xrtStrView(method))) && put(root,"params",xrtValueRetain(params)) && xideLanguageClientSend(client,root,NULL);xrtValueRelease(root);return okay;}
static int sendRequest(const char* method,const xvalue* params,int id)
{xvalue* root=xrtValueObject();int okay=root && put(root,"jsonrpc",xrtValueString(xrtStrView("2.0"))) && put(root,"id",xrtValueInt(id)) && put(root,"method",xrtValueString(xrtStrView(method))) && put(root,"params",xrtValueRetain(params)) && xideLanguageClientSend(client,root,NULL);xrtValueRelease(root);return okay;}
static xvalue* receive(int id,size_t* bytes)
{ULONGLONG until=GetTickCount64()+15000;while(GetTickCount64()<until){xvalue* root=xideLanguageClientNextSized(client,bytes);int64 number=0;if(root && xrtValueGetInt(get(root,"id"),&number) && number==id)return root;xrtValueRelease(root);if(xideLanguageClientState(client,NULL,0)>=XIDE_LS_STOPPED)return NULL;Sleep(1);}return NULL;}
typedef struct fixture {char *paths[8],*uri;xvalue* catalog;xls_sync* sync;xide_files files;xide_file loose;xide_edit_request_model models[2];size_t modelsCount;int request;uint64_t token;const char* body;} fixture;
static int modelAt(void* raw,size_t i,xide_edit_request_model* out){fixture* f=raw;if(i>=f->modelsCount)return 0;*out=f->models[i];return 1;}
static xvalue* configuration(const char* dir,fixture* f)
{
 xvalue *root=xrtValueObject(),*projects=xrtValueArray();int okay=root && projects;
 const size_t entries[]={0,1,4};
 for(size_t i=0;okay && i<3;++i){const char* members[3]={f->paths[entries[i]],f->paths[2],f->paths[3]};size_t count=i==0?2:i==1?3:1;char* project=xidePathResolve(dir,i==0?"A.xlp":i==1?"B.xlp":"C.xlp");
  xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=f->paths[entries[i]],.directory=dir,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,count};xls_context* context=project?xlsContextCreate(&spec,16u*1024u*1024u,NULL):NULL;xvalue* wire=context?xideAnalysisContextWrite(context):NULL;okay=wire && xrtValueArrayAppendNew(projects,wire);xlsContextRelease(context);free(project);
 }
 char* selected=xidePathResolve(dir,"A.xlp");okay=okay && selected && put(root,"schema",xrtValueInt(1)) && put(root,"selectedProject",xrtValueString(xrtStrView(selected))) && put(root,"packagePath",xrtValueNull());free(selected);
 if(okay){okay=put(root,"projects",projects);projects=NULL;}xrtValueRelease(projects);if(!okay){xrtValueRelease(root);root=NULL;}return root;
}
static int mutate(xvalue* root,int mode,fixture* f)
{
 xvalue* candidate=(xvalue*)get(get(root,"result"),"candidate");xvalue* guards=(xvalue*)get(candidate,"inputGuards");xvalue* edit=(xvalue*)get(candidate,"workspaceEdit");xvalue* changes=(xvalue*)get(edit,"documentChanges");
 if(mode==1 || mode==2){xvalue* replacement=xrtValueArray();CHECK(replacement);const char* target=f->paths[mode==1?4:3];size_t removed=0;for(size_t i=0;i<xrtValueCount(guards);++i){const xvalue* row=xrtValueArrayGet(guards,i);xstrview entry={0};CHECK(xrtValueGetString(get(get(row,"context"),"entry"),&entry));if(xidePathLexicalEqual(entry.Data,target)){++removed;continue;}CHECK(xrtValueArrayAppend(replacement,(xvalue*)row));}CHECK(removed && put(candidate,"inputGuards",replacement));}
 if(mode==3)CHECK(put((xvalue*)get(xrtValueArrayGet(guards,0),"context"),"configuration",xrtValueString(xrtStrView("forged"))));
 if(mode==4){int changed=0;for(size_t i=0;i<xrtValueCount(guards) && !changed;++i){const xvalue* files=get(xrtValueArrayGet(guards,i),"files");for(size_t j=0;j<xrtValueCount(files);++j){xvalue* file=(xvalue*)xrtValueArrayGet(files,j);xstrview origin={0};if(xrtValueGetString(get(file,"origin"),&origin) && !strcmp(origin.Data,"disk")){CHECK(put(file,"rawSha256",xrtValueString(xrtStrView("0000000000000000000000000000000000000000000000000000000000000000"))));changed=1;break;}}}CHECK(changed);}
 if(mode==5)CHECK(put((xvalue*)xrtValueArrayGet(get(xrtValueArrayGet(changes,0),"edits"),0),"newText",xrtValueString(xrtStrView("unrequested"))));
 if(mode==6){xvalue* range=xrtJsonParse(xrtStrView("{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":4}}"));CHECK(range && put((xvalue*)xrtValueArrayGet(get(xrtValueArrayGet(changes,0),"edits"),0),"range",range));}
 if(mode==7){int changed=0;for(size_t i=0;i<xrtValueCount(changes);++i){const xvalue* proof=get(get(xrtValueArrayGet(changes,i),"xlang"),"document");xvalue* input=(xvalue*)get(proof,"input");if(input){CHECK(put(input,"futureTamper",xrtValueBool(true)));changed=1;break;}}CHECK(changed);}
 if(mode==8){for(size_t i=0;i<xrtValueCount(changes);++i){xvalue* document=(xvalue*)get(xrtValueArrayGet(changes,i),"textDocument");if(xrtValueType(get(document,"version"))==XVALUE_INT){CHECK(put(document,"version",xrtValueNull()));break;}}}
 if(mode==9)CHECK(put(edit,"changes",xrtValueObject()));
 if(mode==13){xvalue* remaining=xrtValueArray();size_t removed=0;CHECK(remaining);for(size_t i=0;i<xrtValueCount(changes);++i){xvalue* row=(xvalue*)xrtValueArrayGet(changes,i);xstrview uri={0};CHECK(xrtValueGetString(get(get(row,"textDocument"),"uri"),&uri));if(!strcmp(uri.Data,f->uri)){++removed;continue;}CHECK(xrtValueArrayAppend(remaining,row));}CHECK(removed==1 && put(edit,"documentChanges",remaining));}
 return 1;
}
static int stopNow(void* ignored){(void)ignored;return 1;}
static int attempt(fixture* f,int mode,int valid,xide_edit_candidate** keep)
{
 xide_model_error error={0};xide_edit_request* request=xideEditRequestCreate(++f->token,++f->request,f->sync,f->catalog,&f->files,f->modelsCount,modelAt,f,&error);CHECK(request);
 xls_text* text=xlsSyncRetainText(f->sync,f->uri,xlsSyncVersion(f->sync,f->uri));CHECK(text);const char* target=strstr(xlsTextData(text),"compute(1)");xls_position cursor;CHECK(target && xlsTextPosition(text,(size_t)(target-xlsTextData(text)+2),&cursor));xlsTextRelease(text);
 xide_edit_call* call=xideEditCallCreate(request,xideLanguageClientIdentity(client),f->uri,(int)cursor.line,(int)cursor.character,xrtStrView("renamed_😀"),NULL,NULL,NULL);CHECK(call && xideEditCallSend(call,client));size_t bytes=0;xvalue* root=receive(f->request,&bytes);if(root && get(root,"error")){char* s=xrtJsonStringify(root,false,NULL);if(s){fprintf(stderr,"native RPC error: %s\n",s);xrtFree(s);}}
 CHECK(root && get(root,"result") && mutate(root,mode,f));xide_edit_reply* reply=xideEditCallReply(call,xideLanguageClientIdentity(client),root,bytes,NULL);CHECK(reply && xideEditReplyCall(reply)==call && xideEditReplyCandidate(reply));
#ifdef XIDE_CANDIDATE_FAULT_TEST
 if(mode==11){probeThread=GetCurrentThreadId();probeCalls=probeFail=0;probing=1;}
 if(mode==12){probeThread=GetCurrentThreadId();failedTouches=0;denyTouch=1;}
#endif
 xide_edit_candidate* candidate=xideEditCandidatePrepare(call,reply,0,mode==10?stopNow:NULL,NULL,&error);
#ifdef XIDE_CANDIDATE_FAULT_TEST
 if(mode==11){probing=0;size_t observed=probeCalls;CHECK(candidate && observed);for(size_t i=1;i<=observed;++i){probeCalls=0;probeFail=i;probing=1;xide_edit_candidate* bad=xideEditCandidatePrepare(call,reply,0,NULL,NULL,&error);probing=0;CHECK(!bad && probeCalls==i && xideEditCandidateCurrent(candidate));}probeFail=0;CHECK(xideEditCandidateValidate(candidate,NULL,NULL,&error));printf("observed admission map allocation/refusal points: %zu\n",observed);}
 if(mode==12){denyTouch=0;CHECK(failedTouches==1 && !candidate);}
#endif
 if(valid && !candidate)fprintf(stderr,"candidate prepare: %s\n",error.message);
 CHECK((candidate!=NULL)==valid);
 if(candidate){xide_workspace_edit_info info;CHECK(xideEditCandidateCurrent(candidate) && xideEditCandidateCall(candidate)==call && xideWorkspaceEditInfo(xideEditCandidatePlan(candidate),&info) && info.documents==5 && info.edits==5 && info.changedDocuments==5 && xideEditCandidateValidate(candidate,NULL,NULL,&error));size_t opened=0,bomDisk=0;
  for(size_t i=0;i<info.documents;++i){xide_workspace_edit_document document;CHECK(xideWorkspaceEditDocumentAt(xideEditCandidatePlan(candidate),i,&document) && strstr(xlsTextData(document.before),"compute") && strstr(xlsTextData(document.after),"renamed_😀"));if(document.hasVersion){++opened;CHECK(document.identity==21 && document.revision==3);}else if(document.proof.rawSize>xlsTextSize(document.before))++bomDisk;}
  CHECK(opened==1 && bomDisk==1);
 }
 xideEditReplyRelease(reply);xideEditCallRelease(call);xideEditRequestRelease(request);if(keep)*keep=candidate;else xideEditCandidateRelease(candidate);return 1;
}
static int run(const char* directory,const char* server)
{
 fixture f={.modelsCount=1,.request=100};CHECK(xideIoCreateDirectories(directory));const char* names[]={"main.xl","second.xl","shared.xl","unopened.xl","isolated.xl","solo.xl","unused.c","absent-xl.exe"};
 for(size_t i=0;i<8;++i){f.paths[i]=xidePathResolve(directory,names[i]);CHECK(f.paths[i]);}
 const char* main="import shared\nfunc main(...) as int\n return shared.compute(1)\nend\n";const char* use="import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n";const char* shared="\xef\xbb\xbf" "export func compute(int value) as int\r\n return value\r\nend\r\n";const char* unrelated="func main(...) as int\n return 0\nend\n";f.body=main;
 const char* bodies[]={main,use,shared,use,unrelated,use};for(size_t i=0;i<6;++i)CHECK(xideIoWriteAtomic(f.paths[i],(xbytesview){(const unsigned char*)bodies[i],strlen(bodies[i])}));
 f.uri=xideLspPathToUri(f.paths[0],NULL);f.catalog=configuration(directory,&f);f.sync=xlsSyncCreateMonotonic(2);f.loose=(xide_file){.path=f.paths[5]};f.files=(xide_files){&f.loose,1,1};f.models[0]=(xide_edit_request_model){21,3,1,0,0,f.paths[0],"xlang"};CHECK(f.uri && f.catalog && f.sync);
 client=xideLanguageClientStart(server,f.paths[7],directory,NULL);CHECK(client);xvalue* params=xrtJsonParse(xrtStrView("{\"capabilities\":{},\"initializationOptions\":{\"checkOnSave\":false,\"automaticDiagnostics\":false}}"));CHECK(params && sendRequest("initialize",params,1));xrtValueRelease(params);size_t bytes=0;xvalue* reply=receive(1,&bytes);CHECK(reply && get(reply,"result"));xrtValueRelease(reply);
 params=xrtValueObject();CHECK(params && put(params,"epoch",xrtValueInt(1)) && put(params,"catalog",xrtValueRetain(f.catalog)) && sendRequest("xlang/configureAnalysis",params,2));xrtValueRelease(params);reply=receive(2,&bytes);CHECK(reply && get(reply,"result"));xrtValueRelease(reply);
 xls_sync_document document={f.uri,main,strlen(main),21,3};CHECK(xlsSyncUpdate(f.sync,&document,1,emit,NULL));params=xrtValueObject();CHECK(params && sendRequest("xlang/inputState",params,3));xrtValueRelease(params);reply=receive(3,&bytes);CHECK(reply && get(reply,"result"));char digest[65];size_t documents=0;xstrview online={0};CHECK(xlsSyncInputState(f.sync,digest,&documents) && xrtValueGetString(get(get(reply,"result"),"sha256"),&online) && !strcmp(digest,online.Data) && documents==1);xrtValueRelease(reply);
 CHECK(attempt(&f,0,1,NULL));for(int mode=1;mode<=10;++mode)CHECK(attempt(&f,mode,0,NULL));
 CHECK(attempt(&f,13,0,NULL));
#ifdef XIDE_CANDIDATE_FAULT_TEST
 CHECK(attempt(&f,11,1,NULL));
#endif
 f.modelsCount=2;f.models[1]=(xide_edit_request_model){22,1,1,1,0,f.paths[2],"xlang"};CHECK(attempt(&f,0,0,NULL));f.models[1].language="C";CHECK(attempt(&f,0,0,NULL));
#ifdef XIDE_CANDIDATE_FAULT_TEST
 CHECK(attempt(&f,12,0,NULL));
#endif
 f.models[1].path=f.paths[6];CHECK(attempt(&f,0,1,NULL));f.modelsCount=1;f.models[0].external=1;CHECK(attempt(&f,0,0,NULL));f.models[0].external=0;
 xide_edit_candidate* kept=NULL;CHECK(attempt(&f,0,1,&kept) && kept);xide_workspace_edit* retained=xideEditCandidateRetainPlan(kept);CHECK(retained && xideIoWriteAtomic(f.paths[4],(xbytesview){(const unsigned char*)unrelated,strlen(unrelated)}) && !xideEditCandidateValidate(kept,NULL,NULL,NULL) && !xideEditCandidateCurrent(kept));xideEditCandidateRelease(kept);CHECK(xideWorkspaceEditInfo(retained,&(xide_workspace_edit_info){0}));xideWorkspaceEditRelease(retained);
 CHECK(attempt(&f,0,1,&kept));xideEditCallCancel((xide_edit_call*)xideEditCandidateCall(kept));CHECK(!xideEditCandidateCurrent(kept) && !xideEditCandidateValidate(kept,NULL,NULL,NULL));xideEditCandidateRelease(kept);
 xideLanguageClientStop(client);client=NULL;xlsSyncDestroy(f.sync);xrtValueRelease(f.catalog);free(f.uri);for(size_t i=0;i<8;++i)free(f.paths[i]);return 1;
}
int main(int argc,char** argv){if(argc!=3)return 2;char* root=xidePathResolve(argv[1],"candidate-源码😀");int okay=root&&run(root,argv[2]);free(root);if(!okay)return 1;printf("PASS native candidate data admission: %u checks; real routed RPC, all provider/member coverage, five normalized open/closed targets, exact intent, mismatched proofs/edits/models rejected, stale/cancelled results and detached plan\n",checks);return 0;}
