#include "services/build_inputs.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define REPORT_LIMIT (32u*1024u*1024u)
#define FILE_LIMIT (64u*1024u*1024u)
#define TOTAL_LIMIT (320u*1024u*1024u)
#define FILES_LIMIT 1536u
#define NATIVE_LIMIT 4096u
#define XLANG_LIMIT 2048u
typedef struct input { char* path; wchar_t* key; xfileinfo info; unsigned char hash[32]; } input;
typedef struct probe { char* path; wchar_t* key; int exists,native; } probe;
struct xide_build_inputs {
 input *files,artifact; probe* probes; size_t count,probeCount,nativeCount;
 char* receipt; xide_build_cancel cancel; void* user;
};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* get(const xvalue* v,const char* key) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,sv(key)) : NULL; }
static int fail(xide_model_error* e,const char* reason,const char* path) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s%s%s",reason,path ? ": " : "",path ? path : ""); } return 0; }
static const char* text(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) && s.Size<=32768 && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static wchar_t* wide(const char* s) { int n=s ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0) : 0; wchar_t* p=n ? malloc((size_t)n*sizeof(*p)) : NULL; if(p) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,p,n); return p; }
static int same(const wchar_t* a,const wchar_t* b) { return a && b && CompareStringOrdinal(a,-1,b,-1,TRUE)==CSTR_EQUAL; }
static int integer(const xvalue* v,int64* n,int64 max) { return xrtValueType(v)==XVALUE_INT && xrtValueGetInt(v,n) && *n>=0 && *n<=max; }
static int hex(const xvalue* value,unsigned char* bytes,size_t count) {
 const char* s=text(value); if(!s || strlen(s)!=count*2) return 0;
 for(size_t i=0;i<count;++i) { unsigned n=0; for(int j=0;j<2;++j) { char c=s[i*2+j]; unsigned d=c>='0' && c<='9' ? (unsigned)(c-'0') : c>='a' && c<='f' ? (unsigned)(c-'a'+10) : 16u; if(d>15) return 0; n=n*16+d; } bytes[i]=(unsigned char)n; } return 1;
}
static int hex64(const xvalue* v,uint64* out) { unsigned char bytes[8]; if(!hex(v,bytes,8)) return 0; *out=0; for(int i=0;i<8;++i) *out=(*out<<8)|bytes[i]; return 1; }
static int decimal(const xvalue* v,int64* out) { const char* s=text(v); if(!s || !*s || strlen(s)>20) return 0; errno=0; char* end=NULL; long long n=strtoll(s,&end,10); char canonical[32]; snprintf(canonical,sizeof(canonical),"%lld",n); if(errno || !end || *end || strcmp(s,canonical)) return 0; *out=(int64)n; return 1; }
static int canonical(const xvalue* value,char** path,wchar_t** key) {
 const char* s=text(value); size_t n=s ? strlen(s) : 0;
 int absolute=n>=3 && ((s[0]>='a' && s[0]<='z') || (s[0]>='A' && s[0]<='Z')) && s[1]==':' && (s[2]=='/' || s[2]=='\\');
 absolute=absolute || (n>4 && ((s[0]=='/' && s[1]=='/') || (s[0]=='\\' && s[1]=='\\')));
 if(!absolute || n>=4096) return 0;
 char* p=xidePathResolveLexical(NULL,s); wchar_t *w=wide(s),*normalized=wide(p);
 if(w) for(wchar_t* at=w;*at;++at) if(*at=='\\') *at='/';
 int okay=p && same(w,normalized); free(w);
 if(!okay) { free(p); free(normalized); return 0; }
 *path=p; *key=normalized; return 1;
}
static int infoEqual(const xfileinfo* a,const xfileinfo* b) {
 uint32 required=XFILE_INFO_SIZE|XFILE_INFO_MODIFY_TIME|XFILE_INFO_IDENTITY;
 return a->Type==XFILE_TYPE_FILE && b->Type==XFILE_TYPE_FILE && (a->Available&required)==required && (b->Available&required)==required && a->Available==b->Available && a->Size==b->Size && a->Device==b->Device && a->Identity==b->Identity && a->Modified==b->Modified && a->Attributes==b->Attributes && (!(a->Available&XFILE_INFO_CHANGE_TIME) || a->Changed==b->Changed);
}
static int parseFile(const xvalue* row,input* f) {
 const xvalue* m=get(row,"metadata"); int64 size=0,available=0,attributes=0;
 if(xrtValueCount(row)!=4 || xrtValueCount(m)!=6 || !canonical(get(row,"path"),&f->path,&f->key) || !integer(get(row,"size"),&size,FILE_LIMIT) || !hex(get(row,"sha256"),f->hash,32) || !integer(get(m,"available"),&available,UINT32_MAX) || !integer(get(m,"attributes"),&attributes,UINT32_MAX) || !hex64(get(m,"device"),&f->info.Device) || !hex64(get(m,"identity"),&f->info.Identity) || !decimal(get(m,"modifiedUs"),&f->info.Modified) || !decimal(get(m,"changedUs"),&f->info.Changed)) return 0;
 f->info.Type=XFILE_TYPE_FILE; f->info.Size=(uint64)size; f->info.Available=(uint32)available; f->info.Attributes=(uint32)attributes;
 return infoEqual(&f->info,&f->info);
}
static int stopped(xide_build_inputs* r,xide_model_error* e) { if(r->cancel && r->cancel(r->user)) { fail(e,"Build input validation interrupted",NULL); return 1; } return 0; }
/* Same handle supplies metadata and streamed bytes; path identity is checked
   before close. No second XRT implementation is linked into the IDE. */
static int verifyFile(xide_build_inputs* r,const input* expected,const char* path,int identity,xide_model_error* e) {
 xfile file=NULL; xfileinfo first,last,named; unsigned char block[65536],digest[32],extra; size_t n=0;
 BCRYPT_ALG_HANDLE algorithm=NULL; BCRYPT_HASH_HANDLE hash=NULL; unsigned char* object=NULL; DWORD bytes=0,returned=0;
 int okay=!stopped(r,e) && (file=xideIoOpen(path,XFILE_READ)) && xideIoReceiptFileStat(file,&first) && infoEqual(&first,&first) && first.Size==expected->info.Size && (!identity || infoEqual(&first,&expected->info));
 NTSTATUS status=okay ? BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0) : -1;
 if(status==0) status=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&bytes,sizeof(bytes),&returned,0);
 if(status==0) { object=malloc(bytes); if(object) status=BCryptCreateHash(algorithm,&hash,object,bytes,NULL,0,0); else status=-1; }
 uint64 at=0;
 while(status==0 && at<first.Size) {
  size_t want=first.Size-at>sizeof(block) ? sizeof(block) : (size_t)(first.Size-at),done=0;
  if(stopped(r,e) || !xrtReadFull(file,block,want,&done) || done!=want) { status=-1; break; }
  status=BCryptHashData(hash,block,(ULONG)done,0); at+=done;
 }
 if(status==0) status=BCryptFinishHash(hash,digest,sizeof(digest),0);
 okay=okay && status==0 && !memcmp(digest,expected->hash,32) && xrtRead(file,&extra,1,&n) && !n && xideIoReceiptFileStat(file,&last) && infoEqual(&first,&last) && xideIoReceiptStat(path,&named) && infoEqual(&first,&named);
 if(hash) { BCryptDestroyHash(hash); } free(object); if(algorithm) { BCryptCloseAlgorithmProvider(algorithm,0); } if(file && !xrtClose(file)) okay=0;
 return okay ? 1 : fail(e,"Build input or artifact differs from compiler receipt",path);
}
static int missing(const char* path) { wchar_t* w=xidePathNativeWide(path); if(!w) return 0; DWORD flags=GetFileAttributesW(w),code=GetLastError(); free(w); return flags==INVALID_FILE_ATTRIBUTES && (code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND); }
int xideBuildInputsPackagePath(char** out,xide_model_error* e) {
 if(!out) return fail(e,"Missing package environment output",NULL);
 *out=NULL;
 SetLastError(ERROR_SUCCESS); DWORD n=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",NULL,0),code=GetLastError();
 if(!n) { if(code==ERROR_ENVVAR_NOT_FOUND) return 1; if(code==ERROR_SUCCESS) { *out=calloc(1,1); return *out ? 1 : fail(e,"Cannot retain empty package environment",NULL); } return fail(e,"Cannot read package environment",NULL); }
 if(n>32768) return fail(e,"Package environment exceeds the limit",NULL);
 wchar_t* w=malloc((size_t)n*sizeof(*w)); if(!w) return fail(e,"Cannot allocate package environment",NULL);
 DWORD got=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",w,n); int bytes=got && got<n ? WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL) : 0;
 char* p=bytes ? malloc((size_t)bytes) : NULL; int okay=p && WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,p,bytes,NULL,NULL); free(w);
 if(!okay) { free(p); return fail(e,"Cannot capture stable UTF-8 package environment",NULL); } *out=p; return 1;
}
void xideBuildInputsDestroy(xide_build_inputs* r) {
 if(!r) return;
 if(r->files) for(size_t i=0;i<r->count;++i) { free(r->files[i].path); free(r->files[i].key); }
 if(r->probes) for(size_t i=0;i<r->probeCount;++i) { free(r->probes[i].path); free(r->probes[i].key); }
 free(r->files); free(r->probes); free(r->artifact.path); free(r->artifact.key); free(r->receipt); free(r);
}
int xideBuildInputsValidate(xide_build_inputs* r,xide_model_error* e) {
 if(!r) return fail(e,"Missing compiler input receipt",NULL);
 for(size_t i=0;i<r->count;++i) if(!verifyFile(r,&r->files[i],r->files[i].path,1,e)) return 0;
 for(size_t i=0;i<r->probeCount;++i) { probe* p=&r->probes[i]; if(stopped(r,e)) return 0; if(p->native ? !missing(p->path) : (xideIoExists(p->path)!=0)!=p->exists) return fail(e,"Compiler resolution candidate changed",p->path); }
 return verifyFile(r,&r->artifact,r->artifact.path,1,e);
}
int xideBuildInputsHasFile(const xide_build_inputs* r,const char* path) { if(!r || !path) return 0; for(size_t i=0;i<r->count;++i) if(xidePathLexicalEqual(r->files[i].path,path)) return 1; return 0; }
size_t xideBuildInputsPathCount(const xide_build_inputs* r) { return r ? r->count+r->probeCount : 0; }
const char* xideBuildInputsPath(const xide_build_inputs* r,size_t i) { return !r || i>=r->count+r->probeCount ? NULL : i<r->count ? r->files[i].path : r->probes[i-r->count].path; }
int xideBuildInputsOutputValid(xide_build_inputs* r,const char* path,xide_model_error* e) {
 if(!r || !path || xidePathLexicalEqual(path,r->artifact.path) || xidePathEqual(path,r->artifact.path) || xidePathLexicalEqual(path,r->receipt) || xidePathEqual(path,r->receipt)) return fail(e,"Publication overlaps receipt or stage artifact",path);
 xfileinfo target; int named=xideIoStat(path,true,&target);
 if((!named && !missing(path)) || (named && target.Type!=XFILE_TYPE_FILE)) return fail(e,"Cannot verify publication target identity",path);
 for(size_t i=0;i<r->count;++i) if(xidePathLexicalEqual(path,r->files[i].path) || (named && target.Device==r->files[i].info.Device && target.Identity==r->files[i].info.Identity)) return fail(e,"Publication would overwrite a compiler input",r->files[i].path);
 for(size_t i=0;i<r->probeCount;++i) if(xidePathLexicalEqual(path,r->probes[i].path) || (r->probes[i].exists && xidePathEqual(path,r->probes[i].path))) return fail(e,"Publication would overwrite a resolution candidate",r->probes[i].path);
 return 1;
}
int xideBuildInputsArtifactValid(xide_build_inputs* r,const char* path,xide_model_error* e) { return r && path ? verifyFile(r,&r->artifact,path,xidePathLexicalEqual(path,r->artifact.path),e) : fail(e,"Missing built artifact",path); }
xide_build_inputs* xideBuildInputsLoad(const char* receipt,const char* artifact,const char* packagePath,xide_build_cancel cancel,void* user,xide_model_error* e) {
 if(e) { memset(e,0,sizeof(*e)); } size_t size=0; unsigned char* raw=xideIoReadAllLimit(receipt,REPORT_LIMIT,&size);
 xjsonreadconfig config; xrtJsonReadConfigInit(&config); config.MaxDepth=8; config.MaxInputBytes=REPORT_LIMIT; config.MaxStringBytes=32768; config.MaxValues=65536; config.MaxContainerItems=8192;
 xvalue* root=raw && size && !memchr(raw,0,size) ? xrtJsonRead((xstrview){(char*)raw,size},&config) : NULL; xrtFree(raw);
 xide_build_inputs* r=NULL; int64 schema=0; bool complete=false,observed=false;
 const xvalue *files=get(root,"files"),*native=get(root,"nativeMissing"),*probes=get(root,"xlangProbes"),*env=get(root,"XLANG_PACKAGE_PATH");
 const char* scope=text(get(root,"scope")),*package=text(env);
 if(!root || xrtValueCount(root)!=9 || !integer(get(root,"schema"),&schema,1) || schema!=1 || !scope || strcmp(scope,"xlang-native-build-disk-inputs") || !xrtValueGetBool(get(root,"complete"),&complete) || !complete || !xrtValueGetBool(get(root,"packagePathObserved"),&observed) || !env || (!package && xrtValueType(env)!=XVALUE_NULL) || (!observed && package) || (observed && (package && packagePath ? strcmp(package,packagePath)!=0 : package!=packagePath)) || xrtValueType(files)!=XVALUE_ARRAY || !xrtValueCount(files) || xrtValueCount(files)>FILES_LIMIT || xrtValueType(native)!=XVALUE_ARRAY || xrtValueCount(native)>NATIVE_LIMIT || xrtValueType(probes)!=XVALUE_ARRAY || xrtValueCount(probes)>XLANG_LIMIT) { fail(e,"Incomplete, malformed or out-of-scope compiler input receipt",receipt); goto done; }
 r=calloc(1,sizeof(*r)); if(!r) goto invalid; r->cancel=cancel; r->user=user; r->count=xrtValueCount(files); r->nativeCount=xrtValueCount(native); r->probeCount=r->nativeCount+xrtValueCount(probes);
 r->files=calloc(r->count,sizeof(*r->files)); r->probes=calloc(r->probeCount+1,sizeof(*r->probes)); r->receipt=xidePathResolveLexical(NULL,receipt);
 if(!r->files || !r->probes || !r->receipt || !parseFile(get(root,"artifact"),&r->artifact) || !xidePathLexicalEqual(r->artifact.path,artifact)) goto invalid;
 uint64 total=0;
 for(size_t i=0;i<r->count;++i) {
  input* f=&r->files[i]; if(stopped(r,e) || !parseFile(xrtValueArrayGet(files,i),f) || f->info.Size>TOTAL_LIMIT-total || same(f->key,r->artifact.key) || (f->info.Device==r->artifact.info.Device && f->info.Identity==r->artifact.info.Identity) || xidePathLexicalEqual(f->path,r->receipt) || xidePathEqual(f->path,r->receipt)) goto invalid; total+=f->info.Size;
  for(size_t j=0;j<i;++j) if(same(f->key,r->files[j].key)) goto invalid;
 }
 for(size_t i=0;i<r->probeCount;++i) {
  probe* p=&r->probes[i]; p->native=i<r->nativeCount; const xvalue* row=p->native ? xrtValueArrayGet(native,i) : xrtValueArrayGet(probes,i-r->nativeCount); bool exists=false;
  if(stopped(r,e) || !canonical(p->native ? row : get(row,"path"),&p->path,&p->key) || (!p->native && (xrtValueCount(row)!=2 || !xrtValueGetBool(get(row,"exists"),&exists)))) goto invalid;
  p->exists=exists;
  if(same(p->key,r->artifact.key) || xidePathLexicalEqual(p->path,r->receipt)) goto invalid;
  for(size_t j=0;j<i;++j) if(same(p->key,r->probes[j].key) && (p->native==r->probes[j].native || p->exists!=r->probes[j].exists)) goto invalid;
  for(size_t j=0;!p->exists && j<r->count;++j) if(same(p->key,r->files[j].key)) goto invalid;
 }
 if(!xideBuildInputsValidate(r,e)) { xideBuildInputsDestroy(r); r=NULL; } goto done;
invalid:
 fail(e,"Invalid, duplicate, contradictory or oversized compiler input receipt",receipt); xideBuildInputsDestroy(r); r=NULL;
done:
 xrtValueRelease(root); return r;
}
