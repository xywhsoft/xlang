#include "services/c_input_trace.h"
#include "platform/path.h"
#include "../../tools/clangd_input_trace/NativeInput.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define CT_LIMIT 65536u
typedef struct ct_record { char *path,*realPath; int kind,hashed; xide_file_snapshot stamp; xvalue* entries; } ct_record;
struct xide_c_input_trace { CRITICAL_SECTION lock; ct_record* records; uint32_t* slots; size_t count; uint64_t sequence,metadataBytes; int complete,failed; };
static xstrview ctsv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* ctget(const xvalue* v,const char* name) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,ctsv(name)) : NULL; }
static int ctfail(xide_model_error* e,const char* message) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",message); } return 0; }
static const char* cttext(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) && s.Size && s.Size<=32768 && !memchr(s.Data,0,s.Size) && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0) ? s.Data : NULL; }
static char* ctcopy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static size_t ctslot(const xide_c_input_trace* t,const char* path) { uint64_t h=14695981039346656037ULL; for(const unsigned char* p=(const unsigned char*)path;*p;++p) { h^=*p; h*=1099511628211ULL; } size_t slot=(size_t)h&(CT_LIMIT*2u-1u); while(t->slots[slot] && strcmp(t->records[t->slots[slot]-1].path,path)) slot=(slot+1)&(CT_LIMIT*2u-1u); return slot; }
static int ctuint(const xvalue* v,uint64_t* n) {
 const char* s=cttext(v); if(!s || strlen(s)>20 || (s[0]=='0' && s[1])) return 0;
 *n=0; for(;*s;++s) { if(*s<'0' || *s>'9' || *n>(UINT64_MAX-(unsigned)(*s-'0'))/10) return 0; *n=*n*10+(unsigned)(*s-'0'); } return 1;
}
static int ctabsolute(const char* p) { size_t n=p ? strlen(p) : 0; return (n>=3 && (((p[0]>='a' && p[0]<='z') || (p[0]>='A' && p[0]<='Z')) && p[1]==':' && p[2]=='/')) || (n>4 && p[0]=='/' && p[1]=='/'); }
static int ctstamp(const xvalue* row,xide_file_snapshot* s) {
 uint64_t volume=0,attrs=0; memset(s,0,sizeof(*s));
 if(!ctuint(ctget(row,"index"),&s->index) || !ctuint(ctget(row,"volume"),&volume) || volume>UINT32_MAX || !ctuint(ctget(row,"size"),&s->length) || s->length>512ULL*1024*1024 || !ctuint(ctget(row,"modified100ns"),&s->lastWrite) || !ctuint(ctget(row,"attributes"),&attrs) || !attrs || attrs>UINT32_MAX || (attrs&FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
 s->volume=(uint32_t)volume; s->attributes=(uint32_t)attrs;
 const char* sha=cttext(ctget(row,"sha256")); if(!sha) return !ctget(row,"sha256"); if(strlen(sha)!=64) return 0;
 for(int i=0;i<32;++i) { unsigned value=0; for(int j=0;j<2;++j) { char c=sha[i*2+j]; unsigned nibble=c>='0' && c<='9' ? (unsigned)(c-'0') : c>='a' && c<='f' ? (unsigned)(c-'a'+10) : 16u; if(nibble>15) return 0; value=value*16+nibble; } s->sha256[i]=(unsigned char)value; } return 1;
}
static int ctmetadata(const xide_file_snapshot* a,const xlang_native_stamp* b) { return a->index==b->index && a->volume==b->volume && a->attributes==b->attributes && ((a->attributes&FILE_ATTRIBUTE_DIRECTORY) || (a->length==b->size && a->lastWrite==b->modified)); }
static char* ctutf8(const wchar_t* wide) { int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,NULL,0,NULL,NULL); char* p=n ? malloc((size_t)n) : NULL; if(p) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,p,n,NULL,NULL); return p; }
static int ctnameCompare(const void* a,const void* b) { const xvalue *left=*(const xvalue*const*)a,*right=*(const xvalue*const*)b; return strcmp(cttext(ctget(left,"name")),cttext(ctget(right,"name"))); }
static int ctdirectory(const char* path,const xvalue* expected,xide_model_error* e) {
 if(xrtValueType(expected)!=XVALUE_ARRAY || xrtValueCount(expected)>4096) return ctfail(e,"Invalid compiler directory listing.");
 size_t n=strlen(path); char* pattern=malloc(n+3); if(!pattern) return ctfail(e,"Cannot inspect compiler directory listing."); memcpy(pattern,path,n); memcpy(pattern+n,"/*",3); wchar_t* w=xidePathNativeWide(pattern); free(pattern); if(!w) return ctfail(e,"Invalid compiler directory path.");
 WIN32_FIND_DATAW info; HANDLE find=FindFirstFileW(w,&info); DWORD code=GetLastError(); free(w); xvalue* actual=xrtValueArray(); int okay=actual!=NULL;
 if(find==INVALID_HANDLE_VALUE) okay=okay && code==ERROR_FILE_NOT_FOUND;
 else { do { if(!wcscmp(info.cFileName,L".") || !wcscmp(info.cFileName,L"..")) continue;
   char* name=ctutf8(info.cFileName); xvalue* row=xrtValueObject();
   okay=name && row && xrtValueCount(actual)<4096 && !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && xrtValueObjectSetNew(row,ctsv("name"),xrtValueString(ctsv(name))) && xrtValueObjectSetNew(row,ctsv("type"),xrtValueInt(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY ? 3 : 2)) && xrtValueArrayAppend(actual,row);
   free(name); xrtValueRelease(row); if(!okay) break;
  } while(FindNextFileW(find,&info)); code=GetLastError(); FindClose(find); okay=okay && code==ERROR_NO_MORE_FILES; }
 size_t count=xrtValueCount(actual); const xvalue** sorted=okay && count ? malloc(count*sizeof(*sorted)) : NULL;
 if(count && !sorted) okay=0;
 for(size_t i=0;okay && i<count;++i) sorted[i]=xrtValueArrayGet(actual,i);
 if(okay && count) qsort(sorted,count,sizeof(*sorted),ctnameCompare);
 okay=okay && count==xrtValueCount(expected);
 for(size_t i=0;okay && i<count;++i) { const xvalue* row=xrtValueArrayGet(expected,i); const char* name=cttext(ctget(row,"name")); int64 a=0,b=0;
  okay=name && !strchr(name,'/') && !strchr(name,'\\') && strcmp(name,".") && strcmp(name,"..") && !strcmp(name,cttext(ctget(sorted[i],"name"))) && xrtValueGetInt(ctget(row,"type"),&a) && xrtValueGetInt(ctget(sorted[i],"type"),&b) && a==b;
 }
 free(sorted); xrtValueRelease(actual); return okay ? 1 : ctfail(e,"Compiler-observed directory candidates changed or could not be enumerated.");
}
static int ctinspect(ct_record* r,int (*cancelled)(void*),void* user,uint64_t* bytes,xide_model_error* e) {
 if(cancelled && cancelled(user)) return 0;
 void* handle=NULL; xlang_native_stamp now; unsigned long code=0;
 int exists=xlangNativeOpen(r->path,0,&handle,&now,&code);
 if(r->kind==0) { xlangNativeClose(handle); return !exists && (code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND || code==ERROR_DIRECTORY) ? 1 : ctfail(e,"A compiler-observed missing path appeared or became unreadable."); }
 if(!exists || !ctmetadata(&r->stamp,&now) || ((now.attributes&FILE_ATTRIBUTE_DIRECTORY)!=0)!=(r->kind==2)) { xlangNativeClose(handle); return ctfail(e,"Compiler-observed input metadata or physical identity changed."); }
 int okay=1;
 if(r->realPath) {
  DWORD count=GetFinalPathNameByHandleW(handle,NULL,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS); wchar_t* wide=count && count<32768 ? malloc(((size_t)count+1)*sizeof(*wide)) : NULL; DWORD got=wide ? GetFinalPathNameByHandleW(handle,wide,count+1,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS) : 0; char* real=got && got<=count ? ctutf8(wide) : NULL;
  if(real && !strncmp(real,"\\\\?\\UNC\\",8)) { memmove(real+2,real+8,strlen(real+8)+1); real[0]=real[1]='\\'; }
  else if(real && !strncmp(real,"\\\\?\\",4)) memmove(real,real+4,strlen(real+4)+1);
  okay=real && xidePathLexicalEqual(real,r->realPath); free(real); free(wide);
 }
 xlangNativeClose(handle);
 if(!okay) return ctfail(e,"Compiler-observed physical path resolution changed.");
 if(r->kind==2) return !r->entries || ctdirectory(r->path,r->entries,e);
 if(r->stamp.length>1024ULL*1024*1024-*bytes) return ctfail(e,"Compiler-observed guard inputs exceed 1 GiB.");
 *bytes+=r->stamp.length;
 xide_file_snapshot disk={0}; if(!xideFileSnapshotInspect(r->path,&disk,e)) return 0;
 xlang_native_stamp compare={.index=disk.index,.size=disk.length,.modified=disk.lastWrite,.volume=disk.volume,.attributes=disk.attributes};
 if(!ctmetadata(&r->stamp,&compare) || (r->hashed && memcmp(disk.sha256,r->stamp.sha256,32))) return ctfail(e,"Compiler-read bytes or physical identity changed.");
 return 1;
}
xide_c_input_trace* xideCInputTraceCreate(void) { xide_c_input_trace* t=calloc(1,sizeof(*t)); if(t) { InitializeCriticalSection(&t->lock); t->records=calloc(CT_LIMIT,sizeof(*t->records)); t->slots=calloc(CT_LIMIT*2u,sizeof(*t->slots)); if(!t->records || !t->slots) { free(t->records); free(t->slots); DeleteCriticalSection(&t->lock); free(t); return NULL; } } return t; }
void xideCInputTraceDestroy(xide_c_input_trace* t) { if(!t) return; for(size_t i=0;i<t->count;++i) { free(t->records[i].path); free(t->records[i].realPath); xrtValueRelease(t->records[i].entries); } free(t->records); free(t->slots); DeleteCriticalSection(&t->lock); free(t); }
int xideCInputTraceComplete(const xide_c_input_trace* t) { return t && t->sequence && t->complete && !t->failed; }
int xideCInputTraceApply(xide_c_input_trace* t,const xvalue* notification,int (*cancelled)(void*),void* user,xide_model_error* e) {
 const xvalue* p=ctget(notification,"params"),*rows=ctget(p,"records"); uint64_t sequence=0,previous=0; int64 version=0; bool stable=false,complete=false; uint64_t bytes=0; xstrview serverError={0};
 int okay=t && !t->failed && xrtValueGetInt(ctget(p,"version"),&version) && version==1 && ctuint(ctget(p,"sequence"),&sequence) && ctuint(ctget(p,"previous"),&previous) && previous==t->sequence && previous<UINT64_MAX && sequence==previous+1 && xrtValueGetBool(ctget(p,"stable"),&stable) && stable && xrtValueGetBool(ctget(p,"complete"),&complete) && xrtValueGetString(ctget(p,"error"),&serverError) && !serverError.Size && xrtValueType(rows)==XVALUE_ARRAY && xrtValueCount(rows)<=128;
 if(!okay) { if(t) t->failed=1; return ctfail(e,"C server input trace is invalid, unstable or out of sequence."); }
 size_t encodedLength=0; char* encoded=xrtJsonStringify(p,false,&encodedLength); okay=encoded && encodedLength<=512u*1024u+4096u && encodedLength<=128ULL*1024*1024-t->metadataBytes; xrtFree(encoded);
 if(!okay) { t->failed=1; return ctfail(e,"C input observation metadata exceeds its bounded transport or session budget."); }
 t->metadataBytes+=encodedLength;
 for(size_t i=0;okay && i<xrtValueCount(rows);++i) {
  const xvalue* row=xrtValueArrayGet(rows,i); const char *path=cttext(ctget(row,"path")),*kind=cttext(ctget(row,"kind")); ct_record next={0};
  size_t rowLength=0; char* rowBytes=xrtJsonStringify(row,false,&rowLength); okay=rowBytes && rowLength<=256u*1024u && ctabsolute(path) && kind; xrtFree(rowBytes); if(!okay) break;
  next.kind=!strcmp(kind,"missing") ? 0 : !strcmp(kind,"file") ? 1 : !strcmp(kind,"directory") ? 2 : -1;
  next.path=ctcopy(path); next.hashed=ctget(row,"sha256")!=NULL;
  okay=next.path && next.kind>=0 && (!next.kind || (ctstamp(row,&next.stamp) && ((next.stamp.attributes&FILE_ATTRIBUTE_DIRECTORY)!=0)==(next.kind==2))) && (next.kind || (!ctget(row,"sha256") && !ctget(row,"entries") && !ctget(row,"realPath"))) && (next.kind==1 || !next.hashed);
  if(ctget(row,"realPath")) { const char* real=cttext(ctget(row,"realPath")); next.realPath=real && ctabsolute(real) ? ctcopy(real) : NULL; okay=okay && next.realPath; }
  if(ctget(row,"entries")) { next.entries=xrtValueDeepClone(ctget(row,"entries")); okay=okay && next.kind==2 && next.entries && xrtValueType(next.entries)==XVALUE_ARRAY && xrtValueCount(next.entries)<=4096; }
  size_t slot=ctslot(t,path),at=t->slots[slot] ? t->slots[slot]-1 : t->count;
  ct_record* old=at<t->count ? &t->records[at] : NULL;
  if(old && okay) {
   xlang_native_stamp compare={.index=next.stamp.index,.size=next.stamp.length,.modified=next.stamp.lastWrite,.volume=next.stamp.volume,.attributes=next.stamp.attributes};
   okay=old->kind==next.kind && (!next.kind || ctmetadata(&old->stamp,&compare)) && (!old->hashed || !next.hashed || !memcmp(old->stamp.sha256,next.stamp.sha256,32)) && (!old->realPath || !next.realPath || !strcmp(old->realPath,next.realPath)) && (!old->entries || !next.entries || xrtValueEqual(old->entries,next.entries));
   if(old->hashed && !next.hashed) { next.hashed=1; memcpy(next.stamp.sha256,old->stamp.sha256,32); }
   if(old->realPath && !next.realPath) next.realPath=ctcopy(old->realPath);
   if(old->entries && !next.entries) next.entries=xrtValueDeepClone(old->entries);
   okay=okay && (!old->realPath || next.realPath) && (!old->entries || next.entries);
  }
  okay=okay && at<CT_LIMIT && ctinspect(&next,cancelled,user,&bytes,e);
  if(okay) {
   EnterCriticalSection(&t->lock); if(old) { free(old->path); free(old->realPath); xrtValueRelease(old->entries); } else ++t->count;
   t->records[at]=next; t->slots[slot]=(uint32_t)at+1; LeaveCriticalSection(&t->lock);
  } else { free(next.path); free(next.realPath); xrtValueRelease(next.entries); }
 }
 if(!okay) { t->failed=1; if(!e || !*e->message) ctfail(e,"Cannot verify C server physical input observations."); return 0; }
 t->sequence=sequence; t->complete=complete; return 1;
}
int xideCInputTraceCheck(xide_c_input_trace* t,int (*cancelled)(void*),void* user,xide_model_error* e) {
 if(!t || t->failed) return ctfail(e,"C input trace failed.");
 uint64_t bytes=0;
 for(size_t i=0;i<t->count;++i) if(!ctinspect(&t->records[i],cancelled,user,&bytes,e)) { t->failed=1; return 0; }
 return 1;
}
int xideCInputTraceSnapshot(xide_c_input_trace* t,const char* path,xide_file_snapshot* out) {
 if(!t || !path || !out) return 0;
 EnterCriticalSection(&t->lock); int yes=0;
 size_t slot=ctslot(t,path); const ct_record* direct=t->slots[slot] ? &t->records[t->slots[slot]-1] : NULL;
 if(direct && direct->kind==1 && direct->hashed) { *out=direct->stamp; yes=1; }
 else for(size_t i=0;i<t->count;++i) { const ct_record* r=&t->records[i]; if(r->kind==1 && r->hashed && xidePathLexicalEqual(path,r->path)) { *out=r->stamp; yes=1; break; } }
 LeaveCriticalSection(&t->lock); return yes;
}
