#include "services/c_commands.h"
#include "services/c_language.c"
#include "platform/text_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { ++assertions; if(!(v)) { fprintf(stderr,"c_language_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static int assertions,diagnosticEvents; static xvalue* entryDiagnostics; static volatile LONG wakes;
static void wake(void* user) { InterlockedIncrement((volatile LONG*)user); }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static xvalue* nextTransport(xide_language_client* client,xide_model_error* error)
{ uint64_t until=GetTickCount64()+10000; do { xvalue* m=xideLanguageClientNext(client); if(m) return m; if(xideLanguageClientState(client,error->message,sizeof(error->message))>=XIDE_LS_STOPPED) break; Sleep(5); } while(GetTickCount64()<until); return NULL; }
static int waitEvent(xide_c_session* session,enum xide_c_event_kind kind,int id,xide_c_event* event,xide_model_error* error)
{
 uint64_t until=GetTickCount64()+15000;
 while(GetTickCount64()<until) {
  if(xideCSessionNext(session,event)) {
   if(event->kind==XIDE_C_DIAGNOSTICS) { ++diagnosticEvents; if(event->documentId==101) { xrtValueRelease(entryDiagnostics); entryDiagnostics=xrtValueDeepClone(event->message); } }
   if(event->kind==kind && (!id || event->requestId==id)) return 1;
   xideCEventClear(event);
  } else if(xideCSessionState(session,error->message,sizeof(error->message))>=XIDE_C_FAILED) return 0;
  Sleep(5);
 } snprintf(error->message,sizeof(error->message),"Timed out waiting for C event %d/%d",kind,id); return 0;
}
static int dump(const char* root,const char* name,const xvalue* v)
{ char* path=xidePathResolve(root,name); size_t n=0; char* text=xrtJsonStringify(v,true,&n); int ok=path && text && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,n}); free(path); xrtFree(text); return ok; }
static const char* findText(const xvalue* v,const char* key) { return cstring(cget(v,key)); }
static int contains(const xvalue* v,const char* text) { size_t n=0; char* json=xrtJsonStringify(v,false,&n); int okay=json && strstr(json,text)!=NULL; xrtFree(json); return okay; }
static int argumentsContain(const xvalue* args,const char* text)
{ for(size_t i=0;i<xrtValueCount(args);++i) { const char* s=cstring(xrtValueArrayGet(args,i)); if(s && !strcmp(s,text)) return 1; } return 0; }
static char* peer(const char* file)
{ wchar_t path[32768]; char text[131072]; if(!GetModuleFileNameW(NULL,path,32768) || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path,-1,text,sizeof(text),NULL,NULL)) return NULL; char* directory=xidePathDirectory(text); char* result=directory ? xidePathResolve(directory,file) : NULL; free(directory); return result; }
static int rangeTest(const char* text,const char* json,int expected)
{ c_input doc={.text=(char*)text}; xvalue* list=xrtJsonParse(csv(json)); int ok=list && cdiagnostics(&doc,list)==expected; xrtValueRelease(list); return ok; }
static int envelopeTest(const char* json,int expected)
{ xvalue* v=xrtJsonParse(csv(json)); int ok=v && cenvelope(v)==expected; xrtValueRelease(v); return ok; }
int main(int argc,char** argv)
{
 int failed=0,real=0; xide_model_error error={0}; xide_project project={0}; xvalue *commands=NULL,*m=NULL,*bad=NULL,*snapshot=NULL; xide_c_session *session=NULL,*old=NULL; xide_language_client* client=NULL; xide_c_event event={0}; char* owned[32]={0}; size_t ownedCount=0;
 #define OWN(x) (owned[ownedCount++]=(x))
 CHECK(argc>=2 && argc<=3); char unique[100]; snprintf(unique,sizeof(unique),"c-language-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 char *root=OWN(xidePathResolve(argv[1],unique)),*include=OWN(xidePathResolve(root,"包含 目录😀")),*manifest=OWN(xidePathResolve(root,"项目.xlp")),*entry=OWN(xidePathResolve(root,"入口😀.c")),*header=OWN(xidePathResolve(include,"types.h")),*xl=OWN(xidePathResolve(root,"main.xl")),*mode=OWN(xidePathResolve(root,"c-language-mode.txt")),*fixture=OWN(peer("c_language_program.exe"));
 CHECK(root && include && manifest && entry && header && xl && mode && fixture && xrtDirCreateAll(include));
 const char* diskHeader="typedef struct Box { int field; const char* name; } Box;\n";
 const char* diskEntry="#include \"types.h\"\nint main(void) { Box value={0}; return value.field; }\n";
 CHECK(writeText(header,diskHeader) && writeText(entry,diskEntry) && writeText(xl,"func main(...) as int\n return 0\nend\n"));
 CHECK(xideProjectCreate(&project,manifest,"C语义",xl,&error) && xideProjectAddFile(&project,entry,"c",&error) && xideProjectAddFile(&project,header,"header",&error));
 snapshot=xideProjectConfigurationsJson(&project); CHECK(snapshot && xrtValueCount(snapshot)==1); xvalue* paths=xrtValueArray(); CHECK(paths && xrtValueArrayAppendNew(paths,xrtValueString(csv(include))) && cput(xrtValueArrayGet(snapshot,0),"includePaths",paths));
 CHECK(xideProjectApplyConfigurations(&project,snapshot,project.activeConfiguration,&error)); xrtValueRelease(snapshot); snapshot=NULL;
 commands=xideCCommandsCreate(&project,NULL,NULL,&error); CHECK(commands && xrtValueCount(commands)==2); xvalue* args=cget(xrtValueArrayGet(commands,0),"arguments");
 CHECK(argumentsContain(args,"-std=c11") && argumentsContain(args,"-D_CRT_SECURE_NO_WARNINGS") && argumentsContain(args,include) && argumentsContain(args,entry));
 CHECK(!xideCCommandsCreate(&project,"missing",NULL,&error)); xide_c_compile_profile wrong={.driver=""}; CHECK(!xideCCommandsCreate(&project,NULL,&wrong,&error));
 const char* systems[]={include}; xide_c_compile_profile profile={.driver="clang",.target="x86_64-w64-windows-gnu",.prelude=header,.systemIncludes=systems,.systemIncludeCount=1};
 snapshot=xideCCommandsCreate(&project,NULL,&profile,&error); CHECK(snapshot && argumentsContain(cget(xrtValueArrayGet(snapshot,0),"arguments"),"-include") && argumentsContain(cget(xrtValueArrayGet(snapshot,0),"arguments"),"-isystem")); xrtValueRelease(snapshot); snapshot=NULL;
 xide_language_client_spec invalid={.program=fixture,.directory=root,.argumentCount=65}; CHECK(!xideLanguageClientStartProcess(&invalid,&error)); invalid.argumentCount=0; invalid.program="\xff"; CHECK(!xideLanguageClientStartProcess(&invalid,&error));
 char literal[]="中文 😀 quote\" slash\\ tail\\"; const char* literalArgs[]={"",literal,"-looks-like-an-option"}; xide_language_client_spec spec={fixture,root,literalArgs,3};
 client=xideLanguageClientStartProcess(&spec,&error); CHECK(client); xideLanguageClientSetWake(client,wake,(void*)&wakes); literal[0]='X'; m=nextTransport(client,&error);
 if(m) CHECK(dump(root,"transport-arguments.json",m));
 CHECK(m && !strcmp(findText(m,"method"),"transport.arguments") && xrtValueCount(cget(m,"params"))==3 && !strcmp(cstring(xrtValueArrayGet(cget(m,"params"),0)),"") && !strcmp(cstring(xrtValueArrayGet(cget(m,"params"),1)),"中文 😀 quote\" slash\\ tail\\") && !strcmp(cstring(xrtValueArrayGet(cget(m,"params"),2)),"-looks-like-an-option"));
 CHECK(InterlockedCompareExchange(&wakes,0,0)>0); xrtValueRelease(m);m=NULL;xideLanguageClientStop(client);client=NULL;
 CHECK(rangeTest("😀a\n","[{\"message\":\"real\",\"range\":{\"start\":{\"line\":0,\"character\":2},\"end\":{\"line\":0,\"character\":3}}}]",1));
 CHECK(rangeTest("😀a\n","[{\"message\":\"bad\",\"range\":{\"start\":{\"line\":0,\"character\":1},\"end\":{\"line\":0,\"character\":3}}}]",0));
 CHECK(rangeTest("a\n","[{\"message\":\"bad\",\"range\":{\"start\":{\"line\":9,\"character\":0},\"end\":{\"line\":9,\"character\":1}}}]",0));
 CHECK(rangeTest("a\n","[{\"message\":\"\",\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}}}]",0));
 CHECK(envelopeTest("{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":null}",1));
 CHECK(envelopeTest("{\"jsonrpc\":\"1.0\",\"id\":2,\"result\":null}",0));
 CHECK(envelopeTest("{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":null,\"error\":{\"code\":-1,\"message\":\"bad\"}}",0));
 CHECK(envelopeTest("{\"jsonrpc\":\"2.0\",\"method\":false}",0));
 CHECK(envelopeTest("{\"jsonrpc\":\"2.0\",\"method\":\"unknown\",\"params\":null}",0));
 CHECK(envelopeTest("{\"jsonrpc\":\"2.0\",\"id\":\"peer\",\"method\":\"unknown\",\"params\":{}}",1));
 CHECK(envelopeTest("{\"jsonrpc\":\"2.0\",\"id\":2,\"error\":{\"code\":\"bad\",\"message\":\"bad\"}}",0));
 char overlayHeader[]="typedef struct Box { double field; const char* name; } Box;\n";
 const char* overlayEntry="#include \"types.h\"\n/* 中文😀 */ int main(void) { Box value={0}; return value.fiOther; }\n";
 xide_c_document docs[]={{101,entry,overlayEntry,11},{102,header,overlayHeader,22}};
 CHECK(!xideCSessionStart(fixture,"relative",commands,docs,2,&error));
 const char* savedPath=docs[1].path; docs[1].path="relative.h"; CHECK(!xideCSessionStart(fixture,root,commands,docs,2,&error)); docs[1].path=savedPath;
 bad=xrtValueDeepClone(commands); CHECK(bad && cput(xrtValueArrayGet(bad,0),"directory",xrtValueString(csv("relative"))) && !xideCSessionStart(fixture,root,bad,docs,2,&error)); xrtValueRelease(bad); bad=NULL;
 char* expanded=OWN(malloc(2u*1024u*1024u+1)); CHECK(expanded); memset(expanded,1,2u*1024u*1024u); expanded[2u*1024u*1024u]=0; const char* savedEntry=docs[0].text; docs[0].text=expanded;
 CHECK(!xideCSessionStart(fixture,root,commands,docs,2,&error)); docs[0].text=savedEntry;
 CHECK(!xideCSessionStart(fixture,root,commands,docs,65,&error)); docs[1].id=101; CHECK(!xideCSessionStart(fixture,root,commands,docs,2,&error)); docs[1].id=102;
 const char* saved=docs[1].text; docs[1].text="\xff"; CHECK(!xideCSessionStart(fixture,root,commands,docs,2,&error)); docs[1].text=saved;
 bad=xrtValueDeepClone(commands); CHECK(bad && xrtValueArrayAppend(bad,xrtValueArrayGet(commands,0)) && !xideCSessionStart(fixture,root,bad,docs,2,&error)); xrtValueRelease(bad); bad=NULL;
 CHECK(writeText(mode,"bad encoding")); session=xideCSessionStart(fixture,root,commands,docs,2,&error); CHECK(session);
 CHECK(!waitEvent(session,XIDE_C_INITIALIZED,0,&event,&error) && xideCSessionState(session,NULL,0)==XIDE_C_FAILED);
 CHECK(!session->draftDirectory && !session->draftFileCount); xideCSessionDestroy(session);session=NULL; CHECK(xrtFileDelete(mode));
 CHECK(writeText(mode,"notification flood")); session=xideCSessionStart(fixture,root,commands,docs,2,&error); CHECK(session && waitEvent(session,XIDE_C_INITIALIZED,1,&event,&error)); xideCEventClear(&event);
 CHECK(xideCSessionSupports(session,XIDE_C_COMPLETION) && xideCSessionSupports(session,XIDE_C_HOVER) && xideCSessionSupports(session,XIDE_C_SYMBOLS) && xideCSessionSupports(session,XIDE_C_DEFINITION) && !xideCSessionSupports(session,(enum xide_c_query)99));
 xideCSessionSetWake(session,wake,(void*)&wakes); Sleep(200); CHECK(xideLanguageClientHasPending(session->client)); InterlockedExchange(&wakes,0);
 CHECK(!xideCSessionNext(session,&event) && xideLanguageClientHasPending(session->client) && InterlockedCompareExchange(&wakes,0,0)>0);
 int drains=1; while(xideLanguageClientHasPending(session->client) && drains<10) { CHECK(!xideCSessionNext(session,&event)); ++drains; }
 CHECK(drains>=4 && drains<10 && xideCSessionState(session,NULL,0)==XIDE_C_READY);
 int pendingIds[64]; for(int i=0;i<64;++i) { pendingIds[i]=xideCSessionRequest(session,XIDE_C_SYMBOLS,101,0,&error); CHECK(pendingIds[i]>1); }
 CHECK(!xideCSessionRequest(session,XIDE_C_SYMBOLS,101,0,&error)); for(int i=0;i<64;++i) CHECK(xideCSessionCancel(session,pendingIds[i]));
 CHECK(!xideCSessionCancel(session,pendingIds[0])); xideCSessionDestroy(session);session=NULL; CHECK(xrtFileDelete(mode));
 if(argc==3) {
  memset(&error,0,sizeof(error)); real=1; session=xideCSessionStart(argv[2],root,commands,docs,2,&error); CHECK(session); uint64_t generation=xideCSessionGeneration(session); CHECK(generation);
  xideCSessionSetWake(session,wake,(void*)&wakes); overlayHeader[0]='X';
  CHECK(waitEvent(session,XIDE_C_INITIALIZED,1,&event,&error) && event.generation==generation && session->supported[0] && session->supported[1] && session->supported[2] && session->supported[3]); CHECK(dump(root,"initialize.json",event.message)); xideCEventClear(&event);
  CHECK(!strncmp(session->documents[1].text,"typedef",7)); xvalue* originalArguments=cget(xrtValueArrayGet(commands,0),"arguments"); CHECK(xrtValueArraySetNew(originalArguments,0,xrtValueString(csv("caller-mutated-driver"))));
  CHECK(!strcmp(cstring(xrtValueArrayGet(cget(cget(session->commands,session->documents[0].path),"compilationCommand"),0)),"clang")); CHECK(xrtValueArraySetNew(originalArguments,0,xrtValueString(csv("clang"))));
  int offset=(int)(strstr(overlayEntry,"fiOther")-overlayEntry)+2;
  int request=xideCSessionRequest(session,XIDE_C_COMPLETION,101,offset,&error); CHECK(request>1 && waitEvent(session,XIDE_C_RESULT,request,&event,&error) && event.version==11 && event.documentId==101 && event.generation==generation);
  CHECK(dump(root,"completion.json",event.message) && dump(root,"commands.json",commands));
  CHECK(!cget(event.message,"error") && contains(cget(event.message,"result"),"field") && contains(cget(event.message,"result"),"double") && dump(root,"completion.json",event.message));
  bad=xrtValueDeepClone(event.message); xvalue* items=cget(cget(bad,"result"),"items"); CHECK(items && xrtValueCount(items)); c_request sampled={request,0,XIDE_C_COMPLETION};
  CHECK(cput(xrtValueArrayGet(items,0),"insertTextFormat",xrtValueInt(2)) && !cresult(session,&sampled,bad)); xrtValueRelease(bad);bad=NULL;
  bad=xrtValueDeepClone(event.message); items=cget(cget(bad,"result"),"items"); xvalue* first=xrtValueArrayGet(items,0); CHECK(cput(cget(cget(cget(first,"textEdit"),"range"),"start"),"character",xrtValueInt(99999)) && !cresult(session,&sampled,bad)); xrtValueRelease(bad);bad=NULL; xideCEventClear(&event);
  request=xideCSessionRequest(session,XIDE_C_DEFINITION,101,(int)(strstr(overlayEntry,"Box value")-overlayEntry)+1,&error); CHECK(request>1 && waitEvent(session,XIDE_C_RESULT,request,&event,&error));
  CHECK(!cget(event.message,"error") && contains(cget(event.message,"result"),"types.h") && dump(root,"definition.json",event.message)); xideCEventClear(&event);
  request=xideCSessionRequest(session,XIDE_C_HOVER,101,(int)(strstr(overlayEntry,"value={0}")-overlayEntry)+1,&error); CHECK(request>1 && waitEvent(session,XIDE_C_RESULT,request,&event,&error));
  CHECK(!cget(event.message,"error") && contains(cget(event.message,"result"),"Box") && dump(root,"hover.json",event.message)); xideCEventClear(&event);
  request=xideCSessionRequest(session,XIDE_C_SYMBOLS,101,0,&error); CHECK(request>1 && waitEvent(session,XIDE_C_RESULT,request,&event,&error));
  CHECK(!cget(event.message,"error") && contains(cget(event.message,"result"),"main") && dump(root,"symbols.json",event.message));
  bad=xrtValueDeepClone(event.message); sampled=(c_request){request,0,XIDE_C_SYMBOLS}; CHECK(cput(cget(cget(xrtValueArrayGet(cget(bad,"result"),0),"selectionRange"),"end"),"character",xrtValueInt(99999)) && !cresult(session,&sampled,bad)); xrtValueRelease(bad);bad=NULL;xideCEventClear(&event);
  uint64_t until=GetTickCount64()+10000; while(!entryDiagnostics && GetTickCount64()<until) { if(xideCSessionNext(session,&event)) { if(event.kind==XIDE_C_DIAGNOSTICS && event.documentId==101) entryDiagnostics=xrtValueDeepClone(event.message); xideCEventClear(&event); } Sleep(5); }
  CHECK(entryDiagnostics && contains(entryDiagnostics,"fiOther") && dump(root,"diagnostics.json",entryDiagnostics));
  request=xideCSessionRequest(session,XIDE_C_HOVER,101,offset,&error); CHECK(request>1 && xideCSessionCancel(session,request)); until=GetTickCount64()+500;
  while(GetTickCount64()<until) { if(xideCSessionNext(session,&event)) { CHECK(event.kind!=XIDE_C_RESULT || event.requestId!=request);xideCEventClear(&event); } Sleep(5); }
  CHECK(!xideCSessionRequest(session,XIDE_C_HOVER,999,0,&error) && !xideCSessionRequest(session,XIDE_C_HOVER,101,99999,&error));
  const char* emoji=strstr(overlayEntry,"😀"); CHECK(emoji && !xideCSessionRequest(session,XIDE_C_HOVER,101,(int)(emoji-overlayEntry)+1,&error));
  old=session; session=NULL; xideCSessionStop(old); CHECK(xideCSessionState(old,NULL,0)==XIDE_C_STOPPED && !xideCSessionRequest(old,XIDE_C_HOVER,101,0,&error));
  docs[0].text="#include \"types.h\"\n/* 😀 */ int main(void) { Box value={0}; return value.field!=0; }\n"; docs[0].version=12; docs[1].text="typedef struct Box { const char* field; } Box;\n"; docs[1].version=23;
  session=xideCSessionStart(argv[2],root,commands,docs,2,&error); CHECK(session && xideCSessionGeneration(session)!=generation && waitEvent(session,XIDE_C_INITIALIZED,1,&event,&error)); xideCEventClear(&event);
  offset=(int)(strstr(docs[0].text,"field")-docs[0].text)+1;
  request=xideCSessionRequest(session,XIDE_C_HOVER,101,offset,&error); CHECK(request>1 && waitEvent(session,XIDE_C_RESULT,request,&event,&error) && event.version==12 && event.generation==xideCSessionGeneration(session));
  CHECK(!cget(event.message,"error") && contains(cget(event.message,"result"),"char") && dump(root,"changed-header-hover.json",event.message)); xideCEventClear(&event);
  xideCSessionDestroy(old);old=NULL;xideCSessionDestroy(session);session=NULL;
  char* leaf=OWN(xidePathResolve(include,"leaf.h")); CHECK(leaf && writeText(leaf,"#define FIELD_TYPE int\n") && xideProjectAddFile(&project,leaf,"header",&error));
  xrtValueRelease(commands); commands=xideCCommandsCreate(&project,NULL,NULL,&error); CHECK(commands && xrtValueCount(commands)==3);
  docs[1].text="#include \"leaf.h\"\ntypedef struct Box { FIELD_TYPE field; } Box;\n"; docs[1].version=24;
  xide_c_document graph[]={docs[0],docs[1],{103,leaf,"#define FIELD_TYPE double\n",31}};
  session=xideCSessionStart(argv[2],root,commands,graph,3,&error); CHECK(session && waitEvent(session,XIDE_C_INITIALIZED,1,&event,&error)); xideCEventClear(&event);
  offset=(int)(strstr(docs[1].text,"field")-docs[1].text)+1;
  request=xideCSessionRequest(session,XIDE_C_HOVER,102,offset,&error); CHECK(request>1 && waitEvent(session,XIDE_C_RESULT,request,&event,&error) && event.documentId==102 && event.version==24);
  CHECK(dump(root,"unsaved-header-chain-hover.json",event.message));
  CHECK(!cget(event.message,"error") && contains(cget(event.message,"result"),"double")); xideCEventClear(&event);
  xideCSessionDestroy(session);session=NULL;
  xide_text_file leafOriginal={0}; CHECK(xideTextFileRead(leaf,&leafOriginal,&error) && leafOriginal.rawLength==strlen("#define FIELD_TYPE int\n") && !memcmp(leafOriginal.raw,"#define FIELD_TYPE int\n",leafOriginal.rawLength)); xideTextFileClear(&leafOriginal);

 }
 xide_text_file original={0}; CHECK(xideTextFileRead(header,&original,&error) && !strcmp(original.text,diskHeader) && original.rawLength==strlen(diskHeader) && !memcmp(original.raw,diskHeader,original.rawLength)); xideTextFileClear(&original); CHECK(xideTextFileRead(entry,&original,&error) && !strcmp(original.text,diskEntry) && original.rawLength==strlen(diskEntry) && !memcmp(original.raw,diskEntry,original.rawLength));xideTextFileClear(&original);
 printf("c_language_test passed: %d assertions; immutable project commands, Unicode/empty literal arguments, isolated stderr, UTF-16, range/capacity/identity/encoding rejection%s; evidence: %s.\n",assertions,real ? ", real clangd diagnostics/completion/definition/hover/symbols, unsaved header types, cancellation and new-session resynchronization" : " (real C server gate omitted)",root);
cleanup:
 xideCEventClear(&event);xideCSessionDestroy(session);xideCSessionDestroy(old);xideLanguageClientStop(client);xideProjectClear(&project);xrtValueRelease(commands);xrtValueRelease(m);xrtValueRelease(bad);xrtValueRelease(snapshot);xrtValueRelease(entryDiagnostics);entryDiagnostics=NULL;
 for(size_t i=0;i<ownedCount;++i) { free(owned[i]); } return failed;
}
