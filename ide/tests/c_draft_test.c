#include "services/c_commands.h"
#include "services/c_language.c"
#include "platform/text_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { ++assertions; if(!(v)) { fprintf(stderr,"c_draft_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static int assertions;
static int writeText(const char* p,const char* t) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)t,strlen(t)}); }
static int dump(const char* root,const char* name,const xvalue* value)
{ char* path=xidePathResolve(root,name); size_t n=0; char* json=xrtJsonStringify(value,true,&n); int ok=path && json && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)json,n}); free(path);xrtFree(json);return ok; }
static int contains(const xvalue* v,const char* expected)
{ size_t n=0; char* json=xrtJsonStringify(v,false,&n); int ok=json && strstr(json,expected);xrtFree(json);return ok; }
static int waitResult(xide_c_session* s,int id,xide_c_event* event)
{ uint64_t until=GetTickCount64()+15000; while(GetTickCount64()<until) { if(xideCSessionNext(s,event)) { if(event->kind==XIDE_C_RESULT && event->requestId==id) return 1; xideCEventClear(event); } if(xideCSessionState(s,NULL,0)>=XIDE_C_FAILED) return 0;Sleep(5); } return 0; }
static int deniesWrite(const char* path)
{ wchar_t* w=cwide(path);HANDLE h=w ? CreateFileW(w,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL) : INVALID_HANDLE_VALUE;free(w);if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);return h==INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION; }
static int absent(const char* path)
{ wchar_t* w=cwide(path);DWORD attributes=w ? GetFileAttributesW(w) : 0;free(w);return attributes==INVALID_FILE_ATTRIBUTES; }
/* Delay all dependent didOpen notifications until after a real semantic reply.
   This establishes a deterministic startup order, without simulating types or
   relying on sleeps to win an asynchronous preamble race. */
static int openOnly(xide_c_session* s,size_t index)
{
 uint64_t until=GetTickCount64()+15000; xvalue* message=NULL;
 while(!message && GetTickCount64()<until) { message=xideLanguageClientNext(s->client);if(!message) Sleep(5); }
 const xvalue* caps=cget(cget(message,"result"),"capabilities"); int id=0,traceVersion=0;
 const char* encoding=cstring(cget(caps,"positionEncoding"));
 const xvalue* trace=cget(cget(caps,"experimental"),"xlangInputTrace");
 int ok=message && cint(cget(message,"id"),&id) && id==1 && !cget(message,"error") && caps && encoding && !strcmp(encoding,"utf-16");
 /* This helper deliberately delays other didOpen messages. Keep the real
    negotiated trace capability when replacing that startup sequence. */
 ok=ok && (!trace || (cint(trace,&traceVersion) && traceVersion==1));
 if(ok) { s->tracing=traceVersion==1;s->supported[XIDE_C_HOVER]=cprovider(cget(caps,"hoverProvider"));s->supported[XIDE_C_COMPLETION]=cprovider(cget(caps,"completionProvider"));ok=csend(s,"initialized",xrtValueObject(),0) && csend(s,"textDocument/didOpen",copenMessage(&s->documents[index]),0); }
 xrtValueRelease(message); if(ok) s->state=XIDE_C_READY; return ok;
}
int main(int argc,char** argv)
{
 int failed=0; xide_model_error error={0}; xide_project project={0}; xvalue *commands=NULL,*originalCommands=NULL,*userVfs=NULL; xide_c_session *session=NULL,*other=NULL; xide_c_event event={0};
 char *root=NULL,*parent=NULL,*leaf=NULL,*entry=NULL,*xl=NULL,*manifest=NULL,*cache=NULL,*overlay=NULL,*privateLeaf=NULL,*userPath=NULL,*userLeaf=NULL,*missing=NULL; char unique[120];
 CHECK(argc==3);snprintf(unique,sizeof(unique),"c-draft-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 root=xidePathResolve(argv[1],unique);parent=xidePathResolve(root,"types 空间😀.h");leaf=xidePathResolve(root,"leaf 空间😀.h");entry=xidePathResolve(root,"入口😀.c");xl=xidePathResolve(root,"main.xl");manifest=xidePathResolve(root,"项目.xlp");
 CHECK(root && parent && leaf && entry && xl && manifest && xrtDirCreateAll(root));
 const char* diskLeaf="#define FIELD_TYPE int\n";
 const char* parentText="#include \"leaf 空间😀.h\"\ntypedef struct Box { FIELD_TYPE field; } Box;\n";
 const char* entryText="#include \"types 空间😀.h\"\n/* 😀 */ double inspect(Box value) { return value.field; }\n";
 CHECK(writeText(parent,parentText) && writeText(leaf,diskLeaf) && writeText(entry,entryText) && writeText(xl,"func main(...) as int\n return 0\nend\n"));
 CHECK(xideProjectCreate(&project,manifest,"C 草稿快照",xl,&error) && xideProjectAddFile(&project,parent,"header",&error) && xideProjectAddFile(&project,leaf,"header",&error) && xideProjectAddFile(&project,entry,"c",&error));
 commands=xideCCommandsCreate(&project,NULL,NULL,&error);CHECK(commands);originalCommands=xrtValueDeepClone(commands);CHECK(originalCommands);
 char mutableLeaf[]="#define FIELD_TYPE double\n";
 xide_c_document docs[]={{102,parent,parentText,24},{103,leaf,mutableLeaf,31},{101,entry,entryText,12}};
 session=xideCSessionStart(argv[2],root,commands,docs,3,&error);CHECK(session && openOnly(session,0));
 CHECK(xrtValueEqual(commands,originalCommands) && session->draftFileCount==4);
 cache=ccopy(session->draftDirectory);overlay=xidePathResolve(cache,"drafts.vfs.json");privateLeaf=xidePathResolve(cache,"draft-1.txt");CHECK(cache && overlay && privateLeaf && deniesWrite(overlay) && deniesWrite(privateLeaf));
 mutableLeaf[0]='X'; /* Caller edits cannot alter the copied VFS or didOpen. */
 int offset=(int)(strstr(parentText,"field")-parentText)+1;
 int request=xideCSessionRequest(session,XIDE_C_HOVER,102,offset,&error);CHECK(request>1 && waitResult(session,request,&event));
 CHECK(dump(root,"parent-before-leaf-didOpen.json",event.message));
 CHECK(!cget(event.message,"error") && contains(cget(event.message,"result"),"double"));xideCEventClear(&event);
 CHECK(csend(session,"textDocument/didOpen",copenMessage(&session->documents[1]),0) && csend(session,"textDocument/didOpen",copenMessage(&session->documents[2]),0));
 request=xideCSessionRequest(session,XIDE_C_HOVER,101,(int)(strstr(entryText,"field")-entryText)+1,&error);CHECK(request>1 && waitResult(session,request,&event));
 CHECK(dump(root,"source-after-leaf-didOpen.json",event.message) && !cget(event.message,"error") && contains(cget(event.message,"result"),"double"));xideCEventClear(&event);
 xideCSessionStop(session);CHECK(absent(cache) && absent(overlay) && absent(privateLeaf));xideCSessionDestroy(session);session=NULL;
 mutableLeaf[0]='#';
 session=xideCSessionStartCompletion(argv[2],root,commands,docs,3,&error);CHECK(session && openOnly(session,2));
 request=xideCSessionRequest(session,XIDE_C_COMPLETION,101,(int)(strstr(entryText,"value.field")-entryText)+6,&error);CHECK(request>1 && waitResult(session,request,&event));
 CHECK(dump(root,"completion-before-headers-didOpen.json",event.message) && !cget(event.message,"error") && contains(cget(event.message,"result"),"double"));xideCEventClear(&event);
 xideCSessionDestroy(session);session=NULL;
 /* Simultaneous generations cannot share or overwrite private snapshot files. */
 session=xideCSessionStart(argv[2],root,commands,docs,3,&error);CHECK(session && openOnly(session,0));
 docs[1].text="#define FIELD_TYPE long long\n";docs[1].version=32;
 other=xideCSessionStart(argv[2],root,commands,docs,3,&error);CHECK(other && strcmp(session->draftDirectory,other->draftDirectory) && openOnly(other,0));
 request=xideCSessionRequest(other,XIDE_C_HOVER,102,offset,&error);CHECK(request>1 && waitResult(other,request,&event));
 CHECK(dump(root,"parallel-new-generation-hover.json",event.message) && !cget(event.message,"error") && contains(cget(event.message,"result"),"long long"));xideCEventClear(&event);xideCSessionDestroy(other);other=NULL;
 request=xideCSessionRequest(session,XIDE_C_HOVER,102,offset,&error);CHECK(request>1 && waitResult(session,request,&event));
 CHECK(dump(root,"parallel-old-generation-hover.json",event.message) && !cget(event.message,"error") && contains(cget(event.message,"result"),"double"));xideCEventClear(&event);xideCSessionDestroy(session);session=NULL;
 /* An unopened header with no physical file must already exist in the VFS.
    Include guards exercise a cycle; unrelated physical headers still fall
    through to the user's filesystem. */
 missing=xidePathResolve(root,"new 未保存😀.h");CHECK(missing && absent(missing) && xideProjectAddFile(&project,missing,"header",&error));
 const char* cyclicParent="#ifndef TYPES_GUARD\n#define TYPES_GUARD\n#include \"new 未保存😀.h\"\ntypedef struct Box { FIELD_TYPE field; } Box;\n#endif\n";
 const char* cyclicLeaf="#ifndef LEAF_GUARD\n#define LEAF_GUARD\n#include \"types 空间😀.h\"\n#include \"leaf 空间😀.h\"\n#undef FIELD_TYPE\n#define FIELD_TYPE const char*\n#endif\n";
 xrtValueRelease(commands);commands=xideCCommandsCreate(&project,NULL,NULL,&error);CHECK(commands);
 xide_c_document cycle[]={{102,parent,cyclicParent,25},{104,missing,cyclicLeaf,1},{101,entry,entryText,13}};
 session=xideCSessionStart(argv[2],root,commands,cycle,3,&error);CHECK(session && openOnly(session,0));
 request=xideCSessionRequest(session,XIDE_C_HOVER,102,(int)(strstr(cyclicParent,"field")-cyclicParent)+1,&error);CHECK(request>1 && waitResult(session,request,&event));
 CHECK(dump(root,"new-header-include-cycle-hover.json",event.message) && !cget(event.message,"error") && contains(cget(event.message,"result"),"const char"));xideCEventClear(&event);xideCSessionDestroy(session);session=NULL;CHECK(absent(missing));
 /* Explicit user VFS remains intact; the opened editor draft has priority.
    Append our compiler options before '--', preserving literal source paths. */
 userPath=xidePathResolve(root,"user.vfs.json");userLeaf=xidePathResolve(root,"user-leaf.txt");CHECK(userPath && userLeaf && writeText(userLeaf,"#define FIELD_TYPE unsigned long\n"));
 userVfs=xrtValueObject();xvalue *roots=xrtValueArray(),*file=xrtValueObject();CHECK(userVfs && roots && file && cput(file,"type",xrtValueString(csv("file"))) && cput(file,"name",xrtValueString(csv(leaf))) && cput(file,"external-contents",xrtValueString(csv(userLeaf))) && xrtValueArrayAppendNew(roots,file) && cput(userVfs,"version",xrtValueInt(0)) && cput(userVfs,"use-external-names",xrtValueBool(false)) && cput(userVfs,"roots",roots) && dump(root,"user.vfs.json",userVfs));
 for(size_t i=0;i<xrtValueCount(commands);++i) { xvalue* args=cget(xrtValueArrayGet(commands,i),"arguments");size_t last=xrtValueCount(args)-1;const char* input=cstring(xrtValueArrayGet(args,last));char* path=ccopy(input);CHECK(path && xrtValueArraySetNew(args,last,xrtValueString(csv("-ivfsoverlay"))) && xrtValueArrayAppendNew(args,xrtValueString(csv(userPath))) && xrtValueArrayAppendNew(args,xrtValueString(csv("--"))) && xrtValueArrayAppendNew(args,xrtValueString(csv(path))));free(path); }
 xrtValueRelease(originalCommands);originalCommands=xrtValueDeepClone(commands);CHECK(originalCommands);docs[1].text=mutableLeaf;
 session=xideCSessionStartCompletion(argv[2],root,commands,docs,3,&error);CHECK(session && xrtValueEqual(commands,originalCommands) && openOnly(session,2));
 request=xideCSessionRequest(session,XIDE_C_COMPLETION,101,(int)(strstr(entryText,"value.field")-entryText)+6,&error);CHECK(request>1 && waitResult(session,request,&event));
 CHECK(dump(root,"user-vfs-and-double-draft-completion.json",event.message) && !cget(event.message,"error") && contains(cget(event.message,"result"),"double"));xideCEventClear(&event);xideCSessionDestroy(session);session=NULL;
 xide_text_file original={0};CHECK(xideTextFileRead(leaf,&original,&error) && !strcmp(original.text,diskLeaf));xideTextFileClear(&original);
 printf("c_draft_test passed: %d assertions; real clangd before dependent didOpen, Unicode immutable snapshots, simultaneous generations, new headers/cycles, physical fallback, user VFS/-- priority, semantic hover/completion, write leases and stop cleanup; evidence: %s.\n",assertions,root);
cleanup:
 xideCEventClear(&event);xideCSessionDestroy(session);xideCSessionDestroy(other);xrtValueRelease(commands);xrtValueRelease(originalCommands);xrtValueRelease(userVfs);xideProjectClear(&project);free(root);free(parent);free(leaf);free(entry);free(xl);free(manifest);free(cache);free(overlay);free(privateLeaf);free(userPath);free(userLeaf);free(missing);return failed;
}
