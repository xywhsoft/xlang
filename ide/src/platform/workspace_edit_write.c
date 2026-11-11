#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "platform/workspace_edit_write.h"
#include "platform/workspace_edit_files_internal.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#define WRITE_BODY_LIMIT (16u*1024u*1024u)
struct xide_edit_file_write {
 xide_workspace_edit_files* files;size_t file;
 unsigned char *before,*after;size_t beforeSize,afterSize;
 unsigned char afterHash[32];xide_edit_file_status status;int truncated;uint64_t pendingTime;
};
static int fail(xide_model_error* e,int code,const char* text){if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",text);}return 0;}
static xvalue* field(const xvalue* o,const char* key){return xrtValueObjectGet(o,xrtStrView(key));}
static int indexAt(const xvalue* row,const char* key,size_t* at){int64 n=0;if(!xrtValueGetInt(field(row,key),&n)||n<0||n>2047)return 0;*at=(size_t)n;return 1;}
static int digest(const unsigned char* data,size_t size,unsigned char out[32]){xsha256 h;xrtSha256Init(&h);return xrtSha256Update(&h,data,size)&&xrtSha256Final(&h,out);}
static int sameContent(const xide_file_snapshot* s,size_t size,const unsigned char hash[32]){return s->length==size&&!memcmp(s->sha256,hash,32);}
static int cancelled(int(*fn)(void*),void* user,xide_model_error* e){return fn&&fn(user)?!fail(e,XIDE_MODEL_CONFLICT,"File write cancelled; inspect its phase before retirement."):0;}
static int handle(xide_edit_file_write* w,HANDLE* out,xide_model_error* e)
{void* h=NULL;if(!xideWorkspaceEditFilesBorrowWriteHandle(w->files,w->file,&h,e))return 0;*out=(HANDLE)h;return 1;}
static int observe(xide_edit_file_write* w,HANDLE* h,xide_file_snapshot* s,xide_model_error* e)
{
 if(!handle(w,h,e))return 0;
 LARGE_INTEGER size={0};size_t max=w->beforeSize>w->afterSize?w->beforeSize:w->afterSize;
 if(!GetFileSizeEx(*h,&size)||size.QuadPart<0||(uint64_t)size.QuadPart>max)return fail(e,XIDE_MODEL_CONFLICT,"Pinned write source has an unexpected length.");
 return xideFileSnapshotInspectHandle(*h,s,e);
}
static int exact(xide_edit_file_write* w,HANDLE* h,const xide_file_snapshot* expected,xide_model_error* e)
{xide_file_snapshot s={0};if(!observe(w,h,&s,e)){w->status.currentVerified=0;return 0;}if(!xideFileSnapshotEqual(&s,expected)){w->status.currentVerified=0;return fail(e,XIDE_MODEL_CONFLICT,"Pinned write source changed since its verified state.");}w->status.current=s;w->status.currentVerified=1;return 1;}
void xideEditFileWriteDestroy(xide_edit_file_write* w){if(w){if(w->after!=w->before)xrtFree(w->after);xrtFree(w->before);xrtFree(w);}}
int xideEditFileWriteStatus(const xide_edit_file_write* w,xide_edit_file_status* out){if(out)memset(out,0,sizeof(*out));if(!w||!out)return 0;*out=w->status;return 1;}
size_t xideEditFileWriteStorage(const xide_edit_file_write* w){return w?sizeof(*w)+w->beforeSize+1+(w->before==w->after?0:w->afterSize+1):0;}
int xideEditFileWriteVerify(xide_edit_file_write* w,xide_model_error* e)
{if(e)memset(e,0,sizeof(*e));if(!w||!w->status.currentVerified||w->status.phase==XIDE_EDIT_FILE_PARTIAL||w->status.phase==XIDE_EDIT_FILE_IN_DOUBT)return fail(e,XIDE_MODEL_CONFLICT,"No complete pinned write observation is available.");HANDLE h=INVALID_HANDLE_VALUE;xide_file_snapshot expected=w->status.current;return exact(w,&h,&expected,e);}
xide_edit_file_write* xideEditFileWritePrepare(xide_edit_spool* spool,size_t document,size_t budget,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 xide_workspace_edit_files* files=xideEditSpoolSources(spool);const xvalue* root=xideEditSpoolManifest(spool),*rows=root?field(root,"documents"):NULL,*row=rows?xrtValueArrayGet(rows,document):NULL;size_t limit=budget?budget:128u*1024u*1024u;
 if(!files||!row||limit>256u*1024u*1024u)return(fail(e,XIDE_MODEL_INVALID,"A fresh pinned recovery package and bounded file target are required."),NULL);
 size_t at=SIZE_MAX; xide_file_snapshot before={0};for(size_t i=0;i<xideWorkspaceEditFilesCount(files);++i){size_t d=SIZE_MAX;if(xideWorkspaceEditFilesAt(files,i,&d,&before)&&d==document){at=i;break;}}
 size_t raw=0,encoded=0;if(at==SIZE_MAX||!indexAt(row,"baseline",&raw)||!indexAt(row,"encoded",&encoded))return(fail(e,XIDE_MODEL_INVALID,"Only closed pinned recovery rows can be written."),NULL);
 const xvalue* bodies=field(root,"bodies");const xvalue *a=xrtValueArrayGet(bodies,raw),*b=xrtValueArrayGet(bodies,encoded);xstrview sa={0},sb={0};
 if(!xrtValueGetString(field(a,"length"),&sa)||!xrtValueGetString(field(b,"length"),&sb))return(fail(e,XIDE_MODEL_INVALID,"Missing recovery body sizes."),NULL);
 /* Body lengths were strictly validated by Spool. Quota precedes body loads. */
 uint64_t as=0,bs=0;for(size_t i=0;i<sa.Size;++i)as=as*10+(unsigned)(sa.Data[i]-'0');for(size_t i=0;i<sb.Size;++i)bs=bs*10+(unsigned)(sb.Data[i]-'0');
 if(as>WRITE_BODY_LIMIT||bs>WRITE_BODY_LIMIT||sizeof(xide_edit_file_write)+as+1+(encoded==raw?0:bs+1)>limit)return(fail(e,XIDE_MODEL_MEMORY,"Pinned file-write buffers exceed their known-storage budget."),NULL);
 xide_edit_file_write* w=xrtCalloc(1,sizeof(*w));if(!w)return(fail(e,XIDE_MODEL_MEMORY,"Cannot allocate pinned file-write attempt."),NULL);w->files=files;w->file=at;w->status=(xide_edit_file_status){.phase=XIDE_EDIT_FILE_BEFORE,.document=document,.before=before,.current=before,.currentVerified=1};
 w->before=xideEditSpoolReadBody(spool,raw,&w->beforeSize,NULL,NULL,e);if(w->before){if(raw==encoded){w->after=w->before;w->afterSize=w->beforeSize;}else w->after=xideEditSpoolReadBody(spool,encoded,&w->afterSize,NULL,NULL,e);}unsigned char hash[32]={0};HANDLE h=INVALID_HANDLE_VALUE;
 int okay=w->before&&w->after&&digest(w->before,w->beforeSize,hash)&&sameContent(&before,w->beforeSize,hash)&&digest(w->after,w->afterSize,w->afterHash)&&exact(w,&h,&before,e);
 if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_CONFLICT,"Pinned original bytes disagree with the recovery package.");xideEditFileWriteDestroy(w);return NULL;}w->status.changed=!sameContent(&before,w->afterSize,w->afterHash);return w;
}
static int writeBody(xide_edit_file_write* w,HANDLE h,const unsigned char* data,size_t size,int restore,int(*stop)(void*),void* user,xide_model_error* e)
{
 LARGE_INTEGER zero={0};if(!SetFilePointerEx(h,zero,NULL,FILE_BEGIN))return fail(e,XIDE_MODEL_IO,"Cannot seek pinned write target.");
 /* Keep a failed write's metadata provable. Explicit final/restore timestamps
  * are recorded separately; suppress this handle's deferred automatic update. */
 FILETIME frozen={UINT32_MAX,UINT32_MAX};if(!SetFileTime(h,NULL,NULL,&frozen))return fail(e,XIDE_MODEL_IO,"Cannot control pinned writer timestamp updates.");
 if(!restore){w->status.knownPrefix=0;w->status.attemptedPrefix=0;w->truncated=0;}
 for(size_t at=0;at<size;){if(!restore&&cancelled(stop,user,e))return 0;DWORD amount=size-at>65536?65536:(DWORD)(size-at),written=0;if(!restore)w->status.attemptedPrefix=at+amount;
  w->status.touched=1;if(!WriteFile(h,data+at,amount,&written,NULL)||written!=amount)return fail(e,XIDE_MODEL_IO,"Pinned source write failed; bytes may be partial and recovery is required.");at+=amount;if(!restore)w->status.knownPrefix=at;
 }
 if(!restore)w->truncated=1;
 w->status.touched=1;if(!SetEndOfFile(h)||!FlushFileBuffers(h))return fail(e,XIDE_MODEL_IO,"Cannot truncate/flush pinned write target; recovery is required.");return 1;
}
int xideEditFileWriteApply(xide_edit_file_write* w,int(*fence)(void*,int),void* user,int(*stop)(void*),void* stopUser,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(!w||!fence||w->status.applyAttempted||w->status.phase!=XIDE_EDIT_FILE_BEFORE)return fail(e,XIDE_MODEL_INVALID,"Pinned apply requires a fresh attempt and a durable host fence.");
 w->status.applyAttempted=1;w->status.currentVerified=0;HANDLE h=INVALID_HANDLE_VALUE;if(cancelled(stop,stopUser,e)||!exact(w,&h,&w->status.before,e))return 0;
 w->status.currentVerified=0;
 if(!fence(user,XIDE_EDIT_FILE_APPLY))return fail(e,XIDE_MODEL_CONFLICT,"The host did not durably authorize this file write.");
 if(cancelled(stop,stopUser,e)||!exact(w,&h,&w->status.before,e))return 0;
 if(!w->status.changed){w->status.phase=XIDE_EDIT_FILE_AFTER;return 1;}
 w->status.phase=XIDE_EDIT_FILE_PARTIAL;w->status.currentVerified=0;
 if(!writeBody(w,h,w->after,w->afterSize,0,stop,stopUser,e))return 0;
 FILETIME time={0};GetSystemTimeAsFileTime(&time);w->pendingTime=((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime;if(!SetFileTime(h,NULL,NULL,&time)||!FlushFileBuffers(h))return fail(e,XIDE_MODEL_IO,"Cannot publish/flush the output timestamp; recovery is required.");
 xide_file_snapshot now={0};if(!observe(w,&h,&now,e)||!sameContent(&now,w->afterSize,w->afterHash))return fail(e,XIDE_MODEL_CONFLICT,"Pinned output did not verify against the encoded after bytes.");
 w->status.current=now;w->status.currentVerified=1;w->status.phase=XIDE_EDIT_FILE_AFTER;return 1;
}
/* Prove that an unsuccessful synchronous write produced this prefix, including
 * unreported short/failed writes and the possible pre/post-truncation length.
 * An arbitrary disk change is never accepted as a partial transaction result. */
static int partial(xide_edit_file_write* w,HANDLE* h,xide_file_snapshot* snapshot,xide_model_error* e)
{
 if(!observe(w,h,snapshot,e))return 0;
 uint32_t archive=(w->status.before.attributes&~FILE_ATTRIBUTE_NORMAL)|FILE_ATTRIBUTE_ARCHIVE;
 if((snapshot->lastWrite!=w->status.before.lastWrite&&snapshot->lastWrite!=w->pendingTime)||(snapshot->attributes!=w->status.before.attributes&&snapshot->attributes!=archive))return fail(e,XIDE_MODEL_CONFLICT,"Partial source metadata changed outside the recorded write intent.");
 size_t length=(size_t)snapshot->length,low=w->status.knownPrefix,high=w->status.attemptedPrefix;
 if(!w->truncated){if(length<w->beforeSize||length>(w->beforeSize>high?w->beforeSize:high))return fail(e,XIDE_MODEL_CONFLICT,"Partial write length is not explained by this attempt.");if(length>w->beforeSize)low=high=length;else if(high>w->beforeSize)high=w->beforeSize;}
 else if(length!=w->afterSize&&length!=(w->beforeSize>w->afterSize?w->beforeSize:w->afterSize))return fail(e,XIDE_MODEL_CONFLICT,"Unexpected pre/post-truncation source size.");
 LARGE_INTEGER zero={0};if(!SetFilePointerEx(*h,zero,NULL,FILE_BEGIN))return fail(e,XIDE_MODEL_IO,"Cannot inspect partial write bytes.");unsigned char block[65536],hashed[32];xsha256 hash;xrtSha256Init(&hash);
 for(size_t at=0;at<length;){DWORD got=0,want=length-at>sizeof(block)?sizeof(block):(DWORD)(length-at);if(!ReadFile(*h,block,want,&got,NULL)||got!=want||!xrtSha256Update(&hash,block,got))return fail(e,XIDE_MODEL_IO,"Cannot prove partial write bytes.");
  for(size_t j=0;j<got;++j){size_t pos=at+j;if(w->truncated){unsigned char expected=pos<w->afterSize?w->after[pos]:w->before[pos];if(block[j]!=expected)return fail(e,XIDE_MODEL_CONFLICT,"Source contains bytes outside this transaction's output.");}
   else{if(pos>=w->afterSize||block[j]!=w->after[pos]){if(pos<high)high=pos;}if(pos>=w->beforeSize||block[j]!=w->before[pos]){if(pos+1>low)low=pos+1;}}
  }at+=got;
 }
 if((!w->truncated&&low>high)||!xrtSha256Final(&hash,hashed)||memcmp(hashed,snapshot->sha256,32))return fail(e,XIDE_MODEL_CONFLICT,"Partial source does not match a prefix produced by this write.");
 return exact(w,h,snapshot,e);
}
static int restorable(xide_edit_file_write* w,HANDLE* h,xide_file_snapshot* s,xide_model_error* e)
{if(w->status.phase==XIDE_EDIT_FILE_AFTER){*s=w->status.current;return exact(w,h,s,e);}return partial(w,h,s,e);}
int xideEditFileWriteRestore(xide_edit_file_write* w,int(*fence)(void*,int),void* user,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(!w||!fence||w->status.restoreAttempted||w->status.phase==XIDE_EDIT_FILE_RESTORED||w->status.phase==XIDE_EDIT_FILE_IN_DOUBT)return fail(e,XIDE_MODEL_INVALID,"Pinned restore requires an unretired attempt and durable host fence.");
 w->status.restoreAttempted=1;w->status.currentVerified=0;HANDLE h=INVALID_HANDLE_VALUE;
 if(w->status.phase==XIDE_EDIT_FILE_BEFORE||!w->status.changed){if(!exact(w,&h,&w->status.before,e))return 0;w->status.phase=XIDE_EDIT_FILE_RESTORED;return 1;}
 xide_file_snapshot proof={0};if(!restorable(w,&h,&proof,e)){w->status.phase=XIDE_EDIT_FILE_IN_DOUBT;w->status.currentVerified=0;return 0;}
 w->status.currentVerified=0;
 if(!fence(user,XIDE_EDIT_FILE_RESTORE)){w->status.phase=XIDE_EDIT_FILE_IN_DOUBT;return fail(e,XIDE_MODEL_CONFLICT,"The host did not durably authorize restoration.");}
 if(!exact(w,&h,&proof,e)){w->status.phase=XIDE_EDIT_FILE_IN_DOUBT;w->status.currentVerified=0;return 0;}
 w->status.phase=XIDE_EDIT_FILE_IN_DOUBT;w->status.currentVerified=0;
 if(!writeBody(w,h,w->before,w->beforeSize,1,NULL,NULL,e))return 0;
 BY_HANDLE_FILE_INFORMATION info={0};if(!GetFileInformationByHandle(h,&info))return fail(e,XIDE_MODEL_IO,"Cannot read restored source attributes.");
 if(info.dwFileAttributes!=w->status.before.attributes){FILE_BASIC_INFO basic={.FileAttributes=w->status.before.attributes};if(!SetFileInformationByHandle(h,FileBasicInfo,&basic,sizeof(basic)))return fail(e,XIDE_MODEL_IO,"Cannot restore original source attributes.");}
 FILETIME time={(DWORD)w->status.before.lastWrite,(DWORD)(w->status.before.lastWrite>>32)};
 if(!SetFileTime(h,NULL,NULL,&time)||!FlushFileBuffers(h)||!exact(w,&h,&w->status.before,e))return fail(e,XIDE_MODEL_IO,"Restored bytes/metadata did not verify; preserve recovery data.");
 w->status.current=w->status.before;w->status.currentVerified=1;w->status.phase=XIDE_EDIT_FILE_RESTORED;return 1;
}
