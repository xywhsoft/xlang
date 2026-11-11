#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "platform/workspace_edit_spool.h"
#include "platform/recovery_store.h"
#include "platform/state_file.h"
#include "platform/path.h"
#include <windows.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SPOOL_FORMAT "xlang-ide-edit-spool-1"
#define BODY_LIMIT (16u*1024u*1024u)
#define DISK_LIMIT (UINT64_C(512)*1024u*1024u)
#define CODEC_RESERVE (48u*1024u*1024u)
typedef struct spool_body {char ref[128];HANDLE handle;xide_file_snapshot snapshot;} spool_body;
typedef struct body_key {uint64_t bytes;unsigned char digest[32];} body_key;
struct xide_edit_spool {
 xide_recovery_guard guard;xide_state_file marker;HANDLE markerHandle;
 xide_workspace_edit_files* sources;xide_workspace_edit* plan;xarray bodies;xmap index;
 char* id;int markerAttempted;size_t used,limit;uint64_t diskBytes;size_t diskFiles;
};
static xstrview sv(const char* s){return(xstrview){s,strlen(s)};}
static xvalue* get(const xvalue* v,const char* k){return v?xrtValueObjectGet(v,sv(k)):NULL;}
static const char* string(const xvalue* v){xstrview s={0};return xrtValueGetString(v,&s)&&!memchr(s.Data,0,s.Size)?s.Data:NULL;}
static int set(xvalue* v,const char* key,xvalue* value){return value&&xrtValueObjectSetNew(v,sv(key),value);}
static int put(xvalue* v,const char* key,const char* value){return set(v,key,xrtValueString(sv(value)));}
static int fail(xide_model_error* e,int code,const char* s){if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",s);}return 0;}
static int stopped(int(*fn)(void*),void* user,xide_model_error* e){return fn&&fn(user)?!fail(e,XIDE_MODEL_CONFLICT,"Workspace edit spooling cancelled."):0;}
static int charge(xide_edit_spool* p,size_t bytes,xide_model_error* e){if(bytes>p->limit-p->used)return fail(e,XIDE_MODEL_MEMORY,"Workspace edit recovery reservation exceeded.");p->used+=bytes;return 1;}
static int number(xvalue* o,const char* key,uint64_t n){char s[32];snprintf(s,sizeof(s),"%llu",(unsigned long long)n);return put(o,key,s);}
static int readNumber(const xvalue* v,uint64_t* n)
{const char* s=string(v);if(!s||!*s||strlen(s)>20||strspn(s,"0123456789")!=strlen(s))return 0;char* end=NULL;errno=0;unsigned long long value=strtoull(s,&end,10);if(errno||!end||*end)return 0;*n=(uint64_t)value;return 1;}
static int integer(const xvalue* v,int64_t* n){int64 value=0;if(!xrtValueGetInt(v,&value))return 0;*n=value;return 1;}
static int readFlag(const xvalue* v,int* n){bool value=false;if(!xrtValueGetBool(v,&value))return 0;*n=value;return 1;}
static int hash(const unsigned char* bytes,size_t size,unsigned char out[32],int(*cancel)(void*),void* user,xide_model_error* e)
{
 xsha256 h;xrtSha256Init(&h);
 for(size_t at=0;at<size;){if(stopped(cancel,user,e))return 0;size_t step=size-at>65536?65536:size-at;if(!xrtSha256Update(&h,bytes+at,step))return fail(e,XIDE_MODEL_IO,"Cannot hash recovery bytes.");at+=step;}
 return xrtSha256Final(&h,out)||fail(e,XIDE_MODEL_IO,"Cannot finish recovery digest.");
}
static xvalue* snapshotValue(const xide_file_snapshot* s)
{
 char hex[65];size_t written=0;xvalue* o=xrtValueObject();
 if(!o||!xrtHexEncode(s->sha256,32,hex,sizeof(hex),&written,0)||written!=64||!number(o,"length",s->length)||!number(o,"index",s->index)||!number(o,"volume",s->volume)||!number(o,"attributes",s->attributes)||!number(o,"lastWrite",s->lastWrite)||!put(o,"sha256",hex)){xrtValueRelease(o);return NULL;}return o;
}
static int readSnapshot(const xvalue* o,xide_file_snapshot* out)
{
 uint64_t volume=0,attrs=0;size_t bytes=0;const char* h=string(get(o,"sha256"));xide_file_snapshot s={0};
 if(!xrtValueIs(o,XVALUE_OBJECT)||!readNumber(get(o,"length"),&s.length)||!readNumber(get(o,"index"),&s.index)||!readNumber(get(o,"volume"),&volume)||volume>UINT32_MAX||!readNumber(get(o,"attributes"),&attrs)||attrs>UINT32_MAX||!readNumber(get(o,"lastWrite"),&s.lastWrite)||!h||strlen(h)!=64||!xrtHexDecode(sv(h),s.sha256,32,&bytes,0)||bytes!=32)return 0;
 s.volume=(uint32_t)volume;s.attributes=(uint32_t)attrs;*out=s;return 1;
}
/* A writer's last-write timestamp may finalize on its last close. Immutable
 * payload identity/content/attributes must match; source gates still compare
 * their COMPLETE original snapshot including last-write time. */
static int bodyEqual(const xide_file_snapshot* a,const xide_file_snapshot* b)
{return a->length==b->length&&a->volume==b->volume&&a->index==b->index&&a->attributes==b->attributes&&!memcmp(a->sha256,b->sha256,32);}
static int ordinary(HANDLE h)
{BY_HANDLE_FILE_INFORMATION i={0};return h&&h!=INVALID_HANDLE_VALUE&&GetFileType(h)==FILE_TYPE_DISK&&GetFileInformationByHandle(h,&i)&&!(i.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_READONLY))&&i.nNumberOfLinks==1;}
static HANDLE openFile(const char* path,DWORD access,DWORD disposition)
{wchar_t* name=xidePathNativeWide(path);HANDLE h=name?CreateFileW(name,access,FILE_SHARE_READ,NULL,disposition,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL):INVALID_HANDLE_VALUE;free(name);return h;}
static void release(xide_edit_spool* p,int abandon)
{
 if(!p)return;
 if(p->markerHandle&&p->markerHandle!=INVALID_HANDLE_VALUE)CloseHandle(p->markerHandle);
 for(size_t i=0;i<p->bodies.Count;++i){spool_body* b=xrtArrayGet(&p->bodies,i);if(b->handle&&b->handle!=INVALID_HANDLE_VALUE){if(abandon&&!p->markerAttempted){FILE_DISPOSITION_INFO remove={TRUE};(void)SetFileInformationByHandle(b->handle,FileDispositionInfo,&remove,sizeof(remove));}CloseHandle(b->handle);}}
 xrtMapUnit(&p->index);xrtArrayUnit(&p->bodies);xideWorkspaceEditFilesDestroy(p->sources);xideWorkspaceEditRelease(p->plan);xideStateClear(&p->marker);xideRecoveryUnlock(&p->guard);free(p->id);xrtFree(p);
}
void xideEditSpoolDestroy(xide_edit_spool* p){release(p,0);}
const xvalue* xideEditSpoolManifest(const xide_edit_spool* p){return p?p->marker.source:NULL;}
xide_workspace_edit_files* xideEditSpoolSources(xide_edit_spool* p){return p?p->sources:NULL;}
static xide_edit_spool* begin(const char* directory,size_t limit,xide_model_error* e)
{
 xide_edit_spool* p=xrtCalloc(1,sizeof(*p));if(!p){fail(e,XIDE_MODEL_MEMORY,"Cannot allocate edit recovery package.");return NULL;}p->limit=limit;p->used=sizeof(*p);
 if(!xrtArrayInit(&p->bodies,sizeof(spool_body))||!xrtMapInit(&p->index,sizeof(size_t))||!xideRecoveryLock(&p->guard,directory,e)){release(p,0);return NULL;}
 char* marker=xideRecoveryPath(p->guard.directory,"edit-journal.json",e);int okay=marker&&xideStateOpen(&p->marker,marker,SPOOL_FORMAT,e);free(marker);if(!okay){release(p,0);return NULL;}return p;
}
static int capacity(xide_edit_spool* p,xide_model_error* e)
{
 char* pattern=xidePathResolveLexical(p->guard.directory,"recovery/*");wchar_t* native=pattern?xidePathNativeWide(pattern):NULL;free(pattern);if(!native)return fail(e,XIDE_MODEL_MEMORY,"Cannot allocate edit recovery capacity path.");WIN32_FIND_DATAW item={0};HANDLE find=FindFirstFileW(native,&item);DWORD error=GetLastError();free(native);
 if(find==INVALID_HANDLE_VALUE)return error==ERROR_FILE_NOT_FOUND||fail(e,XIDE_MODEL_IO,"Cannot inspect edit recovery storage capacity.");
 int okay=1;do{if(!wcscmp(item.cFileName,L".")||!wcscmp(item.cFileName,L".."))continue;uint64_t bytes=((uint64_t)item.nFileSizeHigh<<32)|item.nFileSizeLow;if(++p->diskFiles>8192||bytes>DISK_LIMIT-p->diskBytes){okay=fail(e,XIDE_MODEL_MEMORY,"Edit recovery storage is full; existing files are preserved.");break;}p->diskBytes+=bytes;}while(FindNextFileW(find,&item));DWORD end=GetLastError();FindClose(find);return okay&&(end==ERROR_NO_MORE_FILES||fail(e,XIDE_MODEL_IO,"Cannot finish edit recovery capacity scan."));
}
static int writeBytes(HANDLE h,const unsigned char* data,size_t size,int(*cancel)(void*),void* user,xide_model_error* e)
{
 for(size_t at=0;at<size;){if(stopped(cancel,user,e))return 0;DWORD wrote=0,want=size-at>65536?65536:(DWORD)(size-at);if(!WriteFile(h,data+at,want,&wrote,NULL)||wrote!=want)return fail(e,XIDE_MODEL_IO,"Cannot write edit recovery body.");at+=wrote;}return 1;
}
static int addBody(xide_edit_spool* p,const unsigned char* data,size_t size,size_t source,size_t* result,int(*cancel)(void*),void* user,xide_model_error* e)
{
 body_key key={.bytes=size};size_t document=0;xide_file_snapshot original={0};
 if(source!=SIZE_MAX){if(!xideWorkspaceEditFilesAt(p->sources,source,&document,&original))return fail(e,XIDE_MODEL_INVALID,"Unknown pinned source for recovery copy.");size=(size_t)original.length;key.bytes=size;memcpy(key.digest,original.sha256,32);}
 else if(!hash(data,size,key.digest,cancel,user,e))return 0;
 if(size>BODY_LIMIT)return fail(e,XIDE_MODEL_MEMORY,"Edit recovery body exceeds16MiB.");
 size_t* old=xrtMapGet(&p->index,(xbytesview){(const unsigned char*)&key,sizeof(key)});if(old){*result=*old;return 1;}
 if(p->bodies.Count>=2048||p->diskFiles>=8192||size>DISK_LIMIT-p->diskBytes)return fail(e,XIDE_MODEL_MEMORY,"Edit recovery disk quota exceeded.");
 size_t at=p->bodies.Count;spool_body next={.handle=INVALID_HANDLE_VALUE};if(snprintf(next.ref,sizeof(next.ref),"recovery/%s-%04zu.body",p->id,at)>=(int)sizeof(next.ref)||!xrtArrayPush(&p->bodies,&next))return fail(e,XIDE_MODEL_MEMORY,"Cannot retain edit recovery body.");
 spool_body* b=xrtArrayGet(&p->bodies,at);char* path=xideRecoveryPath(p->guard.directory,b->ref,e);b->handle=path?openFile(path,GENERIC_READ|GENERIC_WRITE|DELETE,CREATE_NEW):INVALID_HANDLE_VALUE;free(path);
 if(!ordinary(b->handle))return fail(e,XIDE_MODEL_IO,"Cannot create a unique ordinary edit recovery body.");
 ++p->diskFiles;p->diskBytes+=size;
 int okay=source==SIZE_MAX?writeBytes(b->handle,data,size,cancel,user,e):xideWorkspaceEditFilesCopyBefore(p->sources,source,b->handle,cancel,user,e);
 if(!okay)return 0;
 if(!FlushFileBuffers(b->handle)||!xideFileSnapshotInspectHandle(b->handle,&b->snapshot,e)||b->snapshot.length!=size||memcmp(b->snapshot.sha256,key.digest,32))return fail(e,XIDE_MODEL_IO,"Flushed edit recovery body failed verification.");
 if(!xrtMapSet(&p->index,(xbytesview){(const unsigned char*)&key,sizeof(key)},&at))return fail(e,XIDE_MODEL_MEMORY,"Cannot index edit recovery body.");
 *result=at;return 1;
}
unsigned char* xideEditSpoolReadBody(xide_edit_spool* p,size_t at,size_t* size,int(*cancel)(void*),void* user,xide_model_error* e)
{
 if(size)*size=0;
 if(e)memset(e,0,sizeof(*e));
 spool_body* b=p?xrtArrayGet(&p->bodies,at):NULL;if(!b||!size||!ordinary(b->handle)||b->snapshot.length>BODY_LIMIT){fail(e,XIDE_MODEL_INVALID,"Invalid edit recovery body.");return NULL;}
 xide_file_snapshot current={0};if(!xideFileSnapshotInspectHandle(b->handle,&current,e)||!bodyEqual(&current,&b->snapshot)){fail(e,XIDE_MODEL_CONFLICT,"Edit recovery body changed.");return NULL;}
 unsigned char* data=xrtMalloc((size_t)current.length+1);if(!data){fail(e,XIDE_MODEL_MEMORY,"Cannot read edit recovery body.");return NULL;}LARGE_INTEGER zero={0};int okay=SetFilePointerEx(b->handle,zero,NULL,FILE_BEGIN)!=0;size_t done=0;
 while(okay&&done<current.length){if(stopped(cancel,user,e)){okay=0;break;}DWORD got=0,want=current.length-done>65536?65536:(DWORD)(current.length-done);okay=ReadFile(b->handle,data+done,want,&got,NULL)&&got==want;done+=got;}
 unsigned char digest[32];if(okay)okay=hash(data,done,digest,cancel,user,e)&&!memcmp(digest,b->snapshot.sha256,32);
 if(!okay){xrtFree(data);if(!e||!e->code)fail(e,XIDE_MODEL_IO,"Cannot verify edit recovery bytes.");return NULL;}data[done]=0;*size=done;return data;
}
static int bodyIndex(const xvalue* row,const char* key,size_t count,size_t* index)
{int64_t n=0;if(!integer(get(row,key),&n)||n<0||(uint64_t)n>=count)return 0;*index=(size_t)n;return 1;}
static int normalized(const unsigned char* bytes,size_t size,xide_model_error* e)
{
 xide_text_file decoded={0};int okay=xideTextFileDecode(bytes,size,&decoded,e)&&strlen(decoded.text)==size&&!memcmp(decoded.text,bytes,size);xideTextFileClear(&decoded);return okay||fail(e,XIDE_MODEL_INVALID,"Recovery text is not normalized valid UTF-8.");
}
static int verifyRows(xide_edit_spool* p,const xvalue* root,int(*cancel)(void*),void* user,xide_model_error* e)
{
 const xvalue* docs=get(root,"documents");const xvalue* notes=get(root,"annotations");if(!xrtValueIs(notes,XVALUE_ARRAY)||xrtValueCount(notes)>4096)return fail(e,XIDE_MODEL_INVALID,"Invalid edit recovery annotations.");int required=0;int consent=0;const char* state=string(get(root,"state")),*id=string(get(root,"id"));
 if(!state||strcmp(state,"prepared")||!id||strncmp(id,"edit-",5)||strlen(id)!=41||!readFlag(get(root,"approved"),&consent)||consent||!xrtValueIs(docs,XVALUE_ARRAY)||xrtValueCount(docs)>512)return fail(e,XIDE_MODEL_INVALID,"Invalid prepared edit recovery record.");
 for(size_t i=0;i<xrtValueCount(docs);++i){
  if(stopped(cancel,user,e))return 0;
  const xvalue* row=xrtValueArrayGet(docs,i);const char *uri=string(get(row,"uri")),*path=string(get(row,"path")),*kind=string(get(row,"kind"));uint64_t identity=0,revision=0,document=0;int64_t version=0,eol=0;int bom=0,changed=0;size_t baseline=0,before=0,after=0,encoded=0;
  if(!xrtValueIs(row,XVALUE_OBJECT)||!uri||!path||!kind||(strcmp(kind,"disk")&&strcmp(kind,"overlay"))||!readNumber(get(row,"document"),&document)||document!=i||!readNumber(get(row,"identity"),&identity)||!readNumber(get(row,"revision"),&revision)||!readFlag(get(row,"bom"),&bom)||!integer(get(row,"eol"),&eol)||eol<XIDE_TEXT_EOL_LF||eol>XIDE_TEXT_EOL_CR||!readFlag(get(row,"changed"),&changed)||!bodyIndex(row,"baseline",p->bodies.Count,&baseline)||!bodyIndex(row,"before",p->bodies.Count,&before)||!bodyIndex(row,"after",p->bodies.Count,&after)||!bodyIndex(row,"encoded",p->bodies.Count,&encoded))return fail(e,XIDE_MODEL_INVALID,"Invalid edit recovery document metadata.");
  const xvalue* links=get(row,"annotations");if(!xrtValueIs(links,XVALUE_ARRAY)||xrtValueCount(links)>4096)return fail(e,XIDE_MODEL_INVALID,"Invalid per-document recovery annotation links.");int64_t previous=-1;
  for(size_t j=0;j<xrtValueCount(links);++j){int64_t at=0;int confirm=0;if(!integer(xrtValueArrayGet(links,j),&at)||at<=previous||(uint64_t)at>=xrtValueCount(notes)||!readFlag(get(xrtValueArrayGet(notes,(size_t)at),"needsConfirmation"),&confirm))return fail(e,XIDE_MODEL_INVALID,"Invalid recovery annotation association.");previous=at;required|=confirm;}
  int overlay=!strcmp(kind,"overlay");if(overlay?(!identity||!revision||revision==UINT64_MAX||!integer(get(row,"version"),&version)||version<INT_MIN||version>INT_MAX):!xrtValueIs(get(row,"version"),XVALUE_NULL))return fail(e,XIDE_MODEL_INVALID,"Invalid edit recovery version/identity.");
  char* fromUri=xideLspUriToPath(uri,e);int same=fromUri&&xidePathLexicalEqual(path,fromUri);free(fromUri);if(!same)return fail(e,XIDE_MODEL_INVALID,"Edit recovery path/URI disagree.");
  size_t rawSize=0,beforeSize=0;unsigned char* raw=xideEditSpoolReadBody(p,baseline,&rawSize,cancel,user,e);if(!raw)return 0;xide_text_file saved={0};int okay=xideTextFileDecode(raw,rawSize,&saved,e)&&saved.bom==bom&&saved.eol==eol;xrtFree(raw);
  size_t savedSize=okay?strlen(saved.text):0;unsigned char savedHash[32]={0};if(okay)okay=hash((const unsigned char*)saved.text,savedSize,savedHash,cancel,user,e);
  unsigned char* original=okay?xideEditSpoolReadBody(p,before,&beforeSize,cancel,user,e):NULL;
  if(!original||beforeSize>8u*1024u*1024u)okay=0;
  if(okay&&!overlay)okay=strlen(saved.text)==beforeSize&&!memcmp(saved.text,original,beforeSize);
  xideTextFileClear(&saved);if(okay&&overlay)okay=normalized(original,beforeSize,e);
  xrtFree(original);if(!okay)return fail(e,e&&e->code?e->code:XIDE_MODEL_INVALID,"Edit recovery baseline/text/codec disagree.");
  if(!overlay){xide_file_snapshot disk={0};const spool_body* b=xrtArrayConstGet(&p->bodies,baseline);if(!readSnapshot(get(row,"disk"),&disk)||disk.length!=b->snapshot.length||memcmp(disk.sha256,b->snapshot.sha256,32))return fail(e,XIDE_MODEL_INVALID,"Edit recovery disk proof disagrees with before bytes.");}
  else if(!xrtValueIs(get(row,"disk"),XVALUE_NULL))return fail(e,XIDE_MODEL_INVALID,"Overlay recovery row contains a disk proof.");
  size_t afterSize=0,encodedSize=0;unsigned char* next=xideEditSpoolReadBody(p,after,&afterSize,cancel,user,e);if(!next)return 0;xide_text_file serial={0};okay=afterSize<=8u*1024u*1024u&&normalized(next,afterSize,e);
  const spool_body* afterBody=xrtArrayConstGet(&p->bodies,after);int savedAfter=okay&&afterSize==savedSize&&!memcmp(afterBody->snapshot.sha256,savedHash,32);
  if(savedAfter){const spool_body *base=xrtArrayConstGet(&p->bodies,baseline),*output=xrtArrayConstGet(&p->bodies,encoded);okay=base->snapshot.length==output->snapshot.length&&!memcmp(base->snapshot.sha256,output->snapshot.sha256,32);}
  else if(okay){okay=xideTextFileEncode((const char*)next,(int)afterSize,bom,(int)eol,&serial,e);unsigned char* stored=okay?xideEditSpoolReadBody(p,encoded,&encodedSize,cancel,user,e):NULL;if(!stored)okay=0;if(okay)okay=serial.rawLength==encodedSize&&!memcmp(serial.raw,stored,encodedSize);xrtFree(stored);}
  xrtFree(next);xideTextFileClear(&serial);if(!okay)return fail(e,e&&e->code?e->code:XIDE_MODEL_INVALID,"Edit recovery encoded output disagrees with normalized after text.");
  const spool_body *a=xrtArrayConstGet(&p->bodies,before),*b=xrtArrayConstGet(&p->bodies,after);int differs=a->snapshot.length!=b->snapshot.length||memcmp(a->snapshot.sha256,b->snapshot.sha256,32);if(changed!=!!differs)return fail(e,XIDE_MODEL_INVALID,"Edit recovery change marker disagrees with text.");
 }
 const xvalue* annotations=get(root,"annotations");if(!xrtValueIs(annotations,XVALUE_ARRAY)||xrtValueCount(annotations)>4096)return fail(e,XIDE_MODEL_INVALID,"Invalid edit recovery annotations.");
 for(size_t i=0;i<xrtValueCount(annotations);++i){const xvalue* a=xrtValueArrayGet(annotations,i);int confirm=0;if(!string(get(a,"id"))||!string(get(a,"label"))||!string(get(a,"description"))||!readFlag(get(a,"needsConfirmation"),&confirm))return fail(e,XIDE_MODEL_INVALID,"Invalid edit recovery annotation.");}
 int claimed=0;if(!readFlag(get(root,"needsConfirmation"),&claimed)||claimed!=required)return fail(e,XIDE_MODEL_INVALID,"Recovery confirmation flag disagrees with linked annotations.");
 const xvalue* binding=get(root,"binding");if(!xrtValueIs(binding,XVALUE_OBJECT))return fail(e,XIDE_MODEL_INVALID,"Invalid edit recovery binding context.");size_t bindingBytes=0;char* text=xrtJsonStringify(binding,false,&bindingBytes);int okay=text&&bindingBytes<=65536;xrtFree(text);return okay||fail(e,XIDE_MODEL_INVALID,"Edit recovery binding exceeds64KiB.");
}
static int pinMarker(xide_edit_spool* p,xide_model_error* e)
{
 p->markerHandle=openFile(p->marker.path,GENERIC_READ,OPEN_EXISTING);if(!ordinary(p->markerHandle))return fail(e,XIDE_MODEL_CONFLICT,"Cannot pin an ordinary immutable edit recovery marker.");
 xide_state_file disk={0};int okay=xideStateOpen(&disk,p->marker.path,SPOOL_FORMAT,e)&&disk.existed&&xrtValueEqual(disk.source,p->marker.source);xideStateClear(&disk);return okay||fail(e,XIDE_MODEL_CONFLICT,"Edit recovery marker changed before it was pinned.");
}
xide_edit_spool* xideEditSpoolPrepare(xide_workspace_edit* plan,const xide_edit_spool_baseline* views,size_t count,const char* directory,const xvalue* binding,size_t budget,int(*cancel)(void*),void* user,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(!plan||!directory||!*directory||strlen(directory)>32768||count>512||(count&&!views)){fail(e,XIDE_MODEL_INVALID,"Invalid edit recovery inputs or dedicated directory.");return NULL;}
 xide_workspace_edit_info info;size_t limit=budget?budget:128u*1024u*1024u;
 if(!xideWorkspaceEditInfo(plan,&info)||info.documents>512||count>512||(count&&!views)||limit>256u*1024u*1024u||info.bytes>limit||CODEC_RESERVE>limit-info.bytes){fail(e,XIDE_MODEL_MEMORY,"Invalid or excessive edit recovery reservation.");return NULL;}
 if(binding&&!xrtValueIs(binding,XVALUE_OBJECT)){fail(e,XIDE_MODEL_INVALID,"Recovery binding must be an object.");return NULL;}
 size_t bindingSize=0;char* bindingText=binding?xrtJsonStringify(binding,false,&bindingSize):NULL;if(binding&&(!bindingText||bindingSize>65536)){xrtFree(bindingText);fail(e,XIDE_MODEL_INVALID,"Recovery binding exceeds64KiB.");return NULL;}xrtFree(bindingText);
 xide_edit_spool* p=begin(directory,limit,e);if(!p)return NULL;xvalue* root=NULL;p->plan=xideWorkspaceEditRetain(plan);int okay=p->plan&&!p->marker.existed&&charge(p,info.bytes+CODEC_RESERVE,e)&&capacity(p,e);if(p->marker.existed)fail(e,XIDE_MODEL_CONFLICT,"A previous edit recovery package is pending; it is preserved.");
 size_t selected[512];for(size_t i=0;i<512;++i)selected[i]=SIZE_MAX;
 for(size_t i=0;okay&&i<count;++i){const xide_text_file* f=views[i].file;if(views[i].document>=info.documents||selected[views[i].document]!=SIZE_MAX||!f||!f->raw||!f->text||f->rawLength>BODY_LIMIT){okay=fail(e,XIDE_MODEL_INVALID,"Invalid saved-baseline view for edit recovery.");break;}selected[views[i].document]=i;okay=charge(p,2*(f->rawLength+1),e);}
 size_t pathBytes=0;for(size_t i=0;okay&&i<info.documents;++i){xide_workspace_edit_document d;okay=xideWorkspaceEditDocumentAt(plan,i,&d);if(!okay)break;if(!!d.hasVersion!=(selected[i]!=SIZE_MAX)){okay=fail(e,XIDE_MODEL_INVALID,"Recovery requires exactly the open-target baseline views.");break;}size_t bytes=strlen(d.uri)+strlen(d.path)+2;if(bytes>2u*1024u*1024u-pathBytes){okay=fail(e,XIDE_MODEL_MEMORY,"Edit recovery path metadata exceeds2MiB.");break;}pathBytes+=bytes;}
 if(okay)okay=charge(p,pathBytes+info.documents*4*(sizeof(spool_body)+sizeof(body_key)+sizeof(size_t)),e)&&xrtArrayReserve(&p->bodies,info.documents?info.documents*4:1);
 if(okay){p->sources=xideWorkspaceEditFilesPin(plan,cancel,user,e);okay=p->sources!=NULL;}
 if(okay){p->id=xideCreateId("edit");root=xrtValueObject();okay=p->id&&root&&put(root,"format",SPOOL_FORMAT)&&put(root,"state","prepared")&&put(root,"id",p->id)&&set(root,"approved",xrtValueBool(false))&&set(root,"needsConfirmation",xrtValueBool(info.needsConfirmation!=0))&&set(root,"documents",xrtValueArray())&&set(root,"annotations",xrtValueArray())&&set(root,"binding",binding?xrtValueDeepClone(binding):xrtValueObject());}
 for(size_t i=0;okay&&i<info.documents;++i){
  if(stopped(cancel,user,e)){okay=0;break;}xide_workspace_edit_document d;okay=xideWorkspaceEditDocumentAt(plan,i,&d);if(!okay)break;size_t source=SIZE_MAX,refs[4]={0};int bom=0,eol=0;xide_file_snapshot original={0};const xide_text_file* saved=d.hasVersion?views[selected[i]].file:NULL;
  if(d.hasVersion){xide_text_file checked={0};okay=xideTextFileDecode(saved->raw,saved->rawLength,&checked,e)&&checked.bom==saved->bom&&checked.eol==saved->eol&&!strcmp(checked.text,saved->text);xideTextFileClear(&checked);bom=saved->bom;eol=saved->eol;if(okay)okay=addBody(p,saved->raw,saved->rawLength,SIZE_MAX,refs,cancel,user,e);}
  else{for(size_t j=0;j<xideWorkspaceEditFilesCount(p->sources);++j){size_t document=0;if(xideWorkspaceEditFilesAt(p->sources,j,&document,&original)&&document==i){source=j;break;}}okay=source!=SIZE_MAX&&xideWorkspaceEditFilesCodecAt(p->sources,source,&bom,&eol)&&addBody(p,NULL,0,source,refs,cancel,user,e);}
  if(okay)okay=addBody(p,(const unsigned char*)xlsTextData(d.before),xlsTextSize(d.before),SIZE_MAX,refs+1,cancel,user,e)&&addBody(p,(const unsigned char*)xlsTextData(d.after),xlsTextSize(d.after),SIZE_MAX,refs+2,cancel,user,e);
  int savedAfter=d.hasVersion?saved&&strlen(saved->text)==xlsTextSize(d.after)&&!memcmp(saved->text,xlsTextData(d.after),xlsTextSize(d.after)):!d.changed;
  xide_text_file encoded={0};if(okay&&savedAfter)refs[3]=refs[0];else if(okay)okay=xideTextFileEncode(xlsTextData(d.after),(int)xlsTextSize(d.after),bom,eol,&encoded,e)&&addBody(p,encoded.raw,encoded.rawLength,SIZE_MAX,refs+3,cancel,user,e);xideTextFileClear(&encoded);
  if(!okay)break;
  unsigned char associated[4096]={0};for(size_t j=0;j<d.count;++j){xide_workspace_text_edit edit;if(!xideWorkspaceEditAt(plan,d.first+j,&edit)){okay=0;break;}if(edit.annotation!=SIZE_MAX)associated[edit.annotation]=1;}
  xvalue* links=xrtValueArray();if(!links)okay=0;for(size_t j=0;okay&&j<info.annotations;++j)if(associated[j])okay=xrtValueArrayAppendNew(links,xrtValueInt((int64)j));
  xvalue* row=okay?xrtValueObject():NULL;okay=row&&number(row,"document",i)&&put(row,"uri",d.uri)&&put(row,"path",d.path)&&put(row,"kind",d.hasVersion?"overlay":"disk")&&number(row,"identity",d.identity)&&number(row,"revision",d.revision)&&set(row,"version",d.hasVersion?xrtValueInt(d.version):xrtValueNull())&&set(row,"bom",xrtValueBool(bom!=0))&&set(row,"eol",xrtValueInt(eol))&&set(row,"changed",xrtValueBool(d.changed!=0))&&set(row,"baseline",xrtValueInt((int64)refs[0]))&&set(row,"before",xrtValueInt((int64)refs[1]))&&set(row,"after",xrtValueInt((int64)refs[2]))&&set(row,"encoded",xrtValueInt((int64)refs[3]))&&set(row,"disk",d.hasVersion?xrtValueNull():snapshotValue(&original));if(okay){okay=set(row,"annotations",links);links=NULL;}if(okay){okay=xrtValueArrayAppendNew(get(root,"documents"),row);row=NULL;}xrtValueRelease(row);xrtValueRelease(links);
 }
 for(size_t i=0;okay&&i<info.annotations;++i){xide_workspace_edit_annotation a;okay=xideWorkspaceEditAnnotationAt(plan,i,&a);xvalue* row=okay?xrtValueObject():NULL;if(okay)okay=row&&set(row,"id",xrtValueString(a.id))&&set(row,"label",xrtValueString(a.label))&&set(row,"description",xrtValueString(a.description))&&set(row,"needsConfirmation",xrtValueBool(a.needsConfirmation!=0));if(okay){okay=xrtValueArrayAppendNew(get(root,"annotations"),row);row=NULL;}xrtValueRelease(row);}
 if(okay){xvalue* bodies=xrtValueArray();okay=bodies!=NULL;for(size_t i=0;okay&&i<p->bodies.Count;++i){spool_body* b=xrtArrayGet(&p->bodies,i);xvalue* row=snapshotValue(&b->snapshot);okay=row&&put(row,"ref",b->ref);if(okay){okay=xrtValueArrayAppendNew(bodies,row);row=NULL;}xrtValueRelease(row);}if(okay){okay=set(root,"bodies",bodies);bodies=NULL;}xrtValueRelease(bodies);}
 if(okay)okay=verifyRows(p,root,cancel,user,e)&&xideWorkspaceEditFilesValidate(p->sources,cancel,user,e)&&!stopped(cancel,user,e);
 if(okay){size_t size=0;char* serial=xrtJsonStringify(root,true,&size);okay=serial&&size<=8u*1024u*1024u;xrtFree(serial);if(!okay)fail(e,XIDE_MODEL_MEMORY,"Edit recovery marker exceeds8MiB.");}
 if(okay){p->markerAttempted=1;okay=xideStateWrite(&p->marker,root,e);}
 if(okay)okay=pinMarker(p,e);
 xrtValueRelease(root);if(okay){xideWorkspaceEditRelease(p->plan);p->plan=NULL;}if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot prepare edit recovery package.");release(p,1);return NULL;}return p;
}
xide_edit_spool* xideEditSpoolOpen(const char* directory,int(*cancel)(void*),void* user,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(!directory||!*directory||strlen(directory)>32768){fail(e,XIDE_MODEL_INVALID,"Missing dedicated edit recovery directory.");return NULL;}
 xide_edit_spool* p=begin(directory,256u*1024u*1024u,e);if(!p)return NULL;int okay=p->marker.existed&&pinMarker(p,e);const xvalue* bodies=get(p->marker.source,"bodies");const char* id=string(get(p->marker.source,"id"));if(!okay||!id||strlen(id)!=41||strncmp(id,"edit-",5)||!xrtValueIs(bodies,XVALUE_ARRAY)||xrtValueCount(bodies)>2048){if(e&&!e->code)fail(e,XIDE_MODEL_INVALID,"Missing or invalid edit recovery body inventory.");release(p,0);return NULL;}
 uint64_t total=0;for(size_t i=0;okay&&i<xrtValueCount(bodies);++i){
  if(stopped(cancel,user,e)){okay=0;break;}const xvalue* row=xrtValueArrayGet(bodies,i);const char* ref=string(get(row,"ref"));spool_body body={.handle=INVALID_HANDLE_VALUE};char expected[128];int count=snprintf(expected,sizeof(expected),"recovery/%s-%04zu.body",id,i);
  okay=ref&&count>0&&count<(int)sizeof(expected)&&!strcmp(ref,expected)&&readSnapshot(row,&body.snapshot)&&body.snapshot.length<=BODY_LIMIT&&body.snapshot.length<=DISK_LIMIT-total;if(!okay)break;total+=body.snapshot.length;memcpy(body.ref,ref,strlen(ref)+1);if(!xrtArrayPush(&p->bodies,&body)){okay=0;break;}spool_body* b=xrtArrayGet(&p->bodies,i);char* path=xideRecoveryPath(p->guard.directory,b->ref,e);b->handle=path?openFile(path,GENERIC_READ|DELETE,OPEN_EXISTING):INVALID_HANDLE_VALUE;free(path);xide_file_snapshot actual={0};okay=ordinary(b->handle)&&xideFileSnapshotInspectHandle(b->handle,&actual,e)&&bodyEqual(&actual,&b->snapshot);
 }
 if(okay)okay=verifyRows(p,p->marker.source,cancel,user,e);
 if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_INVALID,"Edit recovery payload is missing, changed or inconsistent; files are preserved.");release(p,0);return NULL;}p->markerAttempted=1;return p;
}
