#include "services/rename_journal.h"
#include "services/file_rebind.h"
#include "services/session.h"
#include "services/migration_journal.h"
#include "platform/state_file.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include "lang/languages.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BODY_LIMIT (16u*1024u*1024u)
#define TOTAL_LIMIT (512u*1024u*1024u)
#define JOURNAL_FORMAT "xlang-ide-rename-journal-1"
#define MIGRATION_FORMAT "xlang-ide-migration-journal-1"
struct xide_rename_journal { xide_state_file marker; char *directory,*base,*checkpointDirectory; int recovered,newerCheckpoint,newerPending; };
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,sv(k)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static const char* jstring(const xvalue* v,const char* k) { const char* s=text(get(v,k)); return s ? s : ""; }
static int flag(const xvalue* v,const char* k) { bool b=false; return xrtValueGetBool(get(v,k),&b) && b; }
static int set(xvalue* v,const char* k,xvalue* p) { return p && xrtValueObjectSetNew(v,sv(k),p); }
static int put(xvalue* v,const char* k,const char* s) { return set(v,k,xrtValueString(sv(s ? s : ""))); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static const char* owner(xide_app* a) { return a->workspaceModel.manifest ? a->workspaceModel.manifest : a->projectSessionPath ? a->projectSessionPath : ""; }
static int number(xvalue* o,const char* key,uint64_t n) { char s[32]; snprintf(s,sizeof(s),"%llu",(unsigned long long)n); return put(o,key,s); }
static int readNumber(const xvalue* o,const char* key,uint64_t* n)
{ const char* s=text(get(o,key)); char* end=NULL; if(!s || !*s || strspn(s,"0123456789")!=strlen(s)) return 0; errno=0; unsigned long long v=strtoull(s,&end,10); if(errno || !end || *end) return 0; *n=(uint64_t)v; return 1; }
static void hex(const unsigned char* bytes,size_t n,char* result)
{ static const char h[]="0123456789abcdef"; for(size_t i=0;i<n;++i) { result[i*2]=h[bytes[i]>>4]; result[i*2+1]=h[bytes[i]&15]; } result[n*2]=0; }
static int unhex(const char* s,unsigned char* bytes,size_t n)
{ static const char h[]="0123456789abcdef"; if(strlen(s)!=n*2) return 0; for(size_t i=0;i<n;++i) { const char *a=strchr(h,s[i*2]),*b=strchr(h,s[i*2+1]); if(!a || !b) return 0; bytes[i]=(unsigned char)(((a-h)<<4)|(b-h)); } return 1; }
static xvalue* snapshot(const xide_file_snapshot* s)
{ xvalue* o=xrtValueObject(); char hash[65]; hex(s->sha256,32,hash); if(!o || !number(o,"length",s->length) || !number(o,"index",s->index) || !number(o,"lastWrite",s->lastWrite) || !number(o,"volume",s->volume) || !number(o,"attributes",s->attributes) || !put(o,"sha256",hash)) { xrtValueRelease(o); return NULL; } return o; }
static int readSnapshot(const xvalue* o,xide_file_snapshot* s)
{ uint64_t volume=0,attributes=0; memset(s,0,sizeof(*s)); if(!readNumber(o,"length",&s->length) || !readNumber(o,"index",&s->index) || !readNumber(o,"lastWrite",&s->lastWrite) || !readNumber(o,"volume",&volume) || !readNumber(o,"attributes",&attributes) || volume>UINT32_MAX || attributes>UINT32_MAX || !unhex(jstring(o,"sha256"),s->sha256,32)) return 0; s->volume=(uint32_t)volume; s->attributes=(uint32_t)attributes; return 1; }
static int regular(const char* path,int missing)
{
 wchar_t* w=xidePathNativeWide(path); HANDLE h=w ? CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE;
 DWORD code=GetLastError(); free(w); if(h==INVALID_HANDLE_VALUE) return missing && (code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND);
 BY_HANDLE_FILE_INFORMATION i={0}; int okay=GetFileInformationByHandle(h,&i) && !(i.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) && i.nNumberOfLinks==1; CloseHandle(h); return okay;
}
static int exactLeaf(const char* path)
{
 wchar_t* w=xidePathNativeWide(path); WIN32_FIND_DATAW data={0}; HANDLE h=w ? FindFirstFileW(w,&data) : INVALID_HANDLE_VALUE;
 const wchar_t* leaf=w ? wcsrchr(w,L'\\') : NULL; int okay=h!=INVALID_HANDLE_VALUE && leaf && !wcscmp(leaf+1,data.cFileName);
 if(h!=INVALID_HANDLE_VALUE) { FindClose(h); } free(w); return okay;
}
static xide_rename_journal* openJournal(const char* directory,const char* base,xide_model_error* e)
{
 xide_rename_journal* j=calloc(1,sizeof(*j)); char* path=xideRecoveryPath(directory,"rename-journal.json",e);
 if(!j || !path || !regular(path,1) || !xideStateOpen(&j->marker,path,JOURNAL_FORMAT,e)) { free(path); xideRenameJournalDestroy(j); if(!e || !e->code) fail(e,XIDE_MODEL_CONFLICT,"改名恢复记录不可读或不是普通文件；原记录保留。"); return NULL; }
 free(path); j->directory=copy(directory); j->base=copy(base); if(!j->directory || !j->base) { xideRenameJournalDestroy(j); fail(e,XIDE_MODEL_MEMORY,"无法准备改名记录路径。"); return NULL; } return j;
}
void xideRenameJournalDestroy(xide_rename_journal* j) { if(j) { xideStateClear(&j->marker); free(j->directory); free(j->base); free(j->checkpointDirectory); free(j); } }
int xideRenameJournalPending(const xide_rename_journal* j) { return j && j->marker.existed && strcmp(jstring(j->marker.source,"state"),"retired"); }
static uint64_t journalNow(void)
{ FILETIME now; GetSystemTimeAsFileTime(&now); return ((uint64_t)now.dwHighDateTime<<32)|now.dwLowDateTime; }
static int bodyRef(const char* ref)
{
 if(strncmp(ref,"recovery/rename-",16) || strlen(ref)!=53 || strcmp(ref+48,".body")) return 0;
 for(size_t i=16;i<48;++i) if(!strchr("0123456789abcdef",ref[i])) return 0;
 return 1;
}
typedef struct owned_row { const char* ref; const xvalue* value; } owned_row;
static int ownedCompare(const void* a,const void* b)
{ return strcmp(((const owned_row*)a)->ref,((const owned_row*)b)->ref); }
/* Preserve all published ownership, even when a prepared operation is cancelled
   or a retired store is reused. Legacy records contribute their known blobs. */
static int mergeOwned(xvalue* root,const xvalue* source,xide_model_error* e)
{
 const xvalue* arrays[]={get(root,"ownedBodies"),get(source,"ownedBodies") ? get(source,"ownedBodies") : get(source,"blobs"),get(root,"blobs")};
 size_t count=0;
 for(size_t k=0;k<3;++k) if(arrays[k]) { if(!xrtValueIs(arrays[k],XVALUE_ARRAY) || xrtValueCount(arrays[k])>10000) return fail(e,XIDE_MODEL_INVALID,"项目迁移正文所有权清单无效，原文件保留。"); count+=xrtValueCount(arrays[k]); }
 owned_row* rows=calloc(count ? count : 1,sizeof(*rows)); xvalue* result=xrtValueArray(); int okay=rows && result; size_t at=0,unique=0; uint64_t total=0;
 for(size_t k=0;okay && k<3;++k) for(size_t i=0;i<xrtValueCount(arrays[k]);++i) {
  const xvalue* row=xrtValueArrayGet(arrays[k],i); xide_file_snapshot expected={0};
  okay=bodyRef(jstring(row,"ref")) && readSnapshot(row,&expected) && expected.length<=BODY_LIMIT && !(expected.attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
  if(!okay) break;
  rows[at++]=(owned_row){jstring(row,"ref"),row};
 }
 if(okay) qsort(rows,at,sizeof(*rows),ownedCompare);
 for(size_t i=0;okay && i<at;++i) {
  if(i && !strcmp(rows[i].ref,rows[i-1].ref)) { okay=xrtValueEqual(rows[i].value,rows[i-1].value); continue; }
  uint64_t size=0; okay=readNumber(rows[i].value,"length",&size) && ++unique<=10000 && size<=TOTAL_LIMIT-total;
  if(okay) { total+=size; okay=xrtValueArrayAppendNew(result,xrtValueDeepClone(rows[i].value)); }
 }
 if(okay) okay=xrtValueObjectSet(root,sv("ownedBodies"),result);
 free(rows); xrtValueRelease(result);
 return okay || fail(e,XIDE_MODEL_INVALID,"项目迁移正文所有权重复、超限或缺失，原文件保留。");
}
static unsigned char* readBlob(xide_rename_journal* j,const xvalue* root,const char* ref,size_t* length,xide_model_error* e);
static unsigned char* readOwnedBody(xide_rename_journal* j,const xvalue* row,const char* ref,size_t* length,xide_model_error* e);
static int bodyHash(const unsigned char* body,size_t n,unsigned char hash[32])
{
 BCRYPT_ALG_HANDLE algorithm=NULL; BCRYPT_HASH_HANDLE handle=NULL; DWORD size=0,got=0; unsigned char* object=NULL;
 int okay=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0 && BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&got,0)>=0;
 if(okay) { object=malloc(size); okay=object && BCryptCreateHash(algorithm,&handle,object,size,NULL,0,0)>=0 && BCryptHashData(handle,(PUCHAR)body,(ULONG)n,0)>=0 && BCryptFinishHash(handle,hash,32,0)>=0; }
 if(handle) BCryptDestroyHash(handle);
 if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
 free(object); return okay;
}
/* Charge every ordinary child, including unrecognized files, to this store.
   The caller holds its recovery lock; limits never authorize deleting a file. */
static int migrationCapacity(xide_rename_journal* j,size_t additional,xide_model_error* e)
{
 char* pattern=xidePathResolveLexical(j->directory,"recovery/*");
 wchar_t* wide=pattern ? xidePathNativeWide(pattern) : NULL; WIN32_FIND_DATAW data={0}; HANDLE find=wide ? FindFirstFileW(wide,&data) : INVALID_HANDLE_VALUE; DWORD code=GetLastError();
 int allocated=wide!=NULL; free(pattern); free(wide);
 if(!allocated) return fail(e,XIDE_MODEL_MEMORY,"无法核算项目迁移正文存储，已有草稿保留。");
 if(find==INVALID_HANDLE_VALUE) return code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND || fail(e,XIDE_MODEL_IO,"无法核算项目迁移正文存储，已有草稿保留。");
 uint64_t total=0; size_t count=0; int okay=1;
 do {
  if(!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..")) continue;
  uint64_t length=((uint64_t)data.nFileSizeHigh<<32)|data.nFileSizeLow;
  if(++count>=10000 || (data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || total>TOTAL_LIMIT || length>TOTAL_LIMIT-total) { okay=0; break; }
  total+=length;
 } while(FindNextFileW(find,&data));
 if(okay && GetLastError()!=ERROR_NO_MORE_FILES) { FindClose(find); return fail(e,XIDE_MODEL_IO,"项目迁移正文存储枚举中断，已有草稿保留。"); }
 FindClose(find);
 return (okay && additional<=TOTAL_LIMIT-total) || fail(e,XIDE_MODEL_INVALID,"项目迁移正文存储超过 512 MiB / 10000 文件上限，或含无法确认的目录；已有草稿保留。");
}
static char* blob(xide_rename_journal* j,xvalue* root,const unsigned char* body,size_t n,xide_model_error* e)
{
 if(n>BODY_LIMIT) { fail(e,XIDE_MODEL_INVALID,"改名恢复正文超过单文件 16 MiB 限制。"); return NULL; }
 if(!strcmp(jstring(root,"format"),MIGRATION_FORMAT)) {
  unsigned char hash[32]; if(!bodyHash(body,n,hash)) { fail(e,XIDE_MODEL_IO,"无法校验项目迁移恢复正文，已有草稿保留。"); return NULL; }
  char digest[65],size[32]; hex(hash,32,digest); snprintf(size,sizeof(size),"%llu",(unsigned long long)n);
  const xvalue* rows=get(root,"ownedBodies");
  for(size_t i=0;i<xrtValueCount(rows);++i) {
   const xvalue* row=xrtValueArrayGet(rows,i);
   if(strcmp(jstring(row,"length"),size) || strcmp(jstring(row,"sha256"),digest)) continue;
   int present=0; const xvalue* current=get(root,"blobs"); for(size_t k=0;k<xrtValueCount(current);++k) present|=!strcmp(jstring(xrtValueArrayGet(current,k),"ref"),jstring(row,"ref"));
   size_t length=0; xide_model_error cacheError={0}; unsigned char* existing=readOwnedBody(j,row,jstring(row,"ref"),&length,present ? e : &cacheError);
   int same=existing && length==n && !memcmp(existing,body,n); xrtFree(existing);
   if(!same && !present) continue; /* An obsolete cache entry is not a live checkpoint dependency. */
   if(!same) { if(!e || !e->code) fail(e,XIDE_MODEL_CONFLICT,"项目迁移正文去重校验失败，已有草稿保留。"); return NULL; }
   if(!present && !xrtValueArrayAppendNew(get(root,"blobs"),xrtValueDeepClone(row))) { fail(e,XIDE_MODEL_MEMORY,"无法复用已拥有的项目迁移正文。"); return NULL; }
   return copy(jstring(row,"ref"));
  }
  if(!migrationCapacity(j,n,e)) return NULL;
 }
 if(xrtValueCount(get(root,"blobs"))>=10000) { fail(e,XIDE_MODEL_INVALID,"改名恢复正文超过记录数量限制。"); return NULL; }
 GUID id; if(CoCreateGuid(&id)!=S_OK) { fail(e,XIDE_MODEL_IO,"无法生成独立改名恢复文件名。"); return NULL; }
 char token[33],ref[80]; hex((const unsigned char*)&id,16,token); snprintf(ref,sizeof(ref),"recovery/rename-%s.body",token);
 char* path=xideRecoveryPath(j->directory,ref,e); xide_file_snapshot s={0}; xvalue* row=NULL;
 int okay=path && !xidePathExists(path) && xideIoWriteAtomic(path,(xbytesview){body,n}) && regular(path,0) && xideFileSnapshotInspect(path,&s,e) && s.length==n;
 if(okay) {
  row=snapshot(&s); okay=row && put(row,"ref",ref);
  if(okay && !strcmp(jstring(root,"format"),MIGRATION_FORMAT)) okay=xrtValueArrayAppendNew(get(root,"ownedBodies"),xrtValueDeepClone(row));
  if(okay) { okay=xrtValueArrayAppendNew(get(root,"blobs"),row); row=NULL; }
 }
 xrtValueRelease(row); free(path); if(!okay) { if(!e || !e->code) fail(e,XIDE_MODEL_IO,"无法写入并校验改名恢复正文；磁盘改名尚未开始。"); return NULL; } return copy(ref);
}
static unsigned char* readBlob(xide_rename_journal* j,const xvalue* root,const char* ref,size_t* length,xide_model_error* e)
{
 const xvalue* row=NULL; const xvalue* rows=get(root,"blobs");
 if(!bodyRef(ref)) { fail(e,XIDE_MODEL_INVALID,"改名恢复正文引用无效。"); return NULL; }
 for(size_t i=0;i<xrtValueCount(rows);++i) if(!strcmp(jstring(xrtValueArrayGet(rows,i),"ref"),ref)) { if(row) { fail(e,XIDE_MODEL_INVALID,"改名恢复正文引用重复。"); return NULL; } row=xrtValueArrayGet(rows,i); }
 return readOwnedBody(j,row,ref,length,e);
}
static unsigned char* readOwnedBody(xide_rename_journal* j,const xvalue* row,const char* ref,size_t* length,xide_model_error* e)
{
 xide_file_snapshot expected={0},actual={0},after={0}; char* path=bodyRef(ref) ? xideRecoveryPath(j->directory,ref,e) : NULL; unsigned char* body=NULL;
 int okay=row && path && readSnapshot(row,&expected) && expected.length<=BODY_LIMIT && regular(path,0) && xideFileSnapshotInspect(path,&actual,e) && actual.length==expected.length && !memcmp(actual.sha256,expected.sha256,32);
 if(okay) body=xideIoReadAllLimit(path,BODY_LIMIT,length);
 if(body && (!xideFileSnapshotInspect(path,&after,e) || !xideFileSnapshotEqual(&actual,&after) || *length!=expected.length)) { xrtFree(body); body=NULL; }
 free(path); if(!body && (!e || !e->code)) fail(e,XIDE_MODEL_CONFLICT,"改名恢复正文缺失或校验失败；原记录保留。"); return body;
}
static xvalue* ownCheckpoint(xide_rename_journal* j,xvalue* root,const xvalue* checkpoint,xide_model_error* e)
{
 xvalue* result=xrtValueDeepClone(checkpoint); size_t total=0; const char* keys[]={"recoveryText","recoveryBaseline"};
 if(!result || !xideSessionValidate(result)) { xrtValueRelease(result); fail(e,XIDE_MODEL_INVALID,"改名前会话检查点无效。"); return NULL; }
 xvalue* docs=get(result,"openDocuments"); int okay=1;
 for(size_t i=0;okay && i<xrtValueCount(docs);++i) for(int k=0;okay && k<2;++k) {
  xvalue* entry=xrtValueArrayGet(docs,i); const char* ref=text(get(entry,keys[k])); if(!ref) continue;
  char* path=xideRecoveryPath(j->checkpointDirectory ? j->checkpointDirectory : j->directory,ref,e); size_t n=0; unsigned char* bytes=path && regular(path,0) ? xideIoReadAllLimit(path,BODY_LIMIT,&n) : NULL; free(path);
  if(!bytes || n>TOTAL_LIMIT-total) { xrtFree(bytes); okay=fail(e,XIDE_MODEL_IO,"无法保留完整改名前会话正文，或总量超过 512 MiB。"); break; } total+=n;
  char* own=blob(j,root,bytes,n,e); xrtFree(bytes); okay=own && put(entry,keys[k],own); free(own);
 }
 if(!okay) { xrtValueRelease(result); return NULL; } return result;
}
/* Store normalized live data separately from the original disk JSON baseline.
   A synthetic context supplies a stable base for unnamed/standalone workspaces. */
static xvalue* captureModel(const xide_workspace* w,const char* directory,xide_model_error* e)
{
 xvalue* root=xrtValueObject(); xvalue* rows=xrtValueArray(); char* context=w->manifest ? copy(w->manifest) : xidePathResolveLexical(directory,"journal-workspace.xlw");
 int okay=root && rows && context && put(root,"manifest",w->manifest) && put(root,"context",context) && set(root,"loaded",xrtValueBool(w->source!=NULL)) && set(root,"dirty",xrtValueBool(w->dirty!=0));
 if(okay && w->source) { xide_workspace named=*w; named.manifest=context; xvalue* data=xideWorkspaceExport(&named,e); okay=data && set(root,"data",data) && xrtValueObjectSet(root,sv("baseline"),w->source); }
 for(size_t i=0;okay && i<w->projectCount;++i) {
  const xide_project* p=&w->projects[i]; xvalue* row=xrtValueObject(); xvalue* data=p->available ? xideProjectExport(p,e) : NULL;
  okay=row && put(row,"id",p->id) && put(row,"manifest",p->manifest) && set(row,"available",xrtValueBool(p->available!=0)) && set(row,"dirty",xrtValueBool(p->dirty!=0)) && (!p->source || xrtValueObjectSet(row,sv("baseline"),p->source)) && (!p->available || (data && xrtValueObjectSet(row,sv("data"),data)));
  xrtValueRelease(data); if(okay) okay=xrtValueArrayAppendNew(rows,row); else xrtValueRelease(row);
 }
 if(okay) { okay=xrtValueObjectSet(root,sv("projects"),rows); } xrtValueRelease(rows); free(context);
 if(!okay) { xrtValueRelease(root); if(!e || !e->code) fail(e,XIDE_MODEL_MEMORY,"无法保留完整工程改名状态。"); return NULL; } return root;
}
static int decodeModel(const xvalue* root,xide_workspace* w,xide_model_error* e)
{
 if(!xrtValueIs(root,XVALUE_OBJECT) || !text(get(root,"manifest")) || !*jstring(root,"context") || !xrtValueIs(get(root,"projects"),XVALUE_ARRAY)) return fail(e,XIDE_MODEL_INVALID,"改名工程记录无效。");
 if(!flag(root,"loaded")) return xrtValueCount(get(root,"projects"))==0;
 if(!xideWorkspaceDecode(w,jstring(root,"context"),get(root,"data"),e)) return 0;
 if(xrtValueCount(get(root,"projects"))!=w->projectCount) return fail(e,XIDE_MODEL_INVALID,"改名工程数量不匹配。");
 for(size_t i=0;i<w->projectCount;++i) {
  const xvalue* row=xrtValueArrayGet(get(root,"projects"),i); xide_project* p=&w->projects[i];
  if(strcmp(p->id,jstring(row,"id")) || !xidePathLexicalEqual(p->manifest,jstring(row,"manifest"))) return fail(e,XIDE_MODEL_INVALID,"改名工程引用不匹配。");
  if(flag(row,"available")) {
   char* id=copy(p->id); xvalue* reference=xrtValueDeepClone(p->referenceSource);
   int okay=id && reference && xideProjectDecode(p,jstring(row,"manifest"),get(row,"data"),e);
   if(!okay) { free(id); xrtValueRelease(reference); return 0; } p->id=id; p->referenceSource=reference;
   xrtValueRelease(p->source); p->source=xrtValueDeepClone(get(row,"baseline")); if(!p->source) return fail(e,XIDE_MODEL_INVALID,"改名工程磁盘基线缺失。");
   p->dirty=flag(row,"dirty") || !xrtValueEqual(get(row,"data"),get(row,"baseline"));
  }
 }
 xrtValueRelease(w->source); w->source=xrtValueDeepClone(get(root,"baseline")); if(!w->source) return fail(e,XIDE_MODEL_INVALID,"改名工作空间基线缺失。");
 w->dirty=flag(root,"dirty") || !xrtValueEqual(get(root,"data"),get(root,"baseline"));
 if(!*jstring(root,"manifest")) { free(w->manifest); w->manifest=NULL; } return 1;
}
static xvalue* readJson(const char* path)
{
 size_t n=0; unsigned char* body=xideIoReadAllLimit(path,8u*1024u*1024u,&n); xvalue* v=NULL; size_t offset=n>=3 && body && body[0]==0xef && body[1]==0xbb && body[2]==0xbf ? 3 : 0;
 if(body && n>offset && !memchr(body+offset,0,n-offset) && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(char*)body+offset,(int)(n-offset),NULL,0)) { v=xrtJsonParse((xstrview){(char*)body+offset,n-offset}); } xrtFree(body); return v;
}
static int recognizedModel(xvalue* root,xide_model_error* e)
{
 if(!flag(root,"loaded")) return 1;
 const char* manifest=jstring(root,"manifest");
 if(*manifest) {
  xvalue* disk=readJson(manifest); int okay=disk && (xrtValueEqual(disk,get(root,"baseline")) || xrtValueEqual(disk,get(root,"data"))) && xrtValueObjectSet(root,sv("baseline"),disk); xrtValueRelease(disk);
  if(!okay) return fail(e,XIDE_MODEL_CONFLICT,"工作空间清单已被修改；改名记录保留，请先处理清单冲突。");
 }
 xvalue* rows=get(root,"projects");
 for(size_t i=0;i<xrtValueCount(rows);++i) {
  xvalue* row=xrtValueArrayGet(rows,i); if(!flag(row,"available")) continue;
  xvalue* disk=readJson(jstring(row,"manifest")); int okay=disk && (xrtValueEqual(disk,get(row,"baseline")) || xrtValueEqual(disk,get(row,"data"))) && xrtValueObjectSet(row,sv("baseline"),disk); xrtValueRelease(disk);
  if(!okay) return fail(e,XIDE_MODEL_CONFLICT,"项目清单已被修改；改名记录保留，请先处理清单冲突。");
 } return 1;
}
static int writeMarker(xide_rename_journal* j,xvalue* root,xide_model_error* e)
{
 if(!strcmp(jstring(root,"format"),MIGRATION_FORMAT)) {
  if(!mergeOwned(root,j->marker.source,e)) return 0;
  if(!strcmp(jstring(root,"state"),"retired")) { uint64_t at=0; if(!readNumber(root,"retiredAt",&at) || !at) { if(!number(root,"retiredAt",journalNow())) return 0; } }
  else xrtValueObjectRemove(root,sv("retiredAt"));
 }
 /* Retain only payload references in this generation. Older immutable files
    keep the store's existing grace period, without bloating the live marker. */
 xvalue* kept=xrtValueArray(); const xvalue* rows=get(root,"blobs"); int okay=kept!=NULL;
 for(size_t i=0;okay && i<xrtValueCount(rows);++i) {
  const xvalue* row=xrtValueArrayGet(rows,i); const char* ref=jstring(row,"ref"); int used=0;
  const xvalue* resources=get(root,"resources"); for(size_t k=0;k<xrtValueCount(resources);++k) { const xvalue* r=xrtValueArrayGet(resources,k); used|=!strcmp(jstring(r,"before"),ref) || !strcmp(jstring(r,"desired"),ref); }
  const xvalue* docs=get(get(root,"session"),"openDocuments"); for(size_t k=0;k<xrtValueCount(docs);++k) { const xvalue* d=xrtValueArrayGet(docs,k); used|=!strcmp(jstring(d,"recoveryText"),ref) || !strcmp(jstring(d,"recoveryBaseline"),ref); }
  if(used) okay=xrtValueArrayAppendNew(kept,xrtValueDeepClone(row));
 }
 if(okay) { okay=xrtValueObjectSet(root,sv("blobs"),kept); } xrtValueRelease(kept);
 return okay && regular(j->marker.path,1) && xideStateWrite(&j->marker,root,e);
}
static int pathState(const xvalue* root,xide_model_error* e)
{
 const char *old=jstring(root,"source"),*next=jstring(root,"target"); xide_file_snapshot expected={0},actual={0};
 if(!*old || !*next || !readSnapshot(get(root,"file"),&expected)) { fail(e,XIDE_MODEL_INVALID,"改名磁盘身份记录无效。"); return -1; }
 char *oldAbsolute=xidePathResolveLexical(NULL,old),*nextAbsolute=xidePathResolveLexical(NULL,next),*oldParent=xidePathDirectory(old),*nextParent=xidePathDirectory(next);
 int valid=oldAbsolute && nextAbsolute && !strcmp(oldAbsolute,old) && !strcmp(nextAbsolute,next) && oldParent && nextParent && xidePathLexicalEqual(oldParent,nextParent) && xideFileRenameTargetValid(xidePathFilename(next),e);
 free(oldAbsolute); free(nextAbsolute); free(oldParent); free(nextParent); if(!valid) { fail(e,XIDE_MODEL_INVALID,"改名记录必须包含同一目录内的完整文件路径。"); return -1; }
 int source=xidePathExists(old) && exactLeaf(old),target=xidePathExists(next) && exactLeaf(next);
 if(target && !source && xideFileSnapshotInspect(next,&actual,e) && xideFileSnapshotEqual(&expected,&actual)) return 1;
 /* An unchanged original source proves no move even when another file appeared
    at the destination. The checked rename never replaces that competing file. */
 if(source && !strcmp(jstring(root,"state"),"prepared") && xideFileSnapshotInspect(old,&actual,e) && xideFileSnapshotEqual(&expected,&actual)) return 0;
 fail(e,XIDE_MODEL_CONFLICT,"改名前后文件身份无法确认；改名恢复记录保留，请先处理磁盘冲突。"); return -1;
}
int xideRenameJournalCancelPrepared(xide_rename_journal* j,xide_model_error* e)
{
 if(!xideRenameJournalPending(j) || strcmp(jstring(j->marker.source,"state"),"prepared")) return 1;
 xide_recovery_guard guard={0}; if(!xideRecoveryLock(&guard,j->directory,e)) return 0;
 int okay=pathState(j->marker.source,e)==0; const xvalue* previous=get(j->marker.source,"previous"); xvalue* root=okay ? xrtValueDeepClone(previous ? previous : j->marker.source) : NULL;
 if(okay) okay=root && (previous || put(root,"state","retired")) && writeMarker(j,root,e);
 xrtValueRelease(root); xideRecoveryUnlock(&guard); return okay;
}
xide_rename_journal* xideRenameJournalPrepare(xide_app* a,const char* directory,const char* base,const xvalue* checkpoint,const char* source,const char* target,const xide_resource_rebind* resources,xide_model_error* e)
{
 xide_recovery_guard guard={0}; xide_rename_journal* j=NULL; xvalue *root=NULL,*model=NULL,*rows=NULL,*owned=NULL; xide_workspace clone={0}; xide_file_rebind* plan=NULL; int okay=0; xide_file_snapshot file={0};
 if(!xideRecoveryLock(&guard,directory,e)) return NULL;
 j=openJournal(directory,base,e); if(!j) goto done;
 int previous=xideRenameJournalPending(j);
 if(previous && (strcmp(jstring(j->marker.source,"state"),"active") || strcmp(jstring(j->marker.source,"owner"),owner(a)))) { fail(e,XIDE_MODEL_CONFLICT,"上次改名尚待恢复或不属于当前工作空间。请先处理恢复状态。"); goto done; }
 root=previous ? xrtValueDeepClone(j->marker.source) : xrtValueObject(); rows=previous ? xrtValueDeepClone(get(root,"resources")) : xrtValueArray(); model=captureModel(&a->workspaceModel,directory,e);
 if(!root || !rows || !model || (!previous && (!set(root,"blobs",xrtValueArray()) || !set(root,"moves",xrtValueArray()))) || !put(root,"format",JOURNAL_FORMAT) || !put(root,"state","prepared") || !put(root,"owner",owner(a))) goto done;
 if(previous) {
  xvalue* move=xrtValueObject(); xvalue* saved=xrtValueDeepClone(j->marker.source);
  int good=move && saved && put(move,"path",jstring(j->marker.source,"target")) && xrtValueObjectSet(move,sv("file"),get(j->marker.source,"file")) && xrtValueArrayAppendNew(get(root,"moves"),move); move=NULL;
  if(good) { good=xrtValueObjectSet(root,sv("previous"),saved); } xrtValueRelease(saved); xrtValueRelease(move); if(!good || xrtValueCount(get(root,"moves"))>10000) goto done;
  xvalue* moves=get(root,"moves"); for(size_t i=0;i<xrtValueCount(moves);++i) { xvalue* m=xrtValueArrayGet(moves,i); if(xidePathLexicalEqual(jstring(m,"path"),source) && !put(m,"path",target)) goto done; }
  for(size_t i=0;i<xrtValueCount(rows);++i) { xvalue* row=xrtValueArrayGet(rows,i); if(xidePathLexicalEqual(jstring(row,"path"),source) && !put(row,"path",target)) goto done; }
 }
 if(!put(root,"source",source) || !put(root,"target",target) || !xideFileSnapshotInspect(source,&file,e) || !set(root,"file",snapshot(&file))) goto done;
 if(!decodeModel(model,&clone,e) || !xideFileRebindPrepare(&clone,source,target,&plan,e)) goto done;
 xideFileRebindApply(plan); xrtValueRelease(model); model=captureModel(&clone,directory,e); if(!model || !xrtValueObjectSet(root,sv("model"),model)) goto done;
 for(size_t i=0;i<xideResourceRebindCount(resources);++i) {
  const char *before=xideResourceRebindBefore(resources,i),*after=xideResourceRebindAfter(resources,i); xvalue* row=xrtValueObject();
  char* b=before ? blob(j,root,(const unsigned char*)before,strlen(before),e) : NULL; char* n=after ? blob(j,root,(const unsigned char*)after,strlen(after),e) : NULL;
  int good=row && b && n && put(row,"path",xideResourceRebindPath(resources,i)) && put(row,"before",b) && put(row,"desired",n) && set(row,"pending",xrtValueBool(true)) && set(row,"disk",snapshot(xideResourceRebindDisk(resources,i))); free(b); free(n);
  size_t existing=SIZE_MAX; for(size_t k=0;k<xrtValueCount(rows);++k) if(xidePathLexicalEqual(jstring(xrtValueArrayGet(rows,k),"path"),xideResourceRebindPath(resources,i))) existing=k;
  if(good) good=existing==SIZE_MAX ? xrtValueArrayAppendNew(rows,row) : xrtValueArraySetNew(rows,existing,row); else xrtValueRelease(row); if(!good) goto done;
 }
 if(!xrtValueObjectSet(root,sv("resources"),rows)) goto done;
 owned=ownCheckpoint(j,root,checkpoint,e); if(!owned || !xrtValueObjectSet(root,sv("session"),owned)) goto done;
 okay=writeMarker(j,root,e);
done:
 xideFileRebindDestroy(plan); xideWorkspaceClear(&clone); xrtValueRelease(owned); xrtValueRelease(root); xrtValueRelease(model); xrtValueRelease(rows); xideRecoveryUnlock(&guard);
 if(!okay) { xideRenameJournalDestroy(j); if(!e || !e->code) fail(e,XIDE_MODEL_MEMORY,"无法完整发布改名恢复记录；原文件尚未改名。"); return NULL; } return j;
}
static int captureLive(xide_rename_journal* j,xide_app* a,xvalue* root,const xide_resource_rebind* plan,xide_model_error* e)
{
 xvalue* model=captureModel(&a->workspaceModel,j->directory,e); int okay=model && xrtValueObjectSet(root,sv("model"),model); xrtValueRelease(model);
 char* target=xidePathResolve(NULL,jstring(root,"target")); xide_file_snapshot file={0}; if(okay) okay=target && put(root,"target",target) && xideFileSnapshotInspect(target,&file,e) && set(root,"file",snapshot(&file)); free(target);
 xvalue* moves=get(root,"moves");
 for(size_t i=0;okay && i<xrtValueCount(moves);++i) { xvalue* m=xrtValueArrayGet(moves,i); char* actual=xidePathResolve(NULL,jstring(m,"path")); xide_file_snapshot disk={0}; okay=actual && put(m,"path",actual) && xideFileSnapshotInspect(actual,&disk,e) && set(m,"file",snapshot(&disk)); free(actual); }
 xvalue* rows=get(root,"resources");
 for(size_t i=0;okay && i<xrtValueCount(rows);++i) {
  xvalue* row=xrtValueArrayGet(rows,i); xide_document* d=xideDocumentsFind(&a->documents,jstring(row,"path"));
  size_t planIndex=SIZE_MAX;
  for(size_t k=0;plan && k<xideResourceRebindCount(plan);++k) if(xidePathLexicalEqual(jstring(row,"path"),xideResourceRebindPath(plan,k))) planIndex=k;
  int pending=planIndex!=SIZE_MAX ? !xideResourceRebindApplied(plan,planIndex) : j->recovered==2 ? 0 : flag(row,"pending");
  if(planIndex!=SIZE_MAX || j->recovered==2) okay=set(row,"pending",xrtValueBool(pending!=0));
  if(!pending && d && d->open) { const char* body=xuiCodeEditGetText(d->editor); size_t n=0; unsigned char* prior=readBlob(j,root,jstring(row,"desired"),&n,e);
   int same=prior && strlen(body)==n && !memcmp(prior,body,n); xrtFree(prior); if(!same) { char* ref=blob(j,root,(const unsigned char*)body,strlen(body),e); okay=ref && put(row,"desired",ref); free(ref); }
  }
 } return okay;
}
int xideRenameJournalFinish(xide_rename_journal* j,xide_app* a,const xide_resource_rebind* resources,xide_model_error* e)
{
 if(!j) { return 1; } xide_recovery_guard guard={0}; if(!xideRecoveryLock(&guard,j->directory,e)) return 0;
 xvalue* root=xrtValueDeepClone(j->marker.source); if(root) xrtValueObjectRemove(root,sv("previous")); int okay=root && put(root,"state","active") && captureLive(j,a,root,resources,e) && writeMarker(j,root,e);
 xrtValueRelease(root); xideRecoveryUnlock(&guard); return okay;
}
static int savedResources(xide_rename_journal* j,const xvalue* root,xide_model_error* e)
{
 const xvalue* rows=get(root,"resources");
 for(size_t i=0;i<xrtValueCount(rows);++i) {
  const xvalue* row=xrtValueArrayGet(rows,i); if(flag(row,"pending")) return 0; size_t n=0; unsigned char* desired=readBlob(j,root,jstring(row,"desired"),&n,e); xide_text_file disk={0};
  int okay=desired && xideTextFileRead(jstring(row,"path"),&disk,e) && strlen(disk.text)==n && !memcmp(disk.text,desired,n); xrtFree(desired); xideTextFileClear(&disk); if(!okay) return 0;
 } return 1;
}
int xideRenameJournalCheckpoint(xide_rename_journal* j,xide_app* a,const xvalue* checkpoint,xide_recovery_guard* guard,xide_model_error* e)
{
 if(!xideRenameJournalPending(j) || (!strcmp(jstring(j->marker.source,"state"),"prepared") && j->recovered!=2)) return 1;
 if(!guard || !guard->lockHandle || !xidePathLexicalEqual(guard->directory,j->directory)) return fail(e,XIDE_MODEL_INVALID,"改名检查点需要同一会话锁。");
 xvalue* root=xrtValueDeepClone(j->marker.source); if(root) xrtValueObjectRemove(root,sv("previous")); xvalue* owned=NULL; int okay=root && put(root,"state","active") && captureLive(j,a,root,NULL,e);
 if(okay) { owned=ownCheckpoint(j,root,checkpoint,e); okay=owned && xrtValueObjectSet(root,sv("session"),owned); }
 int saved=!a->workspaceModel.dirty; for(size_t i=0;saved && i<a->workspaceModel.projectCount;++i) saved=!a->workspaceModel.projects[i].dirty;
 xide_model_error inspect={0}; if(okay && saved && savedResources(j,root,&inspect)) okay=put(root,"state","retired");
 if(okay) { okay=writeMarker(j,root,e); } xrtValueRelease(owned); xrtValueRelease(root); return okay;
}
static int validateResources(xide_rename_journal* j,const xvalue* root,xide_model_error* e)
{
 const xvalue* rows=get(root,"resources"); if(!xrtValueIs(rows,XVALUE_ARRAY) || xrtValueCount(rows)>10000) return fail(e,XIDE_MODEL_INVALID,"改名资源记录无效。");
 size_t total=0;
 for(size_t i=0;i<xrtValueCount(rows);++i) {
  const xvalue* row=xrtValueArrayGet(rows,i); size_t n=0,bn=0; unsigned char *desired=readBlob(j,root,jstring(row,"desired"),&n,e),*before=readBlob(j,root,jstring(row,"before"),&bn,e); xide_file_snapshot expected={0},actual={0}; xide_text_file disk={0};
  int okay=desired && before && !memchr(desired,0,n) && !memchr(before,0,bn) && n<=TOTAL_LIMIT-total && (total+=n,bn<=TOTAL_LIMIT-total) && (total+=bn,readSnapshot(get(row,"disk"),&expected)) && xideFileSnapshotInspect(jstring(row,"path"),&actual,e);
  if(okay && !xideFileSnapshotEqual(&actual,&expected)) okay=xideTextFileRead(jstring(row,"path"),&disk,e) && strlen(disk.text)==n && !memcmp(disk.text,desired,n);
  xrtFree(desired); xrtFree(before); xideTextFileClear(&disk);
  if(!okay) return fail(e,XIDE_MODEL_CONFLICT,"资源配置或恢复正文已被修改；改名记录保留，请先处理资源冲突。");
 } return 1;
}
static int mapCheckpoint(xide_rename_journal* j,const xvalue* root,xvalue* checkpoint,int owned,xide_model_error* e)
{
 const char *old=jstring(root,"source"),*next=jstring(root,"target"); xvalue* docs=get(checkpoint,"openDocuments"); size_t total=0;
 for(size_t i=0;i<xrtValueCount(docs);++i) {
  xvalue* entry=xrtValueArrayGet(docs,i); const char* keys[]={"recoveryText","recoveryBaseline"};
  for(int k=0;k<2;++k) if(get(entry,keys[k])) { size_t n=0; unsigned char* bytes=NULL;
   if(owned) bytes=readBlob(j,root,jstring(entry,keys[k]),&n,e);
   else { char* path=xideRecoveryPath(j->directory,jstring(entry,keys[k]),e); if(path && regular(path,0)) bytes=xideIoReadAllLimit(path,BODY_LIMIT,&n); free(path); }
   int okay=bytes && n<=TOTAL_LIMIT-total; xrtFree(bytes); if(!okay) return fail(e,XIDE_MODEL_CONFLICT,"改名会话正文不完整；原记录保留。"); total+=n;
  }
  if(*jstring(entry,"path")) { char* path=xidePathResolveLexical(j->base,jstring(entry,"path")); int moved=path && xidePathLexicalEqual(path,old); free(path);
   if(moved) { char* relative=xidePathRelative(j->base,next); int okay=relative && put(entry,"path",relative) && put(entry,"title",xidePathFilename(next)) && (flag(entry,"languageOverride") || put(entry,"language",xideLanguageForPath(next))); free(relative); if(!okay) return fail(e,XIDE_MODEL_MEMORY,"无法恢复改名文档绑定。"); }
  }
 }
 if(*jstring(checkpoint,"activeDocument")) { char* path=xidePathResolveLexical(j->base,jstring(checkpoint,"activeDocument")); int moved=path && xidePathLexicalEqual(path,old); free(path); if(moved) { char* relative=xidePathRelative(j->base,next); int okay=relative && put(checkpoint,"activeDocument",relative); free(relative); if(!okay) return 0; } }
 return xideSessionValidate(checkpoint);
}
int xideRenameJournalRecover(xide_app* a,const char* directory,const char* base,const xvalue* currentCheckpoint,xide_rename_journal** out,xvalue** checkpoint,xide_model_error* e)
{
 *out=NULL; *checkpoint=NULL; char* path=xideRecoveryPath(directory,"rename-journal.json",e); int exists=path && xidePathExists(path); free(path); if(!exists) return 1;
 xide_recovery_guard guard={0}; if(!xideRecoveryLock(&guard,directory,e)) return 0;
 xide_rename_journal* j=openJournal(directory,base,e); xvalue *root=NULL,*owned=NULL; xide_workspace model={0}; int okay=0;
 if(!j) { goto done; } if(!xideRenameJournalPending(j)) { okay=1; goto done; }
 if(strcmp(jstring(j->marker.source,"state"),"prepared") && strcmp(jstring(j->marker.source,"state"),"active")) { fail(e,XIDE_MODEL_INVALID,"未知改名事务状态，原记录保留。"); goto done; }
 if(strcmp(jstring(j->marker.source,"owner"),owner(a))) { fail(e,XIDE_MODEL_CONFLICT,"改名记录不属于当前工作空间，原记录保留。"); goto done; }
 int moved=pathState(j->marker.source,e); if(moved<0) goto done;
 if(!moved && get(j->marker.source,"previous")) {
  xvalue* previous=xrtValueDeepClone(get(j->marker.source,"previous"));
  int valid=previous && !strcmp(jstring(previous,"state"),"active") && !get(previous,"previous") && writeMarker(j,previous,e); xrtValueRelease(previous); if(!valid) goto done;
  moved=pathState(j->marker.source,e); if(moved<0) goto done;
 }
 root=xrtValueDeepClone(j->marker.source); if(!root) goto done;
 if(!moved) { okay=put(root,"state","retired") && writeMarker(j,root,e); goto done; }
 if(strcmp(jstring(get(root,"model"),"manifest"),a->workspaceModel.manifest ? a->workspaceModel.manifest : "")) { fail(e,XIDE_MODEL_CONFLICT,"改名工程记录不属于当前工作空间，原记录保留。"); goto done; }
 const xvalue* moves=get(root,"moves"); if(!xrtValueIs(moves,XVALUE_ARRAY) || xrtValueCount(moves)>10000) { fail(e,XIDE_MODEL_INVALID,"连续改名身份记录无效。"); goto done; }
 for(size_t i=0;i<xrtValueCount(moves);++i) { const xvalue* m=xrtValueArrayGet(moves,i); xide_file_snapshot expected={0},disk={0}; if(!readSnapshot(get(m,"file"),&expected) || !exactLeaf(jstring(m,"path")) || !xideFileSnapshotInspect(jstring(m,"path"),&disk,e) || !xideFileSnapshotEqual(&expected,&disk)) { fail(e,XIDE_MODEL_CONFLICT,"连续改名文件身份无法确认；原事务保留。"); goto done; } }
 if(!recognizedModel(get(root,"model"),e) || !decodeModel(get(root,"model"),&model,e) || !validateResources(j,root,e)) goto done;
 owned=xrtValueDeepClone(get(root,"session")); if(!owned || !mapCheckpoint(j,root,owned,1,e)) goto done;
 /* session.json is the authoritative current checkpoint. A different valid
    paired layout generation may have published before the journal update
    failed; never replace those newer editor drafts with an older journal. */
 const xvalue* saved=get(root,"session");
 int newer=currentCheckpoint && xideSessionValidate(currentCheckpoint) && *jstring(currentCheckpoint,"layoutGeneration") && strcmp(jstring(currentCheckpoint,"layoutGeneration"),jstring(saved,"layoutGeneration")) && !strcmp(jstring(currentCheckpoint,"ownerKind"),jstring(saved,"ownerKind")) && !strcmp(jstring(currentCheckpoint,"ownerPath"),jstring(saved,"ownerPath"));
 if(newer) { xvalue* fresh=xrtValueDeepClone(currentCheckpoint); if(!fresh || !mapCheckpoint(j,root,fresh,0,e)) { xrtValueRelease(fresh); goto done; } xrtValueRelease(owned); owned=fresh; j->newerCheckpoint=1; j->newerPending=flag(currentCheckpoint,"renameResourcesPending"); }
 /* Complete validation/allocation precedes the sole live-model replacement. */
 xideWorkspaceClear(&a->workspaceModel); a->workspaceModel=model; memset(&model,0,sizeof(model)); j->recovered=1; *out=j; j=NULL; *checkpoint=owned; owned=NULL; okay=1;
done:
 xideWorkspaceClear(&model); xrtValueRelease(root); xrtValueRelease(owned); xideRenameJournalDestroy(j); xideRecoveryUnlock(&guard); return okay;
}
int xideRenameJournalApplyRecovered(xide_rename_journal* j,xide_app* a,xide_model_error* e)
{
 if(!j || !j->recovered) { return 1; } const xvalue* root=j->marker.source; const xvalue* rows=get(root,"resources");
 /* Reject all stale editor bodies before applying any patch. A checkpointed
    later edit (including undo) is the desired body, not overwritten by replay. */
 for(size_t i=0;i<xrtValueCount(rows);++i) {
  const xvalue* row=xrtValueArrayGet(rows,i); xide_document* d=xideDocumentsFind(&a->documents,jstring(row,"path")); size_t n=0,bn=0; unsigned char *desired=readBlob(j,root,jstring(row,"desired"),&n,e),*before=readBlob(j,root,jstring(row,"before"),&bn,e);
  const char* body=d && d->open ? xuiCodeEditGetText(d->editor) : NULL; int same=body && desired && strlen(body)==n && !memcmp(body,desired,n); int patch=body && before && strlen(body)==bn && !memcmp(body,before,bn);
  int current=j->newerCheckpoint && body; xrtFree(desired); xrtFree(before); if(!same && !patch && !current) return fail(e,XIDE_MODEL_CONFLICT,"工程引用已恢复，但资源草稿无法自动合并。原事务保留，请先处理资源文档。");
 }
 for(size_t i=0;i<xrtValueCount(rows);++i) {
  const xvalue* row=xrtValueArrayGet(rows,i); xide_document* d=xideDocumentsFind(&a->documents,jstring(row,"path")); size_t n=0; unsigned char* desired=readBlob(j,root,jstring(row,"desired"),&n,e);
  if(j->newerCheckpoint) {
   size_t bn=0; unsigned char* before=readBlob(j,root,jstring(row,"before"),&bn,e); const char* body=xuiCodeEditGetText(d->editor);
   int pending=j->newerPending && flag(row,"pending") && before && strlen(body)==bn && !memcmp(body,before,bn); xrtFree(before);
   if(!pending) { xrtFree(desired); continue; }
  }
  if(!desired) { return 0; } xui_code_document core=xuiCodeEditGetDocument(d->editor); int okay=!strcmp(xuiCodeEditGetText(d->editor),(const char*)desired) || xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),(const char*)desired)==XUI_OK; xrtFree(desired);
  if(!okay) return fail(e,XIDE_MODEL_MEMORY,"工程引用已恢复，资源草稿仍待恢复；原事务保留。");
 }
 j->recovered=2; return 1;
}

/* Project migration uses the same bounded, immutable payload mechanism, in a
   stable global directory. It must be discoverable before loading old paths. */
#define MIGRATION_RECORD_LIMIT 1024
static xide_migration_journal* migrationOpen(const char* directory,xide_model_error* e)
{
 xide_migration_journal* j=calloc(1,sizeof(*j)); char* path=xidePathExists(directory) ? xideRecoveryPath(directory,"migration-journal.json",e) : xidePathResolveLexical(directory,"migration-journal.json");
 if(!j || !path || !regular(path,1) || !xideStateOpen(&j->marker,path,MIGRATION_FORMAT,e)) { free(path); xideRenameJournalDestroy(j); return NULL; }
 free(path); j->directory=copy(directory); j->base=copy(directory);
 if(!j->directory || !j->base) { xideRenameJournalDestroy(j); fail(e,XIDE_MODEL_MEMORY,"无法准备项目迁移记录目录。"); return NULL; } return j;
}
static int migrationMatches(const xvalue* root,const char* path,int projectOnly)
{
 const char* kind=projectOnly ? "project" : *path ? "workspace" : "none";
 if(strcmp(jstring(root,"ownerKind"),kind)) return 0;
 if((!*path && !*jstring(root,"ownerAfter")) || (*path && xidePathLexicalEqual(jstring(root,"ownerAfter"),path))) return 1;
 if(!strcmp(jstring(root,"state"),"retired")) return 0;
 const xvalue* aliases=get(root,"owners");
 for(size_t i=0;i<xrtValueCount(aliases);++i) { const char* s=text(xrtValueArrayGet(aliases,i)); if(s && ((!*s && !*path) || (*s && *path && xidePathLexicalEqual(s,path)))) return 1; }
 return 0;
}
static int migrationFind(const char* global,const char* path,int projectOnly,xide_migration_journal** result,xide_model_error* e)
{
 *result=NULL; char* root=xidePathResolveLexical(global,"migrations"),*pattern=root ? xidePathResolveLexical(root,"*") : NULL;
 wchar_t* wide=pattern ? xidePathNativeWide(pattern) : NULL; WIN32_FIND_DATAW data={0}; HANDLE h=wide ? FindFirstFileW(wide,&data) : INVALID_HANDLE_VALUE; DWORD code=GetLastError(); free(wide); free(pattern);
 if(h==INVALID_HANDLE_VALUE) { free(root); return code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND || fail(e,XIDE_MODEL_IO,"无法枚举项目迁移记录；原记录保留。"); }
 int okay=1,count=0;
 do {
  if(!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..")) continue;
  if(!wcscmp(data.cFileName,L"maintenance.lock") || !wcscmp(data.cFileName,L"maintenance.json") || !wcscmp(data.cFileName,L"maintenance.json.lock")) continue;
  if(++count>MIGRATION_RECORD_LIMIT) { okay=fail(e,XIDE_MODEL_INVALID,"项目迁移记录超过 1024 项，请保留记录并整理存储。"); break; }
  if(wcslen(data.cFileName)!=16 || wcsspn(data.cFileName,L"0123456789abcdef")!=16) continue;
  if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) { okay=fail(e,XIDE_MODEL_CONFLICT,"项目迁移目录被替换，原记录保留。"); break; }
  char leaf[17]; for(int i=0;i<16;++i) leaf[i]=(char)data.cFileName[i]; leaf[16]=0;
  char* directory=xidePathResolveLexical(root,leaf); xide_migration_journal* j=directory ? migrationOpen(directory,e) : NULL; free(directory);
  if(!j) { okay=0; break; }
  if(j->marker.existed && migrationMatches(j->marker.source,path,projectOnly)) {
   if(*result) { xideRenameJournalDestroy(j); okay=fail(e,XIDE_MODEL_CONFLICT,"同一工作空间存在多个迁移记录；原记录保留。"); break; } *result=j;
  } else xideRenameJournalDestroy(j);
 } while(FindNextFileW(h,&data));
 if(okay && GetLastError()!=ERROR_NO_MORE_FILES) okay=fail(e,XIDE_MODEL_IO,"项目迁移记录枚举中断。");
 FindClose(h); free(root); if(!okay) { xideRenameJournalDestroy(*result); *result=NULL; } return okay;
}
static xvalue* migrationIdentity(const char* path,int directory,xide_model_error* e)
{
 if(!directory) { xide_file_snapshot file={0}; return xideFileSnapshotInspect(path,&file,e) ? snapshot(&file) : NULL; }
 xide_directory_identity id={0}; xvalue* value=xrtValueObject();
 if(!value || !xideDirectoryIdentityRead(path,&id,e) || !number(value,"index",id.index) || !number(value,"volume",id.volume)) { xrtValueRelease(value); return NULL; } return value;
}
static int migrationIdentityMatches(const char* path,const xvalue* identity,int directory,xide_model_error* e)
{
 if(!directory) { xide_file_snapshot file={0},expected={0}; return readSnapshot(identity,&expected) && xideFileSnapshotInspect(path,&file,e) && xideFileSnapshotEqual(&file,&expected); }
 xide_directory_identity id={0}; uint64_t index=0,volume=0;
 return readNumber(identity,"index",&index) && readNumber(identity,"volume",&volume) && xideDirectoryIdentityRead(path,&id,e) && id.index==index && id.volume==volume;
}
static int migrationState(const xvalue* root,xide_model_error* e)
{
 const char *source=jstring(root,"source"),*target=jstring(root,"target"); char *a=xidePathResolveLexical(NULL,source),*b=xidePathResolveLexical(NULL,target);
 int valid=*source && *target && a && b && !strcmp(a,source) && !strcmp(b,target) && strcmp(source,target) && get(root,"identity"); free(a); free(b);
 if(!valid) { fail(e,XIDE_MODEL_INVALID,"项目迁移磁盘身份记录无效。"); return -1; }
 int old=xidePathExists(source) && exactLeaf(source),next=xidePathExists(target) && exactLeaf(target),directory=flag(root,"directoryMove");
 if(!strcmp(jstring(root,"state"),"aborted") && old && migrationIdentityMatches(source,get(root,"identity"),directory,e)) return 2;
 if(!old && next && migrationIdentityMatches(target,get(root,"identity"),directory,e)) return 1;
 if(old && !strcmp(jstring(root,"state"),"prepared") && migrationIdentityMatches(source,get(root,"identity"),directory,e)) return 0;
 fail(e,XIDE_MODEL_CONFLICT,"项目迁移前后身份无法确认，原记录和草稿保留。"); return -1;
}
static uint64_t processCreated(HANDLE process)
{ FILETIME created={0},exit={0},kernel={0},user={0}; return GetProcessTimes(process,&created,&exit,&kernel,&user) ? ((uint64_t)created.dwHighDateTime<<32)|created.dwLowDateTime : 0; }
static int migrationWriterAlive(const xvalue* root,xide_model_error* e)
{
 uint64_t pid=0,created=0;
 if(!readNumber(root,"writerPid",&pid) || !readNumber(root,"writerCreated",&created) || !pid || pid>UINT32_MAX || !created) { fail(e,XIDE_MODEL_INVALID,"迁移进程身份记录无效，原记录保留。"); return -1; }
 HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,(DWORD)pid);
 if(!process) { if(GetLastError()==ERROR_INVALID_PARAMETER) return 0; fail(e,XIDE_MODEL_CONFLICT,"无法确认迁移进程已退出，原记录保留。"); return -1; }
 uint64_t actual=processCreated(process); DWORD wait=WaitForSingleObject(process,0); CloseHandle(process);
 if(!actual || wait==WAIT_FAILED) { fail(e,XIDE_MODEL_CONFLICT,"无法验证迁移进程身份，原记录保留。"); return -1; }
 return actual==created && wait==WAIT_TIMEOUT;
}
static int migrationMapField(xvalue* object,const char* key,const char* base,const xide_project_migration* plan,xide_model_error* e)
{
 const char* s=text(get(object,key)); if(!s || !*s) return 1;
 char* absolute=xidePathResolveLexical(base,s),*mapped=absolute ? (plan ? xideProjectMigrationMapPath(plan,absolute) : copy(absolute)) : NULL;
 int okay=mapped && put(object,key,mapped); free(absolute); free(mapped); return okay || fail(e,XIDE_MODEL_MEMORY,"无法保留迁移会话路径。");
}
static int migrationMapCheckpoint(xvalue* checkpoint,const char* base,const xide_project_migration* plan,const char* newOwner,xide_model_error* e)
{
 if(!xideSessionValidate(checkpoint) || !migrationMapField(checkpoint,"activeDocument",base,plan,e) || !put(checkpoint,"ownerPath",newOwner)) return 0;
 xvalue* docs=get(checkpoint,"openDocuments");
 for(size_t i=0;i<xrtValueCount(docs);++i) {
  xvalue* d=xrtValueArrayGet(docs,i); if(!migrationMapField(d,"path",base,plan,e)) return 0;
  const char* path=jstring(d,"path"); if(*path && (!put(d,"title",xidePathFilename(path)) || (!flag(d,"languageOverride") && !put(d,"language",xideLanguageForPath(path))))) return 0;
 }
 xvalue* configs=get(checkpoint,"configurations");
 for(size_t i=0;i<xrtValueCount(configs);++i) if(!migrationMapField(xrtValueArrayGet(configs,i),"manifest",base,plan,e)) return 0;
 return xideSessionValidate(checkpoint);
}
int xideMigrationJournalCancel(xide_migration_journal* j,xide_model_error* e)
{
 if(!xideRenameJournalPending(j) || strcmp(jstring(j->marker.source,"state"),"prepared")) return 1;
 uint64_t pid=0,created=0; int own=readNumber(j->marker.source,"writerPid",&pid) && readNumber(j->marker.source,"writerCreated",&created) && pid==GetCurrentProcessId() && created==processCreated(GetCurrentProcess());
 if(!own && migrationWriterAlive(j->marker.source,e)!=0) return fail(e,XIDE_MODEL_CONFLICT,"另一个 IDE 正在迁移项目，请等待其完成，原记录保留。");
 xide_recovery_guard guard={0}; if(!xideRecoveryLock(&guard,j->directory,e)) return 0;
 int okay=migrationState(j->marker.source,e)==0; const xvalue* previous=get(j->marker.source,"previous"); xvalue* root=okay ? xrtValueDeepClone(previous ? previous : j->marker.source) : NULL;
 if(okay) okay=root && (previous || put(root,"state","retired")) && writeMarker(j,root,e);
 xrtValueRelease(root); xideRecoveryUnlock(&guard); return okay;
}
xide_migration_journal* xideMigrationJournalPrepare(xide_app* a,const char* global,const char* directory,const char* base,const xvalue* checkpoint,const xvalue* layout,const xide_project_migration* plan,xide_model_error* e)
{
 const char* currentOwner=owner(a); xide_migration_journal* j=NULL; xide_recovery_guard guard={0}; xvalue *root=NULL,*model=NULL,*owned=NULL,*predicted=NULL; xide_workspace clone={0}; xide_project_migration* preview=NULL; char* nextOwner=NULL; int okay=0;
 if(!migrationFind(global,currentOwner,a->projectSessionPath!=NULL,&j,e)) return NULL;
 if(j && !xideMigrationJournalCancel(j,e)) goto done;
 if(!j) {
  uint64_t hash=UINT64_C(1469598103934665603); for(const unsigned char* p=(const unsigned char*)currentOwner;*p;++p) { hash^=(*p>='A' && *p<='Z') ? *p+32 : *p; hash*=UINT64_C(1099511628211); } hash^=a->projectSessionPath!=NULL;
  char name[64]; snprintf(name,sizeof(name),"migrations/%016llx",(unsigned long long)hash); char* path=xidePathResolveLexical(global,name); j=path ? migrationOpen(path,e) : NULL; free(path); if(!j) goto done;
 }
 if(xideProjectMigrationMode(plan)==XIDE_MIGRATE_DIRECTORY) {
  const char* source=xideProjectMigrationSource(plan); size_t n=strlen(source);
  if(strlen(global)>=n && !memcmp(global,source,n) && (!global[n] || global[n]=='/')) { fail(e,XIDE_MODEL_CONFLICT,"迁移记录存储位于待移动目录内，请使用目录外的会话存储。"); goto done; }
 }
 if(!xideRecoveryLock(&guard,j->directory,e)) goto done;
 int previous=xideRenameJournalPending(j); if(previous && strcmp(jstring(j->marker.source,"state"),"active") && strcmp(jstring(j->marker.source,"state"),"aborted")) { fail(e,XIDE_MODEL_CONFLICT,"上次项目迁移仍待恢复。"); goto done; }
 root=previous ? xrtValueDeepClone(j->marker.source) : xrtValueObject(); model=captureModel(&a->workspaceModel,directory,e);
 if(!root || !model || !decodeModel(model,&clone,e) || !set(root,"beforeModel",xrtValueDeepClone(model))) goto done;
 const char* selected=xideProjectMigrationProjectCount(plan) ? xideProjectMigrationProjectAt(plan,0)->id : a->selectedProject;
 /* The selected project need not be the first affected project. Locate the
    actual source manifest/root using the plan's recorded source identity. */
 for(size_t i=0;i<clone.projectCount;++i) {
  char* source=xideProjectMigrationMode(plan)==XIDE_MIGRATE_DIRECTORY ? xidePathDirectory(clone.projects[i].manifest) : copy(clone.projects[i].manifest);
  int same=source && xidePathLexicalEqual(source,xideProjectMigrationSource(plan)); free(source); if(same && clone.projects[i].available) { selected=clone.projects[i].id; break; }
 }
 preview=xideProjectMigrationPrepare(&clone,selected,xideProjectMigrationTarget(plan),xideProjectMigrationMode(plan),e);
 if(!preview || !xideProjectMigrationApplyPreview(preview,e)) goto done;
 predicted=captureModel(&clone,directory,e); nextOwner=*currentOwner ? xideProjectMigrationMapPath(plan,currentOwner) : copy("");
 if(!predicted || !nextOwner || !put(root,"format",MIGRATION_FORMAT) || !put(root,"state","prepared") || !number(root,"writerPid",GetCurrentProcessId()) || !number(root,"writerCreated",processCreated(GetCurrentProcess())) || !put(root,"ownerKind",a->workspaceModel.manifest ? "workspace" : a->projectSessionPath ? "project" : "none") || !put(root,"ownerBefore",currentOwner) || !put(root,"ownerAfter",nextOwner) || !put(root,"source",xideProjectMigrationSource(plan)) || !put(root,"target",xideProjectMigrationTarget(plan)) || !set(root,"directoryMove",xrtValueBool(xideProjectMigrationMode(plan)==XIDE_MIGRATE_DIRECTORY)) || !set(root,"identity",migrationIdentity(xideProjectMigrationSource(plan),flag(root,"directoryMove"),e)) || !xrtValueObjectSet(root,sv("model"),predicted) || !set(root,"layout",xrtValueDeepClone(layout))) goto done;
 if(!previous && (!set(root,"blobs",xrtValueArray()) || !set(root,"resources",xrtValueArray()) || !set(root,"moves",xrtValueArray()) || !set(root,"owners",xrtValueArray()))) goto done;
 if(!mergeOwned(root,j->marker.source,e)) goto done;
 if(previous) {
  const char* priorPath=jstring(j->marker.source,!strcmp(jstring(j->marker.source,"state"),"aborted") ? "source" : "target");
  xvalue* move=xrtValueObject(); int good=move && put(move,"path",priorPath) && set(move,"directory",xrtValueBool(flag(j->marker.source,"directoryMove"))) && set(move,"identity",xrtValueDeepClone(get(j->marker.source,"identity"))) && set(root,"previous",xrtValueDeepClone(j->marker.source));
  if(good) good=xrtValueArrayAppendNew(get(root,"moves"),move); else xrtValueRelease(move); if(!good) goto done;
  for(size_t i=0;i<xrtValueCount(get(root,"moves"));++i) { xvalue* m=xrtValueArrayGet(get(root,"moves"),i); char* mapped=xideProjectMigrationMapPath(plan,jstring(m,"path")); good=mapped && put(m,"path",mapped); free(mapped); if(!good) goto done; }
 }
 int alias=0; for(size_t i=0;i<xrtValueCount(get(root,"owners"));++i) alias|=!strcmp(text(xrtValueArrayGet(get(root,"owners"),i)),currentOwner);
 if(!alias && !xrtValueArrayAppendNew(get(root,"owners"),xrtValueString(sv(currentOwner)))) goto done;
 free(j->checkpointDirectory); j->checkpointDirectory=copy(directory); if(!j->checkpointDirectory) goto done;
 owned=ownCheckpoint(j,root,checkpoint,e); if(!owned || !migrationMapCheckpoint(owned,base,NULL,currentOwner,e) || !set(root,"beforeSession",xrtValueDeepClone(owned)) || !migrationMapCheckpoint(owned,base,plan,nextOwner,e) || !xrtValueObjectSet(root,sv("session"),owned) || !writeMarker(j,root,e)) goto done;
 okay=1;
done:
 free(nextOwner); xideProjectMigrationDestroy(preview); xideWorkspaceClear(&clone); xrtValueRelease(root); xrtValueRelease(model); xrtValueRelease(predicted); xrtValueRelease(owned); xideRecoveryUnlock(&guard);
 if(!okay) { xideRenameJournalDestroy(j); if(!e || !e->code) fail(e,XIDE_MODEL_MEMORY,"无法准备完整项目迁移恢复记录，磁盘尚未移动。"); return NULL; } return j;
}
int xideMigrationJournalCheckpoint(xide_migration_journal* j,xide_app* a,const char* directory,const char* base,const xvalue* checkpoint,const xvalue* layout,xide_model_error* e)
{
 if(!xideRenameJournalPending(j)) return 1;
 if(!strcmp(jstring(j->marker.source,"state"),"prepared") && migrationState(j->marker.source,e)!=1) return !e || !e->code;
 xide_recovery_guard guard={0}; if(!xideRecoveryLock(&guard,j->directory,e)) return 0;
 xvalue *root=xrtValueDeepClone(j->marker.source),*model=captureModel(&a->workspaceModel,directory,e),*owned=NULL; int okay=root && model;
 if(okay) okay=mergeOwned(root,j->marker.source,e);
 int aborted=!strcmp(jstring(j->marker.source,"state"),"aborted");
 if(okay) { xrtValueObjectRemove(root,sv("previous")); okay=put(root,"state",aborted ? "aborted" : "active") && put(root,"ownerAfter",owner(a)) && xrtValueObjectSet(root,sv("model"),model) && set(root,"layout",xrtValueDeepClone(layout)); }
 if(okay) { free(j->checkpointDirectory); j->checkpointDirectory=copy(directory); okay=j->checkpointDirectory!=NULL; }
 if(okay) { owned=ownCheckpoint(j,root,checkpoint,e); okay=owned && migrationMapCheckpoint(owned,base,NULL,owner(a),e) && xrtValueObjectSet(root,sv("session"),owned) && set(root,"identity",migrationIdentity(jstring(root,aborted ? "source" : "target"),flag(root,"directoryMove"),e)); }
 for(size_t i=0;okay && i<xrtValueCount(get(root,"moves"));++i) { xvalue* m=xrtValueArrayGet(get(root,"moves"),i); okay=set(m,"identity",migrationIdentity(jstring(m,"path"),flag(m,"directory"),e)); }
 int saved=!a->workspaceModel.dirty; for(size_t i=0;saved && i<a->workspaceModel.projectCount;++i) saved=!a->workspaceModel.projects[i].dirty;
 if(okay && saved) okay=put(root,"state","retired");
 if(okay) okay=writeMarker(j,root,e);
 xrtValueRelease(root); xrtValueRelease(model); xrtValueRelease(owned); xideRecoveryUnlock(&guard); return okay;
}
int xideMigrationJournalLoad(const char* global,const char* path,int projectOnly,xide_workspace* model,char** projectPath,xide_migration_journal** out,xvalue** checkpoint,xide_model_error* e)
{
 *out=NULL; *checkpoint=NULL; *projectPath=NULL; xide_migration_journal* j=NULL; xide_recovery_guard guard={0}; xvalue *root=NULL,*owned=NULL; int okay=0;
 char* absolute=*path ? xidePathResolveLexical(NULL,path) : copy(""); if(!absolute || !migrationFind(global,absolute,projectOnly,&j,e)) { free(absolute); return 0; } free(absolute);
 if(!j || !xideRenameJournalPending(j)) { xideRenameJournalDestroy(j); return 1; }
 if(!xideRecoveryLock(&guard,j->directory,e)) goto done;
 if(strcmp(jstring(j->marker.source,"state"),"prepared") && strcmp(jstring(j->marker.source,"state"),"active") && strcmp(jstring(j->marker.source,"state"),"aborted")) { fail(e,XIDE_MODEL_INVALID,"项目迁移状态无效，原记录保留。"); goto done; }
 if(!strcmp(jstring(j->marker.source,"state"),"prepared") && migrationWriterAlive(j->marker.source,e)!=0) { fail(e,XIDE_MODEL_CONFLICT,"另一个 IDE 正在迁移项目，原记录和草稿保留，请等待其完成。"); goto done; }
 int moved=migrationState(j->marker.source,e); if(moved<0) goto done;
 if(!moved && get(j->marker.source,"previous")) { xvalue* prior=xrtValueDeepClone(get(j->marker.source,"previous")); int good=prior && !get(prior,"previous") && (!strcmp(jstring(prior,"state"),"active") || !strcmp(jstring(prior,"state"),"aborted")) && writeMarker(j,prior,e); xrtValueRelease(prior); if(!good) goto done; moved=migrationState(j->marker.source,e); if(moved<0) goto done; }
 root=xrtValueDeepClone(j->marker.source); if(!root) goto done;
 if(!moved) { if(!set(root,"model",xrtValueDeepClone(get(root,"beforeModel"))) || !set(root,"session",xrtValueDeepClone(get(root,"beforeSession"))) || !put(root,"ownerAfter",jstring(root,"ownerBefore")) || !put(root,"state","aborted") || !writeMarker(j,root,e)) goto done; }
 const xvalue* moves=get(root,"moves"); if(!xrtValueIs(moves,XVALUE_ARRAY) || xrtValueCount(moves)>10000 || !xrtValueIs(get(root,"owners"),XVALUE_ARRAY)) goto done;
 for(size_t i=0;i<xrtValueCount(moves);++i) { const xvalue* m=xrtValueArrayGet(moves,i); if(!exactLeaf(jstring(m,"path")) || !migrationIdentityMatches(jstring(m,"path"),get(m,"identity"),flag(m,"directory"),e)) { fail(e,XIDE_MODEL_CONFLICT,"连续项目迁移身份无法确认，原记录保留。"); goto done; } }
 if(!recognizedModel(get(root,"model"),e) || !decodeModel(get(root,"model"),model,e)) goto done;
 const char* kind=jstring(root,"ownerKind"),*after=jstring(root,"ownerAfter");
 if((!strcmp(kind,"workspace") && (!model->manifest || !xidePathLexicalEqual(model->manifest,after))) || (!strcmp(kind,"project") && (model->manifest || !*after)) || (!strcmp(kind,"none") && (model->manifest || *after))) goto done;
 if(!strcmp(kind,"project")) { *projectPath=copy(after); if(!*projectPath) goto done; }
 owned=xrtValueDeepClone(get(root,"session")); if(!owned || !xideSessionValidate(owned)) goto done;
 size_t total=0; const xvalue* docs=get(owned,"openDocuments");
 for(size_t i=0;i<xrtValueCount(docs);++i) { const xvalue* d=xrtValueArrayGet(docs,i); const char* keys[]={"recoveryText","recoveryBaseline"}; for(int k=0;k<2;++k) if(get(d,keys[k])) { size_t n=0; unsigned char* bytes=readBlob(j,root,jstring(d,keys[k]),&n,e); int good=bytes && n<=TOTAL_LIMIT-total; xrtFree(bytes); if(!good) goto done; total+=n; } }
 j->recovered=1; *out=j; j=NULL; *checkpoint=owned; owned=NULL; okay=1;
done:
 if(!okay) { xideWorkspaceClear(model); free(*projectPath); *projectPath=NULL; if(!e || !e->code) fail(e,XIDE_MODEL_INVALID,"项目迁移记录不完整，原模型和恢复正文保留。"); }
 xrtValueRelease(root); xrtValueRelease(owned); xideRenameJournalDestroy(j); xideRecoveryUnlock(&guard); return okay;
}
int xideMigrationJournalStage(xide_migration_journal* j,const char* directory,const char* base,const xvalue* current,xvalue** checkpoint,xide_model_error* e)
{
 xvalue* saved=*checkpoint; const xvalue* root=j->marker.source;
 const char *kind=jstring(root,"ownerKind"),*path=jstring(current,"ownerPath"),*after=jstring(root,"ownerAfter");
 /* An unnamed owner is an empty identity, not a path relative to base. */
 char* currentOwner=*path ? xidePathResolveLexical(base,path) : NULL;
 int sameOwner=!strcmp(kind,"none") ? !*path && !*after : currentOwner && *after && xidePathLexicalEqual(currentOwner,after);
 int newer=current && xideSessionValidate(current) && *jstring(current,"layoutGeneration") && strcmp(jstring(current,"layoutGeneration"),jstring(saved,"layoutGeneration")) && sameOwner && !strcmp(jstring(current,"ownerKind"),kind); free(currentOwner);
 xvalue* fresh=newer ? xrtValueDeepClone(current) : NULL;
 if(newer && !fresh) return fail(e,XIDE_MODEL_MEMORY,"无法保留最新迁移会话。");
 const xvalue* docs=get(newer ? fresh : saved,"openDocuments"); size_t total=0; xide_recovery_guard guard={0};
 if(!xideRecoveryLock(&guard,directory,e)) { xrtValueRelease(fresh); return 0; }
 int okay=1;
 for(size_t i=0;okay && i<xrtValueCount(docs);++i) { const xvalue* d=xrtValueArrayGet(docs,i); const char* keys[]={"recoveryText","recoveryBaseline"}; for(int k=0;okay && k<2;++k) if(get(d,keys[k])) {
  const char* ref=jstring(d,keys[k]); size_t n=0,dn=0; char* path=xideRecoveryPath(directory,ref,e); unsigned char* bytes=newer ? (path && regular(path,0) ? xideIoReadAllLimit(path,BODY_LIMIT,&n) : NULL) : readBlob(j,root,ref,&n,e);
  okay=bytes && n<=TOTAL_LIMIT-total; if(okay) total+=n;
  if(okay && !newer) { if(xidePathExists(path)) { unsigned char* disk=regular(path,0) ? xideIoReadAllLimit(path,BODY_LIMIT,&dn) : NULL; okay=disk && dn==n && !memcmp(disk,bytes,n); xrtFree(disk); } else okay=xideIoWriteAtomic(path,(xbytesview){bytes,n}) && regular(path,0); }
  xrtFree(bytes); free(path);
 } }
 if(okay && newer) { xrtValueRelease(*checkpoint); *checkpoint=fresh; fresh=NULL; }
 xrtValueRelease(fresh); xideRecoveryUnlock(&guard); return okay || fail(e,XIDE_MODEL_CONFLICT,"迁移正文缺失或目标恢复文件冲突，原记录保留。");
}
const xvalue* xideMigrationJournalDock(const xide_migration_journal* j,const char* generation)
{
 const xvalue* rows=j ? get(get(j->marker.source,"layout"),"states") : NULL;
 for(size_t i=0;i<xrtValueCount(rows);++i) { const xvalue* row=xrtValueArrayGet(rows,i); if(!strcmp(jstring(row,"id"),generation)) return get(row,"dock"); } return NULL;
}

#define MIGRATION_GRACE (UINT64_C(7)*24*60*60*10000000)
#define MAINTENANCE_FORMAT "xlang-ide-migration-maintenance-1"
static int migrationLeaf(const char* leaf)
{ return strlen(leaf)==16 && strspn(leaf,"0123456789abcdef")==16; }
static int cancelled(volatile LONG* flag)
{ return flag && InterlockedCompareExchange(flag,0,0); }
static HANDLE maintenanceDirectory(const char* path)
{
 wchar_t* wide=xidePathNativeWide(path); HANDLE h=wide ? CreateFileW(wide,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(wide);
 BY_HANDLE_FILE_INFORMATION info={0};
 if(h!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(h,&info) || !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))) { CloseHandle(h); h=INVALID_HANDLE_VALUE; }
 return h;
}
static HANDLE maintenanceFile(const char* path,DWORD access,DWORD disposition)
{
 wchar_t* wide=xidePathNativeWide(path); HANDLE h=wide ? CreateFileW(wide,access,access==GENERIC_READ ? FILE_SHARE_READ : 0,NULL,disposition,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(wide);
 BY_HANDLE_FILE_INFORMATION info={0};
 if(h!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(h,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY)) || info.nNumberOfLinks!=1)) { CloseHandle(h); h=INVALID_HANDLE_VALUE; }
 return h;
}
static int maintenanceMarker(xide_migration_journal* j,uint64_t* retired,xide_model_error* e)
{
 const xvalue* root=j->marker.source; const char* kind=jstring(root,"ownerKind");
 if(!xideSessionValidate(get(root,"session")) || !xrtValueIs(get(root,"model"),XVALUE_OBJECT) || !xrtValueIs(get(get(root,"model"),"projects"),XVALUE_ARRAY) || (strcmp(kind,"workspace") && strcmp(kind,"project") && strcmp(kind,"none"))) return 0;
 if(get(root,"retiredAt")) return readNumber(root,"retiredAt",retired) && *retired;
 /* A legacy retired marker's last publication is a conservative retirement date. */
 xide_file_snapshot marker={0}; if(!xideFileSnapshotInspect(j->marker.path,&marker,e)) return 0; *retired=marker.lastWrite; return *retired!=0;
}
static int refRemoved(const char* ref,const owned_row* removed,size_t count)
{ for(size_t i=0;i<count;++i) if(!strcmp(ref,removed[i].ref)) return 1; return 0; }
static int maintenanceStore(const char* directory,uint64_t now,volatile LONG* stop,size_t* inspected,uint64_t* readBytes,xide_recovery_stats* stats,xide_model_error* e)
{
 xide_recovery_guard guard={0}; xide_model_error local={0}; xide_migration_journal* j=NULL; HANDLE marker=INVALID_HANDLE_VALUE; xvalue *root=NULL,*kept=NULL,*blobs=NULL; owned_row* removed=NULL; size_t count=0; int okay=1;
 j=migrationOpen(directory,&local);
 if(!j || !j->marker.existed || strcmp(jstring(j->marker.source,"state"),"retired")) { ++stats->skipped; goto done; }
 if(!xideRecoveryLock(&guard,directory,&local)) { ++stats->skipped; goto done; }
 xideRenameJournalDestroy(j); j=migrationOpen(directory,&local);
 if(!j || !j->marker.existed || strcmp(jstring(j->marker.source,"state"),"retired")) { ++stats->skipped; goto done; }
 marker=maintenanceFile(j->marker.path,GENERIC_READ,OPEN_EXISTING); xvalue* pinned=marker!=INVALID_HANDLE_VALUE ? readJson(j->marker.path) : NULL;
 int stable=pinned && xrtValueEqual(pinned,j->marker.source); xrtValueRelease(pinned);
 uint64_t retired=0;
 if(!stable || !maintenanceMarker(j,&retired,&local)) { ++stats->skipped; goto done; }
 if(now<retired || now-retired<MIGRATION_GRACE) { ++stats->retained; goto done; }
 root=xrtValueDeepClone(j->marker.source);
 if(!root || !mergeOwned(root,j->marker.source,&local)) { ++stats->skipped; goto done; }
 const xvalue* rows=get(root,"ownedBodies"); size_t total=xrtValueCount(rows); const char* cursor=jstring(root,"maintenanceAfter");
 if(*cursor && !bodyRef(cursor)) { ++stats->skipped; goto done; }
 removed=calloc(total ? total : 1,sizeof(*removed)); kept=xrtValueArray(); blobs=xrtValueArray();
 if(!removed || !kept || !blobs) { okay=fail(e,XIDE_MODEL_MEMORY,"无法准备迁移正文维护，原文件保留。"); goto done; }
 size_t first=0; while(first<total && strcmp(jstring(xrtValueArrayGet(rows,first),"ref"),cursor)<=0) ++first;
 const char* after=cursor;
 for(size_t step=0;step<total;++step) {
  if(cancelled(stop) || *inspected>=128 || *readBytes>=64u*1024u*1024u) break;
  const xvalue* row=xrtValueArrayGet(rows,(first+step)%total); const char* ref=jstring(row,"ref"); xide_file_snapshot expected={0},actual={0}; readSnapshot(row,&expected);
  if(expected.length>64u*1024u*1024u-*readBytes) break;
  ++stats->scanned; ++*inspected; after=ref;
  char* path=xideRecoveryPath(directory,ref,&local); wchar_t* wide=path ? xidePathNativeWide(path) : NULL;
  int attempted=wide!=NULL; HANDLE body=wide ? CreateFileW(wide,GENERIC_READ|DELETE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; DWORD code=GetLastError(); free(wide); free(path);
  if(body==INVALID_HANDLE_VALUE) {
   if(attempted && (code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND)) removed[count++]=(owned_row){ref,row};
   else ++stats->skipped;
   continue;
  }
  BY_HANDLE_FILE_INFORMATION info={0}; int ordinary=GetFileInformationByHandle(body,&info) && !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY)) && info.nNumberOfLinks==1;
  uint64_t created=((uint64_t)info.ftCreationTime.dwHighDateTime<<32)|info.ftCreationTime.dwLowDateTime;
  if(!ordinary || now<created || now-created<MIGRATION_GRACE || now<expected.lastWrite || now-expected.lastWrite<MIGRATION_GRACE) ++stats->retained;
  else if(expected.length!=(((uint64_t)info.nFileSizeHigh<<32)|info.nFileSizeLow) || expected.index!=(((uint64_t)info.nFileIndexHigh<<32)|info.nFileIndexLow) || expected.volume!=info.dwVolumeSerialNumber || expected.attributes!=info.dwFileAttributes || expected.lastWrite!=(((uint64_t)info.ftLastWriteTime.dwHighDateTime<<32)|info.ftLastWriteTime.dwLowDateTime)) ++stats->skipped;
  else {
   *readBytes+=expected.length;
   if(!xideFileSnapshotInspectHandle(body,&actual,&local) || !xideFileSnapshotEqual(&actual,&expected) || cancelled(stop)) ++stats->skipped;
   else { FILE_DISPOSITION_INFO disposition={TRUE};
    if(SetFileInformationByHandle(body,FileDispositionInfo,&disposition,sizeof(disposition))) { removed[count++]=(owned_row){ref,row}; ++stats->removed; stats->removedBytes+=actual.length; }
    else ++stats->skipped;
   }
  }
  CloseHandle(body);
 }
 for(size_t i=0;okay && i<total;++i) { const xvalue* row=xrtValueArrayGet(rows,i); if(!refRemoved(jstring(row,"ref"),removed,count)) okay=xrtValueArrayAppendNew(kept,xrtValueDeepClone(row)); }
 const xvalue* oldBlobs=get(root,"blobs");
 for(size_t i=0;okay && i<xrtValueCount(oldBlobs);++i) { const xvalue* row=xrtValueArrayGet(oldBlobs,i); if(!refRemoved(jstring(row,"ref"),removed,count)) okay=xrtValueArrayAppendNew(blobs,xrtValueDeepClone(row)); }
 if(okay) okay=xrtValueObjectSet(root,sv("ownedBodies"),kept) && xrtValueObjectSet(root,sv("blobs"),blobs) && number(root,"retiredAt",retired) && put(root,"maintenanceAfter",after);
 CloseHandle(marker); marker=INVALID_HANDLE_VALUE;
 /* A failed publication leaves a retired record with missing known bodies.
    The next pass removes those stale rows; retired records are never restored. */
 if(okay) okay=regular(j->marker.path,0) && xideStateWrite(&j->marker,root,e);
done:
 if(marker!=INVALID_HANDLE_VALUE) CloseHandle(marker);
 free(removed); xrtValueRelease(kept); xrtValueRelease(blobs); xrtValueRelease(root); xideRenameJournalDestroy(j); xideRecoveryUnlock(&guard); return okay;
}
int xideMigrationJournalMaintain(const char* global,uint64_t now,volatile LONG* stop,xide_recovery_stats* out,xide_model_error* e)
{
 if(e) memset(e,0,sizeof(*e));
 if(out) memset(out,0,sizeof(*out));
 if(!global || !*global) return fail(e,XIDE_MODEL_INVALID,"迁移维护目录无效，原文件保留。");
 xide_recovery_stats stats={0}; xide_state_file cursor={0}; HANDLE parent=INVALID_HANDLE_VALUE,rootHandle=INVALID_HANDLE_VALUE,mutex=INVALID_HANDLE_VALUE,find=INVALID_HANDLE_VALUE;
 char *root=xidePathResolveLexical(global,"migrations"),*pattern=NULL,*path=NULL; wchar_t* wide=NULL; char (*leaves)[17]=NULL; xvalue* fresh=NULL; int okay=0; size_t count=0,inspected=0; uint64_t bytes=0;
 if(!root || !*global) { fail(e,XIDE_MODEL_INVALID,"迁移维护目录无效，原文件保留。"); goto done; }
 if(!xidePathExists(root)) { okay=1; goto done; }
 wchar_t* globalWide=xidePathNativeWide(global); wchar_t volume[MAX_PATH]={0}; int local=globalWide && GetVolumePathNameW(globalWide,volume,MAX_PATH) && GetDriveTypeW(volume)==DRIVE_FIXED; free(globalWide);
 if(!local) { ++stats.skipped; okay=1; goto done; }
 parent=maintenanceDirectory(global); rootHandle=maintenanceDirectory(root);
 if(parent==INVALID_HANDLE_VALUE || rootHandle==INVALID_HANDLE_VALUE) { fail(e,XIDE_MODEL_CONFLICT,"迁移维护目录不能固定，原文件保留。"); goto done; }
 path=xidePathResolveLexical(root,"maintenance.lock"); mutex=path ? maintenanceFile(path,GENERIC_READ|GENERIC_WRITE,OPEN_ALWAYS) : INVALID_HANDLE_VALUE; free(path); path=NULL;
 if(mutex==INVALID_HANDLE_VALUE) { ++stats.skipped; okay=1; goto done; }
 path=xidePathResolveLexical(root,"maintenance.json");
 if(!path || !regular(path,1) || !xideStateOpen(&cursor,path,MAINTENANCE_FORMAT,e)) goto done;
 free(path); path=NULL; const char* after=jstring(cursor.source,"afterLeaf");
 if((get(cursor.source,"afterLeaf") && !text(get(cursor.source,"afterLeaf"))) || (*after && !migrationLeaf(after))) { fail(e,XIDE_MODEL_INVALID,"迁移维护游标损坏，原文件保留。"); goto done; }
 leaves=calloc(MIGRATION_RECORD_LIMIT,sizeof(*leaves)); pattern=xidePathResolveLexical(root,"*"); wide=pattern ? xidePathNativeWide(pattern) : NULL;
 if(!leaves || !wide) { fail(e,XIDE_MODEL_MEMORY,"无法枚举迁移维护目录。"); goto done; }
 WIN32_FIND_DATAW data={0}; find=FindFirstFileW(wide,&data);
 if(find==INVALID_HANDLE_VALUE) { fail(e,XIDE_MODEL_IO,"无法枚举迁移维护目录。"); goto done; }
 size_t scanned=0;
 do {
  if(!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..") || !wcscmp(data.cFileName,L"maintenance.lock") || !wcscmp(data.cFileName,L"maintenance.json") || !wcscmp(data.cFileName,L"maintenance.json.lock")) continue;
  if(++scanned>MIGRATION_RECORD_LIMIT) { fail(e,XIDE_MODEL_INVALID,"迁移维护超过 1024 项，原文件保留。"); goto done; }
  if(wcslen(data.cFileName)!=16 || wcsspn(data.cFileName,L"0123456789abcdef")!=16 || !(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) { ++stats.skipped; continue; }
  for(size_t i=0;i<16;++i) leaves[count][i]=(char)data.cFileName[i];
  ++count;
 } while(FindNextFileW(find,&data));
 if(GetLastError()!=ERROR_NO_MORE_FILES) { fail(e,XIDE_MODEL_IO,"迁移维护枚举中断，原文件保留。"); goto done; }
 FindClose(find); find=INVALID_HANDLE_VALUE;
 /* A persisted cursor prevents a busy/damaged store from starving others. */
 for(size_t i=0;i<count;++i) for(size_t k=i+1;k<count;++k) if(strcmp(leaves[i],leaves[k])>0) { char temp[17]; memcpy(temp,leaves[i],17); memcpy(leaves[i],leaves[k],17); memcpy(leaves[k],temp,17); }
 size_t first=0; while(first<count && strcmp(leaves[first],after)<=0) ++first;
 okay=1; const char* next=after;
 for(size_t step=0;okay && step<count && step<64 && inspected<128 && bytes<64u*1024u*1024u && !cancelled(stop);++step) {
  next=leaves[(first+step)%count]; path=xidePathResolveLexical(root,next);
  okay=path && maintenanceStore(path,now,stop,&inspected,&bytes,&stats,e); free(path); path=NULL;
 }
 if(okay) { fresh=xrtValueDeepClone(cursor.source); okay=fresh && put(fresh,"format",MAINTENANCE_FORMAT) && put(fresh,"afterLeaf",next) && regular(cursor.path,1) && xideStateWrite(&cursor,fresh,e); }
done:
 if(find!=INVALID_HANDLE_VALUE) FindClose(find);
 if(mutex!=INVALID_HANDLE_VALUE) CloseHandle(mutex);
 if(rootHandle!=INVALID_HANDLE_VALUE) CloseHandle(rootHandle);
 if(parent!=INVALID_HANDLE_VALUE) CloseHandle(parent);
 free(root); free(pattern); free(path); free(wide); free(leaves); xrtValueRelease(fresh); xideStateClear(&cursor); if(out) *out=stats; return okay;
}
