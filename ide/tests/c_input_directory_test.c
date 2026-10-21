#include "services/c_input_trace.h"
#include "platform/path.h"
#include "../../tools/clangd_input_trace/NativeInput.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
static xide_model_error error;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"c_input_directory_test:%d: %s; %s\n",__LINE__,#v,error.message); goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int put(xvalue* value,const char* key,xvalue* child) { return xrtValueObjectSetNew(value,sv(key),child); }
static int number(xvalue* value,const char* key,uint64_t n) { char buffer[32]; snprintf(buffer,sizeof(buffer),"%llu",(unsigned long long)n); return put(value,key,xrtValueString(sv(buffer))); }
static int createDirectories(const char* path) { char* native=xidePathNativeUtf8(path); int okay=native && xrtDirCreateAll(native); free(native); return okay; }
static int writeText(const char* path,const char* text) { char* native=xidePathNativeUtf8(path); int okay=native && xrtFileWriteAtomic(native,(xbytesview){(const unsigned char*)text,strlen(text)}); free(native); return okay; }
static xvalue* entry(const char* name,int type) {
 xvalue* result=xrtValueObject();
 if(!result || !put(result,"name",xrtValueString(sv(name))) || !put(result,"type",xrtValueInt(type))) { xrtValueRelease(result); return NULL; }
 return result;
}
static xvalue* observedDirectory(const char* path,xvalue* entries) {
 void* handle=NULL; xlang_native_stamp stamp={0}; unsigned long code=0;
 if(!xlangNativeOpen(path,0,&handle,&stamp,&code)) return NULL;
 xlangNativeClose(handle);
 xvalue *message=xrtValueObject(),*params=xrtValueObject(),*rows=xrtValueArray(),*row=xrtValueObject();
 int okay=message && params && rows && row && put(row,"path",xrtValueString(sv(path))) && put(row,"kind",xrtValueString(sv("directory"))) &&
  number(row,"index",stamp.index) && number(row,"volume",stamp.volume) && number(row,"size",stamp.size) && number(row,"modified100ns",stamp.modified) && number(row,"attributes",stamp.attributes) && put(row,"entries",xrtValueRetain(entries)) && xrtValueArrayAppend(rows,row) &&
  put(params,"version",xrtValueInt(1)) && number(params,"sequence",1) && number(params,"previous",0) && put(params,"stable",xrtValueBool(true)) && put(params,"complete",xrtValueBool(true)) && put(params,"error",xrtValueString(sv(""))) && put(params,"records",xrtValueRetain(rows)) &&
  put(message,"jsonrpc",xrtValueString(sv("2.0"))) && put(message,"method",xrtValueString(sv("xlang/inputTrace"))) && put(message,"params",xrtValueRetain(params));
 xrtValueRelease(params); xrtValueRelease(rows); xrtValueRelease(row);
 if(!okay) { xrtValueRelease(message); return NULL; }
 return message;
}
static int inspectDirectory(const char* root,int deep) {
 int result=0; char *path=xidePathResolve(root,deep ? "deep 中文😀" : "short 中文😀"),*a=NULL,*emoji=NULL,*folder=NULL,*added=NULL,*empty=NULL,*child=NULL;
 xvalue *entries=NULL,*wire=NULL,*emptyEntries=NULL; xide_c_input_trace* trace=NULL;
 CHECK(path);
 if(deep) while(1) {
  int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
  CHECK(length>0); if(length>420) break;
  char* next=xidePathResolve(path,"长目录0123456789中文😀"); CHECK(next); free(path); path=next;
 }
 CHECK(createDirectories(path));
 a=xidePathResolve(path,"a.h"); emoji=xidePathResolve(path,"😀.h"); folder=xidePathResolve(path,"子目录"); added=xidePathResolve(path,"new.h");
 CHECK(a && emoji && folder && added && writeText(a,"#define A 1\n") && writeText(emoji,"#define EMOJI 2\n") && createDirectories(folder));
 entries=xrtValueArray(); CHECK(entries && xrtValueArrayAppendNew(entries,entry("a.h",2)) && xrtValueArrayAppendNew(entries,entry("子目录",3)) && xrtValueArrayAppendNew(entries,entry("😀.h",2)));
 wire=observedDirectory(path,entries); trace=xideCInputTraceCreate(); CHECK(wire && trace);
 CHECK(xideCInputTraceApply(trace,wire,NULL,NULL,&error) && xideCInputTraceComplete(trace) && xideCInputTraceCheck(trace,NULL,NULL,&error));
 xide_file_snapshot file={0}; CHECK(!xideCInputTraceSnapshot(trace,path,&file));
 CHECK(writeText(added,"#define ADDED 3\n") && !xideCInputTraceCheck(trace,NULL,NULL,&error));
 xideCInputTraceDestroy(trace); trace=NULL; xrtValueRelease(wire); wire=NULL;
 CHECK(xrtValueArrayAppendNew(entries,entry("new.h",2)));
 /* Expected listings are sorted by UTF-8 byte order, independently of Windows enumeration order. */
 xvalue* sorted=xrtValueArray(); CHECK(sorted && xrtValueArrayAppend(sorted,xrtValueArrayGet(entries,0)) && xrtValueArrayAppend(sorted,xrtValueArrayGet(entries,3)) && xrtValueArrayAppend(sorted,xrtValueArrayGet(entries,1)) && xrtValueArrayAppend(sorted,xrtValueArrayGet(entries,2)));
 xrtValueRelease(entries); entries=sorted; wire=observedDirectory(path,entries); trace=xideCInputTraceCreate(); CHECK(wire && trace && xideCInputTraceApply(trace,wire,NULL,NULL,&error));
 CHECK(writeText(a,"#define A 9\n") && xideCInputTraceCheck(trace,NULL,NULL,&error));
 /* A listing observes candidate names/types, not bytes of files the compiler never read. */
 wchar_t* native=xidePathNativeWide(emoji); int deleted=native && DeleteFileW(native); free(native); CHECK(deleted && !xideCInputTraceCheck(trace,NULL,NULL,&error));
 xideCInputTraceDestroy(trace); trace=NULL; xrtValueRelease(wire); wire=NULL;
 empty=xidePathResolve(path,"empty"); child=empty ? xidePathResolve(empty,"child") : NULL; CHECK(empty && child && createDirectories(empty));
 emptyEntries=xrtValueArray(); wire=observedDirectory(empty,emptyEntries); trace=xideCInputTraceCreate(); CHECK(wire && trace && xideCInputTraceApply(trace,wire,NULL,NULL,&error));
 CHECK(createDirectories(child) && !xideCInputTraceCheck(trace,NULL,NULL,&error));
 printf("Directory observation %s passed: UTF16=%d; %s\n",deep ? "deep" : "short",MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0)-1,path);
 result=1;
cleanup:
 xideCInputTraceDestroy(trace); xrtValueRelease(entries); xrtValueRelease(wire); xrtValueRelease(emptyEntries);
 free(path); free(a); free(emoji); free(folder); free(added); free(empty); free(child); return result;
}
int main(int argc,char** argv) {
 if(argc!=2) return 2;
 char unique[96]; snprintf(unique,sizeof(unique),"directory-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 char* root=xidePathResolve(argv[1],unique); int okay=root && xrtDirCreateAll(root) && inspectDirectory(root,0) && inspectDirectory(root,1);
 printf("c_input_directory_test: %d checks, %s\n",checks,okay ? "passed" : "failed"); free(root); return okay ? 0 : 1;
}
