#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "platform/workspace_edit_journal.h"
#include "platform/workspace_edit_spool_internal.h"
#include "platform/recovery_store.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#define JOURNAL_LIMIT (8u*1024u*1024u)
#define RECORD_LIMIT 65536u
static const unsigned char magic[8]={'X','L','E','D','T','X','1','\n'};
typedef struct journal_file {int closed;size_t afterSize;unsigned char afterHash[32];xide_file_snapshot before;xide_edit_journal_row row;} journal_file;
struct xide_edit_journal {HANDLE handle;BY_HANDLE_FILE_INFORMATION identity;xide_edit_journal_status status;unsigned char hash[32],markerHash[32];char package[42];journal_file rows[512];};
typedef struct delta {int phase,hasRow;size_t document;xide_edit_journal_row row;} delta;
static int fail(xide_model_error* e,int code,const char* s){if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",s);}return 0;}
static xvalue* get(const xvalue* o,const char* key){return o?xrtValueObjectGet(o,xrtStrView(key)):NULL;}
static const char* text(const xvalue* v){xstrview s={0};return xrtValueGetString(v,&s)&&!memchr(s.Data,0,s.Size)?s.Data:NULL;}
static int put(xvalue* o,const char* k,xvalue* v){return v&&xrtValueObjectSetNew(o,xrtStrView(k),v);}
static int string(xvalue* o,const char* k,const char* v){return put(o,k,xrtValueString(xrtStrView(v)));}
static int number(xvalue* o,const char* k,uint64_t v){char s[32];snprintf(s,sizeof(s),"%llu",(unsigned long long)v);return string(o,k,s);}
static int readNumber(const xvalue* o,const char* k,uint64_t* v){const char* s=text(get(o,k));char* end=NULL;if(!s||!*s||strlen(s)>20||strspn(s,"0123456789")!=strlen(s))return 0;errno=0;unsigned long long n=strtoull(s,&end,10);if(errno||!end||*end)return 0;*v=(uint64_t)n;return 1;}
static int integer(const xvalue* o,const char* k,int* v){int64 n=0;if(!xrtValueGetInt(get(o,k),&n)||n<0||n>INT_MAX)return 0;*v=(int)n;return 1;}
static int flag(const xvalue* o,const char* k,int* v){bool b=false;if(!xrtValueGetBool(get(o,k),&b))return 0;*v=b;return 1;}
static int hex(const unsigned char h[32],char s[65]){size_t n=0;return xrtHexEncode(h,32,s,65,&n,0)&&n==64;}
static int digestValue(const xvalue* o,const char* k,unsigned char h[32]){const char* s=text(get(o,k));size_t n=0;return s&&strlen(s)==64&&xrtHexDecode(xrtStrView(s),h,32,&n,0)&&n==32;}
static xvalue* snapshot(const xide_file_snapshot* s){xvalue* o=xrtValueObject();char h[65];if(!o||!hex(s->sha256,h)||!number(o,"length",s->length)||!number(o,"index",s->index)||!number(o,"lastWrite",s->lastWrite)||!number(o,"volume",s->volume)||!number(o,"attributes",s->attributes)||!string(o,"sha256",h)){xrtValueRelease(o);return NULL;}return o;}
static int readSnapshot(const xvalue* o,xide_file_snapshot* s){uint64_t volume=0,attrs=0;if(!xrtValueIs(o,XVALUE_OBJECT)||!readNumber(o,"length",&s->length)||!readNumber(o,"index",&s->index)||!readNumber(o,"lastWrite",&s->lastWrite)||!readNumber(o,"volume",&volume)||volume>UINT32_MAX||!readNumber(o,"attributes",&attrs)||attrs>UINT32_MAX||!digestValue(o,"sha256",s->sha256))return 0;s->volume=(uint32_t)volume;s->attributes=(uint32_t)attrs;return 1;}
static int regular(HANDLE h,int writable,BY_HANDLE_FILE_INFORMATION* i){return h!=INVALID_HANDLE_VALUE&&GetFileType(h)==FILE_TYPE_DISK&&GetFileInformationByHandle(h,i)&&!(i->dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&(!writable||!(i->dwFileAttributes&FILE_ATTRIBUTE_READONLY))&&i->nNumberOfLinks==1;}
static int location(xide_edit_journal* j,size_t bytes,xide_model_error* e){BY_HANDLE_FILE_INFORMATION i={0};if(!regular(j->handle,!j->status.recovered,&i)||i.dwVolumeSerialNumber!=j->identity.dwVolumeSerialNumber||i.nFileIndexHigh!=j->identity.nFileIndexHigh||i.nFileIndexLow!=j->identity.nFileIndexLow||(((uint64_t)i.nFileSizeHigh<<32)|i.nFileSizeLow)!=bytes)return fail(e,XIDE_MODEL_CONFLICT,"Edit transaction log identity, length or attributes changed.");return 1;}
void xideEditJournalDestroy(xide_edit_journal* j){if(j){if(j->handle!=INVALID_HANDLE_VALUE)CloseHandle(j->handle);xrtFree(j);}}
int xideEditJournalStatus(const xide_edit_journal* j,xide_edit_journal_status* s){if(s)memset(s,0,sizeof(*s));if(!j||!s)return 0;*s=j->status;return 1;}
int xideEditJournalRow(const xide_edit_journal* j,size_t document,xide_edit_journal_row* row){if(row)memset(row,0,sizeof(*row));if(!j||!row||document>=j->status.documents||!j->rows[document].closed)return 0;*row=j->rows[document].row;return 1;}
static xide_edit_journal* allocate(xide_edit_spool* spool,const char** directory,xide_model_error* e)
{
 xide_edit_journal* j=xrtCalloc(1,sizeof(*j));if(!j){fail(e,XIDE_MODEL_MEMORY,"Cannot allocate edit journal state.");return NULL;}j->handle=INVALID_HANDLE_VALUE;
 const xvalue* root=xideEditSpoolManifest(spool),*docs=get(root,"documents"),*bodies=get(root,"bodies");const char* id=text(get(root,"id"));size_t count=xrtValueCount(docs);
 int okay=id&&strlen(id)==41&&count<=512&&xideEditSpoolJournalIdentity(spool,directory,j->markerHash,e);if(okay){memcpy(j->package,id,42);j->status.documents=count;j->status.bytes=8;}
 for(size_t i=0;okay&&i<count;++i){const xvalue* d=xrtValueArrayGet(docs,i);const char* kind=text(get(d,"kind"));if(!kind){okay=0;break;}if(strcmp(kind,"disk"))continue;int encoded=0;journal_file* f=j->rows+i;f->closed=1;f->row.state=XIDE_EDIT_TX_FILE_BEFORE;++j->status.closed;
  uint64_t size=0;okay=readSnapshot(get(d,"disk"),&f->before)&&integer(d,"encoded",&encoded)&&(size_t)encoded<xrtValueCount(bodies);const xvalue* body=okay?xrtValueArrayGet(bodies,(size_t)encoded):NULL;if(okay)okay=readNumber(body,"length",&size)&&size<=16u*1024u*1024u&&digestValue(body,"sha256",f->afterHash);f->afterSize=(size_t)size;
 }
 if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_INVALID,"Invalid prepared package journal metadata.");xideEditJournalDestroy(j);return NULL;}return j;
}
static int statusValue(xvalue* event,const xide_edit_file_status* s)
{return put(event,"filePhase",xrtValueInt(s->phase))&&put(event,"applyAttempted",xrtValueBool(s->applyAttempted!=0))&&put(event,"restoreAttempted",xrtValueBool(s->restoreAttempted!=0))&&put(event,"touched",xrtValueBool(s->touched!=0))&&put(event,"changed",xrtValueBool(s->changed!=0))&&put(event,"verified",xrtValueBool(s->currentVerified!=0))&&number(event,"known",s->knownPrefix)&&number(event,"attempted",s->attemptedPrefix)&&put(event,"before",snapshot(&s->before))&&put(event,"current",s->currentVerified?snapshot(&s->current):xrtValueNull());}
static int readStatus(const xvalue* event,xide_edit_file_status* s)
{
 uint64_t known=0,attempted=0;if(!integer(event,"filePhase",&s->phase)||!flag(event,"applyAttempted",&s->applyAttempted)||!flag(event,"restoreAttempted",&s->restoreAttempted)||!flag(event,"touched",&s->touched)||!flag(event,"changed",&s->changed)||!flag(event,"verified",&s->currentVerified)||!readNumber(event,"known",&known)||!readNumber(event,"attempted",&attempted)||known>attempted||attempted>16u*1024u*1024u||!readSnapshot(get(event,"before"),&s->before))return 0;s->knownPrefix=(size_t)known;s->attemptedPrefix=(size_t)attempted;
 return s->currentVerified?readSnapshot(get(event,"current"),&s->current):xrtValueIs(get(event,"current"),XVALUE_NULL);
}
static int transition(xide_edit_journal* j,const xvalue* event,delta* d,xide_model_error* e)
{
 uint64_t seq=0;const char* type=text(get(event,"type"));memset(d,0,sizeof(*d));d->phase=j->status.phase;if(!xrtValueIs(event,XVALUE_OBJECT)||!type||!readNumber(event,"seq",&seq)||seq!=j->status.records)return fail(e,XIDE_MODEL_INVALID,"Invalid edit journal sequence or event type.");
 if(!j->status.records){unsigned char h[32];int docs=0,closed=0;const char* id=text(get(event,"package"));if(strcmp(type,"begin")||!id||strcmp(id,j->package)||!digestValue(event,"sha256",h)||memcmp(h,j->markerHash,32)||!integer(event,"documents",&docs)||(size_t)docs!=j->status.documents||!integer(event,"closed",&closed)||(size_t)closed!=j->status.closed)return fail(e,XIDE_MODEL_INVALID,"Edit journal does not bind this exact prepared package.");d->phase=XIDE_EDIT_TX_PREPARED;return 1;}
 if(j->status.phase==XIDE_EDIT_TX_COMMITTED||j->status.phase==XIDE_EDIT_TX_ROLLED_BACK||j->status.phase==XIDE_EDIT_TX_IN_DOUBT)return fail(e,XIDE_MODEL_CONFLICT,"Edit transaction is terminal.");
 if(!strcmp(type,"group")){int next=0,noGui=0;if(!integer(event,"phase",&next)||!flag(event,"guiNotCommitted",&noGui))return fail(e,XIDE_MODEL_INVALID,"Invalid edit journal group transition.");int prior=j->status.phase;
  int okay=next==XIDE_EDIT_TX_IN_DOUBT||(next==XIDE_EDIT_TX_APPLYING&&prior==XIDE_EDIT_TX_PREPARED)||(next==XIDE_EDIT_TX_READY&&prior==XIDE_EDIT_TX_APPLYING)||(next==XIDE_EDIT_TX_PUBLISHING&&prior==XIDE_EDIT_TX_READY)||(next==XIDE_EDIT_TX_COMMITTED&&prior==XIDE_EDIT_TX_PUBLISHING)||(next==XIDE_EDIT_TX_ROLLING_BACK&&(prior==XIDE_EDIT_TX_PREPARED||prior==XIDE_EDIT_TX_APPLYING||prior==XIDE_EDIT_TX_READY||(prior==XIDE_EDIT_TX_PUBLISHING&&noGui)))||(next==XIDE_EDIT_TX_ROLLED_BACK&&prior==XIDE_EDIT_TX_ROLLING_BACK);
  if(!okay)return fail(e,XIDE_MODEL_CONFLICT,"Illegal or unproven GUI commit/rollback transition.");
  if(next==XIDE_EDIT_TX_READY||next==XIDE_EDIT_TX_COMMITTED||next==XIDE_EDIT_TX_ROLLED_BACK)for(size_t i=0;i<j->status.documents;++i){journal_file* f=j->rows+i;if(!f->closed)continue;int state=f->row.state;if(next==XIDE_EDIT_TX_ROLLED_BACK?(state!=XIDE_EDIT_TX_FILE_BEFORE&&state!=XIDE_EDIT_TX_FILE_RESTORED):state!=XIDE_EDIT_TX_FILE_AFTER)return fail(e,XIDE_MODEL_CONFLICT,"Not all closed targets have the required acknowledged outcome.");}
  d->phase=next;return 1;
 }
 int document=0;if(!integer(event,"document",&document)||(size_t)document>=j->status.documents||!j->rows[document].closed)return fail(e,XIDE_MODEL_INVALID,"Unknown closed journal target.");journal_file* f=j->rows+document;d->hasRow=1;d->document=(size_t)document;d->row=f->row;
 if(!strcmp(type,"intent")){int action=0;if(!integer(event,"action",&action))return fail(e,XIDE_MODEL_INVALID,"Invalid journal file intent.");int okay=action==XIDE_EDIT_FILE_APPLY?(j->status.phase==XIDE_EDIT_TX_APPLYING&&f->row.state==XIDE_EDIT_TX_FILE_BEFORE):action==XIDE_EDIT_FILE_RESTORE&&(j->status.phase==XIDE_EDIT_TX_ROLLING_BACK&&(f->row.state==XIDE_EDIT_TX_FILE_APPLY_INTENT||f->row.state==XIDE_EDIT_TX_FILE_AFTER||f->row.state==XIDE_EDIT_TX_FILE_PARTIAL));if(!okay)return fail(e,XIDE_MODEL_CONFLICT,"File intent is inconsistent with the transaction.");d->row.state=action==XIDE_EDIT_FILE_APPLY?XIDE_EDIT_TX_FILE_APPLY_INTENT:XIDE_EDIT_TX_FILE_RESTORE_INTENT;return 1;}
 if(strcmp(type,"outcome"))return fail(e,XIDE_MODEL_INVALID,"Unsupported edit journal event.");
 xide_edit_file_status s={.document=(size_t)document};if(!readStatus(event,&s)||!xideFileSnapshotEqual(&s.before,&f->before)||s.changed!=(f->before.length!=f->afterSize||memcmp(f->before.sha256,f->afterHash,32)!=0)||s.attemptedPrefix>f->afterSize)return fail(e,XIDE_MODEL_INVALID,"File observation disagrees with the prepared package.");
 int prior=f->row.state,next=XIDE_EDIT_TX_FILE_UNKNOWN,okay=0;
 if(f->row.observed&&(s.touched<f->row.file.touched||s.applyAttempted<f->row.file.applyAttempted||s.restoreAttempted<f->row.file.restoreAttempted))return fail(e,XIDE_MODEL_CONFLICT,"File observation regressed a recorded attempt or mutation.");
 if(s.phase==XIDE_EDIT_FILE_BEFORE){okay=!s.touched&&(prior==XIDE_EDIT_TX_FILE_BEFORE||prior==XIDE_EDIT_TX_FILE_APPLY_INTENT);if(s.currentVerified){okay=okay&&xideFileSnapshotEqual(&s.current,&f->before);next=XIDE_EDIT_TX_FILE_BEFORE;}}
 else if(s.phase==XIDE_EDIT_FILE_AFTER){okay=j->status.phase==XIDE_EDIT_TX_APPLYING&&prior==XIDE_EDIT_TX_FILE_APPLY_INTENT&&s.applyAttempted&&!s.restoreAttempted&&s.currentVerified&&s.current.volume==f->before.volume&&s.current.index==f->before.index&&s.current.length==f->afterSize&&!memcmp(s.current.sha256,f->afterHash,32)&&!(s.current.attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY));next=XIDE_EDIT_TX_FILE_AFTER;}
 else if(s.phase==XIDE_EDIT_FILE_PARTIAL){okay=j->status.phase==XIDE_EDIT_TX_APPLYING&&prior==XIDE_EDIT_TX_FILE_APPLY_INTENT&&s.applyAttempted&&!s.restoreAttempted&&!s.currentVerified;next=XIDE_EDIT_TX_FILE_PARTIAL;}
 else if(s.phase==XIDE_EDIT_FILE_RESTORED){okay=j->status.phase==XIDE_EDIT_TX_ROLLING_BACK&&s.restoreAttempted&&s.currentVerified&&xideFileSnapshotEqual(&s.current,&f->before)&&(prior==XIDE_EDIT_TX_FILE_RESTORE_INTENT||(!s.touched&&(prior==XIDE_EDIT_TX_FILE_BEFORE||prior==XIDE_EDIT_TX_FILE_APPLY_INTENT||prior==XIDE_EDIT_TX_FILE_UNKNOWN||(!s.changed&&prior==XIDE_EDIT_TX_FILE_AFTER))));next=XIDE_EDIT_TX_FILE_RESTORED;}
 else if(s.phase==XIDE_EDIT_FILE_IN_DOUBT){okay=s.restoreAttempted&&!s.currentVerified&&(prior==XIDE_EDIT_TX_FILE_RESTORE_INTENT||prior==XIDE_EDIT_TX_FILE_AFTER||prior==XIDE_EDIT_TX_FILE_PARTIAL||prior==XIDE_EDIT_TX_FILE_APPLY_INTENT);}
 if(!okay)return fail(e,XIDE_MODEL_CONFLICT,"File observation has no matching write-ahead intent or valid outcome.");
 d->row=(xide_edit_journal_row){.state=next,.observed=1,.file=s};return 1;
}
static void acceptEvent(xide_edit_journal* j,const delta* d,const unsigned char hash[32],size_t bytes){j->status.phase=d->phase;if(d->hasRow)j->rows[d->document].row=d->row;memcpy(j->hash,hash,32);j->status.bytes+=bytes;++j->status.records;}
static void lengthBytes(size_t n,unsigned char out[4]){for(unsigned i=0;i<4;++i)out[i]=(unsigned char)(n>>(8*i));}
static int chain(const unsigned char previous[32],const unsigned char size[4],const unsigned char* body,size_t n,unsigned char hash[32]){xsha256 h;xrtSha256Init(&h);return xrtSha256Update(&h,previous,32)&&xrtSha256Update(&h,size,4)&&xrtSha256Update(&h,body,n)&&xrtSha256Final(&h,hash);}
static int append(xide_edit_journal* j,xvalue* event,xide_model_error* e)
{
 if(!j||j->status.frozen)return fail(e,XIDE_MODEL_CONFLICT,"Frozen/recovered edit journal cannot authorize writes.");
 delta d;int okay=event&&number(event,"seq",j->status.records)&&transition(j,event,&d,e);if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot encode edit journal event.");return 0;}size_t n=0;char* json=xrtJsonStringify(event,false,&n);if(!json||n>RECORD_LIMIT||j->status.records>=4096||n+36>JOURNAL_LIMIT-j->status.bytes){xrtFree(json);return fail(e,XIDE_MODEL_MEMORY,"Edit journal record/storage limit exceeded.");}
 unsigned char* frame=xrtMalloc(n+36);if(!frame){xrtFree(json);return fail(e,XIDE_MODEL_MEMORY,"Cannot allocate journal append frame.");}lengthBytes(n,frame);memcpy(frame+4,json,n);xrtFree(json);okay=chain(j->hash,frame,frame+4,n,frame+n+4);j->status.frozen=1;
 LARGE_INTEGER position={.QuadPart=(LONGLONG)j->status.bytes};DWORD written=0;okay=okay&&location(j,j->status.bytes,e)&&SetFilePointerEx(j->handle,position,NULL,FILE_BEGIN)&&WriteFile(j->handle,frame,(DWORD)(n+36),&written,NULL)&&written==n+36&&FlushFileBuffers(j->handle)&&location(j,j->status.bytes+n+36,e)&&SetFilePointerEx(j->handle,position,NULL,FILE_BEGIN);
 unsigned char block[4096];for(size_t at=0;okay&&at<n+36;){DWORD got=0,want=n+36-at>sizeof(block)?sizeof(block):(DWORD)(n+36-at);okay=ReadFile(j->handle,block,want,&got,NULL)&&got==want&&!memcmp(block,frame+at,want);at+=got;}
 if(okay)okay=location(j,j->status.bytes+n+36,e);
 if(okay){acceptEvent(j,&d,frame+n+4,n+36);j->status.frozen=0;}else if(!e||!e->code)fail(e,XIDE_MODEL_IO,"Edit journal append outcome is uncertain; preserve it and stop writes.");xrtFree(frame);return okay;
}
static xvalue* event(const char* type){xvalue* o=xrtValueObject();if(!o||!string(o,"type",type)){xrtValueRelease(o);return NULL;}return o;}
static HANDLE openLog(const char* directory,int create,xide_model_error* e)
{char* p=xideRecoveryPath(directory,"edit-transaction.log",e);wchar_t* w=p?xidePathNativeWide(p):NULL;free(p);HANDLE h=w?CreateFileW(w,create?GENERIC_READ|GENERIC_WRITE:GENERIC_READ,FILE_SHARE_READ,NULL,create?CREATE_NEW:OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,NULL):INVALID_HANDLE_VALUE;DWORD code=GetLastError();free(w);if(h==INVALID_HANDLE_VALUE&&(code==ERROR_FILE_EXISTS||code==ERROR_ALREADY_EXISTS||code==ERROR_SHARING_VIOLATION))fail(e,XIDE_MODEL_CONFLICT,"An existing or active transaction log is preserved.");return h;}
xide_edit_journal* xideEditJournalCreate(xide_edit_spool* spool,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(!xideEditSpoolSources(spool)){fail(e,XIDE_MODEL_INVALID,"A live pinned package is required to create a transaction log.");return NULL;}const char* directory=NULL;xide_edit_journal* j=allocate(spool,&directory,e);if(!j)return NULL;j->handle=openLog(directory,1,e);int okay=regular(j->handle,1,&j->identity)&&!j->identity.nFileSizeHigh&&!j->identity.nFileSizeLow;DWORD wrote=0;if(okay)okay=WriteFile(j->handle,magic,8,&wrote,NULL)&&wrote==8&&FlushFileBuffers(j->handle);char h[65];xvalue* root=okay?event("begin"):NULL;
 if(okay){LARGE_INTEGER zero={0};unsigned char check[8];DWORD got=0;okay=SetFilePointerEx(j->handle,zero,NULL,FILE_BEGIN)&&ReadFile(j->handle,check,8,&got,NULL)&&got==8&&!memcmp(check,magic,8);}
 if(okay)okay=root&&hex(j->markerHash,h)&&string(root,"package",j->package)&&string(root,"sha256",h)&&put(root,"documents",xrtValueInt((int64)j->status.documents))&&put(root,"closed",xrtValueInt((int64)j->status.closed))&&append(j,root,e);
 xrtValueRelease(root);if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_IO,"Cannot create a new ordinary edit transaction log; existing bytes are preserved.");xideEditJournalDestroy(j);return NULL;}return j;
}
static int readExact(HANDLE h,void* buffer,size_t n){DWORD got=0;return n<=RECORD_LIMIT+36&&ReadFile(h,buffer,(DWORD)n,&got,NULL)&&got==n;}
xide_edit_journal* xideEditJournalOpen(xide_edit_spool* spool,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 const char* directory=NULL;xide_edit_journal* j=allocate(spool,&directory,e);if(!j)return NULL;j->status.frozen=j->status.recovered=1;j->handle=openLog(directory,0,e);int okay=regular(j->handle,0,&j->identity);uint64_t length=((uint64_t)j->identity.nFileSizeHigh<<32)|j->identity.nFileSizeLow;unsigned char start[8];if(okay)okay=length>=8&&length<=JOURNAL_LIMIT&&readExact(j->handle,start,8)&&!memcmp(start,magic,8);
 unsigned char* body=okay?xrtMalloc(RECORD_LIMIT+36):NULL;if(okay&&!body)okay=fail(e,XIDE_MODEL_MEMORY,"Cannot inspect edit journal records.");
 while(okay&&j->status.bytes<length){size_t remaining=(size_t)length-j->status.bytes;if(remaining<4){j->status.incompleteTail=1;break;}unsigned char size[4],hash[32];if(!readExact(j->handle,size,4)){okay=0;break;}uint32_t n=(uint32_t)size[0]|((uint32_t)size[1]<<8)|((uint32_t)size[2]<<16)|((uint32_t)size[3]<<24);if(!n||n>RECORD_LIMIT||j->status.records>=4096){okay=0;break;}if((size_t)n+36>remaining){j->status.incompleteTail=1;break;}
  if(!readExact(j->handle,body,(size_t)n+32)||!chain(j->hash,size,body,n,hash)||memcmp(hash,body+n,32)||memchr(body,0,n)||!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(const char*)body,(int)n,NULL,0)){okay=0;break;}xvalue* value=xrtJsonParse((xstrview){(const char*)body,n});delta d;okay=value&&transition(j,value,&d,e);xrtValueRelease(value);if(okay)acceptEvent(j,&d,hash,(size_t)n+36);
 }
 xrtFree(body);if(okay)okay=j->status.records&&location(j,(size_t)length,e);if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_INVALID,"Edit journal is invalid, foreign or damaged; all bytes are preserved.");xideEditJournalDestroy(j);return NULL;}return j;
}
int xideEditJournalFileIntent(xide_edit_journal* j,size_t document,int action,xide_model_error* e)
{if(e)memset(e,0,sizeof(*e));if(!j||document>=j->status.documents||!j->rows[document].closed||(action!=XIDE_EDIT_FILE_APPLY&&action!=XIDE_EDIT_FILE_RESTORE))return fail(e,XIDE_MODEL_INVALID,"Invalid journal target or file intent.");xvalue* o=event("intent");int okay=o&&put(o,"document",xrtValueInt((int64)document))&&put(o,"action",xrtValueInt(action))&&append(j,o,e);xrtValueRelease(o);if(!okay&&e&&!e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot retain journal file intent.");return okay;}
int xideEditJournalRecordFile(xide_edit_journal* j,const xide_edit_file_write* write,xide_model_error* e)
{if(e)memset(e,0,sizeof(*e));xide_edit_file_status s={0};if(!xideEditFileWriteStatus(write,&s))return fail(e,XIDE_MODEL_INVALID,"Missing verified file-write attempt.");xvalue* o=event("outcome");int okay=o&&s.document<=INT_MAX&&put(o,"document",xrtValueInt((int64)s.document))&&statusValue(o,&s)&&append(j,o,e);xrtValueRelease(o);if(!okay&&e&&!e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot retain file-write outcome.");return okay;}
int xideEditJournalSetPhase(xide_edit_journal* j,int phase,int guiNotCommitted,xide_model_error* e)
{if(e)memset(e,0,sizeof(*e));xvalue* o=event("group");int okay=o&&put(o,"phase",xrtValueInt(phase))&&put(o,"guiNotCommitted",xrtValueBool(guiNotCommitted!=0))&&append(j,o,e);xrtValueRelease(o);if(!okay&&e&&!e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot retain transaction group phase.");return okay;}
