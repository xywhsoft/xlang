#include "protocol/lsp_wire.h"
#include "services/language_check.h"
#include "services/language_client.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"lsp_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return xrtValueObjectGet(v,sv(k)); }
static const char* string(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) ? s.Data : NULL; }
static int put(xvalue* v,const char* k,xvalue* x) { return xrtValueObjectSetNew(v,sv(k),x); }
static int number(const xvalue* v) { int64 n=INT64_MIN; return xrtValueGetInt(v,&n) ? (int)n : INT_MIN; }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int received(const xvalue* v,void* data) { int* n=data; if(!string(get(v,"jsonrpc"))) return 0; ++*n; return 1; }
static int invalid(const char* bytes,size_t n) { xide_lsp_stream s={0}; xide_model_error e={0}; int count=0; int okay=!xideLspStreamFeed(&s,bytes,n,received,&count,&e) || !xideLspStreamEnd(&s,&e); xideLspStreamClear(&s); return okay; }
static int waitCheck(xide_language_check* q,xide_model_error* e)
{ uint64_t until=GetTickCount64()+15000; enum xide_check_state state; do { state=xideLanguageCheckState(q,e->message,sizeof(e->message)); if(state!=XIDE_CHECK_RUNNING) return state; Sleep(5); } while(GetTickCount64()<until); return -1; }
static volatile LONG wakeCount;
static void wake(void* user) { InterlockedIncrement((volatile LONG*)user); }
static int waitWake(void) { uint64_t until=GetTickCount64()+2000; while(!InterlockedCompareExchange(&wakeCount,0,0) && GetTickCount64()<until) Sleep(5); return InterlockedCompareExchange(&wakeCount,0,0)>0; }
static int emit(xide_language_client* c,const char* method,xvalue* params,int id,xide_model_error* e)
{ xvalue* m=xrtValueObject(); put(m,"jsonrpc",xrtValueString(sv("2.0"))); put(m,"method",xrtValueString(sv(method))); if(params) put(m,"params",params); if(id) put(m,"id",xrtValueInt(id)); int okay=xideLanguageClientSend(c,m,e); xrtValueRelease(m); return okay; }
static xvalue* next(xide_language_client* c,xide_model_error* e)
{ uint64_t until=GetTickCount64()+15000; do { xvalue* m=xideLanguageClientNext(c); if(m) return m; if(xideLanguageClientState(c,e->message,sizeof(e->message))>=XIDE_LS_STOPPED) break; Sleep(5); } while(GetTickCount64()<until); return NULL; }
static xvalue* params(const char* uri,int version,const char* text)
{ xvalue *p=xrtValueObject(),*d=xrtValueObject(); put(d,"uri",xrtValueString(sv(uri))); if(text) { put(d,"version",xrtValueInt(version)); put(d,"languageId",xrtValueString(sv("xlang"))); put(d,"text",xrtValueString(sv(text))); } put(p,"textDocument",d); put(p,"version",xrtValueInt(version)); return p; }
static int checkRequest(xide_language_client* c,const char* uri,int version,int id,xide_model_error* e) { return emit(c,"xlang/checkSaved",params(uri,version,NULL),id,e); }
static int signal(const char* path) { uint64_t until=GetTickCount64()+5000; do { if(xidePathExists(path)) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
int main(int argc,char** argv)
{
 int failed=0; xide_model_error error={0}; xide_lsp_stream stream={0}; xide_toolchains* cache=NULL; xide_language_check* q=NULL; xide_language_client* c=NULL; xvalue *m=NULL,*out=NULL; char *frame=NULL,*uri=NULL,*round=NULL; size_t length=0; int count=0;
 char* owned[30]={0}; size_t ownedCount=0;
 #define KEEP(p) (owned[ownedCount++]=(p))
 CHECK(argc>=2 && argc<=3); char unique[90]; snprintf(unique,sizeof(unique),"protocol-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 char *root=KEEP(xidePathResolve(argv[1],unique)),*tools=KEEP(xidePathResolve(root,"工具😀")),*compiler=KEEP(xidePathResolve(tools,"xl.exe")),*mode=KEEP(xidePathResolve(root,"language-mode.txt")),*entry=KEEP(xidePathResolve(root,"中文 空格😀.xl")),*checkSignal=KEEP(xidePathResolve(root,"language-signal.txt"));
 wchar_t module[32768]; char binary[131072]; CHECK(GetModuleFileNameW(NULL,module,32768)); wchar_t* slash=wcsrchr(module,L'\\'); CHECK(slash); wcscpy(slash+1,L"xide_language_program.exe"); CHECK(WideCharToMultiByte(CP_UTF8,0,module,-1,binary,sizeof(binary),NULL,NULL));
 char* fixture=KEEP(xidePathResolve(NULL,binary)); wcscpy(slash+1,L"xlang-ls.exe"); CHECK(WideCharToMultiByte(CP_UTF8,0,module,-1,binary,sizeof(binary),NULL,NULL)); char* server=KEEP(xidePathResolve(NULL,binary));
 CHECK(root && tools && compiler && mode && entry && checkSignal && xrtDirCreateAll(tools) && xrtFileCopy(fixture,compiler,true));
 m=xrtJsonParse(sv("{\"jsonrpc\":\"2.0\",\"method\":\"中文😀\"}")); CHECK(m && (frame=xideLspFrame(m,&length,&error)));
 for(size_t i=0;i<length;++i) CHECK(xideLspStreamFeed(&stream,frame+i,1,received,&count,&error));
 CHECK(count==1 && xideLspStreamEnd(&stream,&error)); CHECK(xideLspStreamFeed(&stream,frame,length,received,&count,&error) && xideLspStreamFeed(&stream,frame,length,received,&count,&error) && count==3); xideLspStreamClear(&stream); free(frame); frame=NULL; xrtValueRelease(m); m=NULL;
 const char* bad[]={"Content-Length: 0\r\n\r\n","Content-Length: -1\r\n\r\n","Content-Length: 8388609\r\n\r\n","Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}","Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; Charset = UTF-16\r\n\r\n{}","Content-Length: 3\r\n\r\n{}","Content-Length: 2\r\n\r\n[]","Content-Length: 2\n\n{}","Missing: 2\r\n\r\n{}"};
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) CHECK(invalid(bad[i],strlen(bad[i])));
 const char* good="Content-Length: 17\r\nContent-Type: application/vscode-jsonrpc; Charset = \"UTF-8\"; extra=yes\r\n\r\n{\"jsonrpc\":\"2.0\"}"; CHECK(xideLspStreamFeed(&stream,good,strlen(good),received,&count,&error) && xideLspStreamEnd(&stream,&error)); xideLspStreamClear(&stream);
 const char raw[]="Content-Length: 2\r\n\r\n\xff\xff"; CHECK(invalid(raw,sizeof(raw)-1));
 uri=xideLspPathToUri("D:/MixedCase/中文 空格😀#%.xl",&error); CHECK(uri && strstr(uri,"MixedCase") && strstr(uri,"%F0%9F%98%80") && (round=xideLspUriToPath(uri,&error)) && !strcmp(round,"D:/MixedCase/中文 空格😀#%.xl")); free(uri); uri=NULL; free(round); round=NULL;
 uri=xideLspPathToUri("//Server/Share/中文😀.xl",&error); CHECK(uri && (round=xideLspUriToPath(uri,&error)) && !strcmp(round,"//Server/Share/中文😀.xl")); free(uri); uri=NULL; free(round); round=NULL;
 const char* badUri[]={"https://example/x.xl","file:///D:/x%00.xl","file:///D:/x%2F.xl","file:///D:/x%5c.xl","file:///D:/x%FF.xl","file://server","file://server/","file:///relative.xl","file:///D:/x.xl?x","file:///D:/x.xl#part"};
 for(size_t i=0;i<sizeof(badUri)/sizeof(*badUri);++i) { round=xideLspUriToPath(badUri[i],&error); CHECK(!round); }
 const char* text="a中文😀e\xcc\x81\r\nb\rc\n"; int offset=0,line=0,column=0;
 CHECK(xideLspPositionToOffset(text,0,5,&offset) && offset==11 && xideLspOffsetToPosition(text,offset,&line,&column) && line==0 && column==5);
 CHECK(!xideLspPositionToOffset(text,0,4,&offset) && !xideLspOffsetToPosition(text,9,&line,&column) && !xideLspOffsetToPosition(text,15,&line,&column) && xideLspPositionToOffset(text,1,0,&offset) && offset==16 && xideLspPositionToOffset(text,3,0,&offset) && !xideLspPositionToOffset(text,4,0,&offset));
 xide_lsp_position batch[]={{1,0,-1},{0,5,-1},{0,0,-1},{3,0,-1},{0,5,-1}};
 CHECK(xideLspPositionsToOffsets(text,batch,5) && batch[0].offset==16 && batch[1].offset==11 && batch[2].offset==0 && batch[3].offset==20 && batch[4].offset==11);
 batch[1]=(xide_lsp_position){.line=0,.character=4,.offset=123}; CHECK(!xideLspPositionsToOffsets(text,batch,5) && batch[1].offset==123 && xideLspPositionsToOffsets(text,NULL,0));
 const char* saved="int 中文 = missingName\n"; CHECK(writeText(entry,saved) && writeText(mode,"bad")); cache=xideToolchainsCreate(); CHECK(cache);
 q=xideLanguageCheckStart(cache,compiler,root,entry,saved,7,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_READY && (out=xideLanguageCheckResult(q)) && number(get(out,"version"))==7 && xrtValueCount(get(out,"diagnostics"))==1 && number(get(get(get(xrtValueArrayGet(get(out,"diagnostics"),0),"range"),"start"),"character"))==9); xrtValueRelease(out); out=NULL; xideLanguageCheckDestroy(q); q=NULL;
 const char* invalidModes[]={"unsupported","malformed","invalid-utf8","oversize","crash","foreign","range","wrong-exit","change-entry","change-toolchain"};
 for(size_t i=0;i<sizeof(invalidModes)/sizeof(*invalidModes);++i) { CHECK(writeText(entry,saved) && writeText(mode,invalidModes[i])); q=xideLanguageCheckStart(cache,compiler,root,entry,saved,7,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_FAILED && !xideLanguageCheckResult(q) && *error.message); xideLanguageCheckDestroy(q); q=NULL; }
 CHECK(writeText(entry,saved) && writeText(mode,"stderr-flood")); q=xideLanguageCheckStart(cache,compiler,root,entry,saved,8,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_READY); xideLanguageCheckDestroy(q); q=NULL;
 CHECK(writeText(mode,"slow")); q=xideLanguageCheckStart(cache,compiler,root,entry,saved,9,&error); CHECK(q); Sleep(100); uint64_t began=GetTickCount64(); xideLanguageCheckCancel(q); CHECK(waitCheck(q,&error)==XIDE_CHECK_CANCELLED && GetTickCount64()-began<2000); xideLanguageCheckDestroy(q); q=NULL;
 CHECK(writeText(mode,"bad")); c=xideLanguageClientStart(server,compiler,root,&error); xideLanguageClientSetWake(c,wake,(void*)&wakeCount); CHECK(c && checkRequest(c,"file:///D:/missing.xl",1,10,&error) && (m=next(c,&error)) && number(get(get(m,"error"),"code"))==-32002 && waitWake()); xrtValueRelease(m); m=NULL;
 xvalue* init=xrtValueObject(); put(init,"capabilities",xrtValueObject()); xvalue* options=xrtValueObject(); put(options,"checkOnSave",xrtValueBool(false)); put(init,"initializationOptions",options); CHECK(emit(c,"initialize",init,1,&error) && (m=next(c,&error)) && !strcmp(string(get(get(get(m,"result"),"capabilities"),"positionEncoding")),"utf-16") && get(get(get(m,"result"),"capabilities"),"hoverProvider") && get(get(get(m,"result"),"capabilities"),"documentSymbolProvider")); xrtValueRelease(m); m=NULL;
 uri=xideLspPathToUri(entry,&error); CHECK(uri && emit(c,"initialized",xrtValueObject(),0,&error) && emit(c,"textDocument/didOpen",params(uri,1,saved),0,&error) && checkRequest(c,uri,1,2,&error));
 m=next(c,&error); CHECK(m && string(get(m,"method")) && !strcmp(string(get(m,"method")),"textDocument/publishDiagnostics") && xrtValueCount(get(get(m,"params"),"diagnostics"))==1); xrtValueRelease(m); m=next(c,&error); CHECK(m && number(get(m,"id"))==2 && number(get(get(m,"result"),"count"))==1); xrtValueRelease(m); m=NULL;
 xvalue* change=params(uri,2,NULL); put(get(change,"textDocument"),"version",xrtValueInt(2)); xvalue *changes=xrtValueArray(),*body=xrtValueObject(); put(body,"text",xrtValueString(sv("int other = 0\n"))); xrtValueArrayAppend(changes,body); xrtValueRelease(body); put(change,"contentChanges",changes);
 CHECK(emit(c,"textDocument/didChange",change,0,&error) && (m=next(c,&error)) && number(get(get(m,"params"),"version"))==2 && !xrtValueCount(get(get(m,"params"),"diagnostics"))); xrtValueRelease(m); m=NULL;
 CHECK(checkRequest(c,uri,2,3,&error) && (m=next(c,&error)) && number(get(m,"id"))==3 && number(get(get(m,"error"),"code"))==-32603); xrtValueRelease(m); m=NULL;
 CHECK(writeText(entry,"int other = 0\n") && writeText(mode,"good") && checkRequest(c,uri,2,4,&error) && (m=next(c,&error)) && !xrtValueCount(get(get(m,"params"),"diagnostics"))); xrtValueRelease(m); m=next(c,&error); CHECK(m && number(get(m,"id"))==4 && number(get(get(m,"result"),"count"))==0); xrtValueRelease(m); m=NULL;
 CHECK(writeText(mode,"slow")); wchar_t signalW[32768]; CHECK(MultiByteToWideChar(CP_UTF8,0,checkSignal,-1,signalW,32768)); DeleteFileW(signalW); CHECK(checkRequest(c,uri,2,5,&error) && signal(checkSignal)); body=xrtValueObject(); put(body,"id",xrtValueInt(5)); began=GetTickCount64(); CHECK(emit(c,"$/cancelRequest",body,0,&error) && (m=next(c,&error)) && number(get(m,"id"))==5 && number(get(get(m,"error"),"code"))==-32800 && GetTickCount64()-began<2000); xrtValueRelease(m); m=NULL;
 CHECK(writeText(mode,"slow-bad")); DeleteFileW(signalW); CHECK(checkRequest(c,uri,2,7,&error) && signal(checkSignal));
 change=params(uri,3,NULL); put(get(change,"textDocument"),"version",xrtValueInt(3)); changes=xrtValueArray(); body=xrtValueObject(); put(body,"text",xrtValueString(sv("int modified = 0\n"))); xrtValueArrayAppend(changes,body); xrtValueRelease(body); put(change,"contentChanges",changes); CHECK(emit(c,"textDocument/didChange",change,0,&error));
 m=next(c,&error); CHECK(m && number(get(get(m,"params"),"version"))==3 && !xrtValueCount(get(get(m,"params"),"diagnostics"))); xrtValueRelease(m); m=next(c,&error); CHECK(m && number(get(m,"id"))==7 && number(get(get(m,"error"),"code"))==-32801); xrtValueRelease(m); m=NULL;
 CHECK(emit(c,"shutdown",xrtValueObject(),6,&error) && (m=next(c,&error)) && number(get(m,"id"))==6 && xrtValueType(get(m,"result"))==XVALUE_NULL); xrtValueRelease(m); m=NULL; CHECK(emit(c,"exit",NULL,0,&error));
 uint64_t until=GetTickCount64()+5000; while(xideLanguageClientState(c,error.message,sizeof(error.message))<=XIDE_LS_RUNNING && GetTickCount64()<until) Sleep(5); CHECK(xideLanguageClientState(c,error.message,sizeof(error.message))==XIDE_LS_STOPPED); xideLanguageClientStop(c); c=NULL;
 const char* serverBad[]={"server-partial","server-malformed"}; for(size_t i=0;i<2;++i) { CHECK(writeText(mode,serverBad[i])); InterlockedExchange(&wakeCount,0); c=xideLanguageClientStart(fixture,compiler,root,&error); CHECK(c); xideLanguageClientSetWake(c,wake,(void*)&wakeCount); until=GetTickCount64()+5000; while(xideLanguageClientState(c,error.message,sizeof(error.message))<=XIDE_LS_RUNNING && GetTickCount64()<until) Sleep(5); CHECK(xideLanguageClientState(c,error.message,sizeof(error.message))==XIDE_LS_FAILED && *error.message && waitWake()); xideLanguageClientStop(c); c=NULL; }
 if(argc==3) { CHECK(writeText(entry,saved)); q=xideLanguageCheckStart(cache,argv[2],root,entry,saved,99,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_READY && (out=xideLanguageCheckResult(q)) && xrtValueCount(get(out,"diagnostics"))==1 && strstr(string(get(xrtValueArrayGet(get(out,"diagnostics"),0),"message")),"missingName")); xrtValueRelease(out); out=NULL; xideLanguageCheckDestroy(q); q=NULL; }
 puts("lsp_test passed: fragmented framing, strict encoding/URI/UTF-16, saved compiler checks and invalid outputs, separate stderr, source/toolchain changes, lifecycle/version/cancellation and malformed server cleanup.");
cleanup:
 xideLanguageClientStop(c); xideLanguageCheckDestroy(q); xideToolchainsDestroy(cache); xrtValueRelease(m); xrtValueRelease(out); xideLspStreamClear(&stream); free(frame); free(uri); free(round); for(size_t i=0;i<ownedCount;++i) free(owned[i]); return failed;
}
