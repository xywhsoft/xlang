#include "workspace_edit_fixture.h"
#include "editor/workspace_edit_transaction.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"mixed-edit:%d: %s\n",__LINE__,#v);exit(1);}}while(0)

static const char* raw[4]={"\xef\xbb\xbf" "alpha\r\n", "int alpha=0;\r\n", "beta\r\n", "func alpha()\nend\n"};
static const char* before[4]={"alpha\n", "int alpha=1;\n", "beta\n", "func alpha()\nend\n"};
static const char* after[4]={"gamma\n", "int gamma=1;\n", "gamma\n", "func gamma()\nend\n"};
static const char* encoded[4]={"\xef\xbb\xbf" "gamma\r\n", "int gamma=1;\r\n", "gamma\r\n", "func gamma()\nend\n"};
static int failJournalFlush,partialSourceWrite,failUnlock,probing;static size_t allocations,failAllocation;
extern void* (*__real___imp_xrtMalloc)(size_t);
extern void* (*__real___imp_xrtCalloc)(size_t,size_t);
static void* mallocProbe(size_t n){if(probing&&++allocations==failAllocation)return NULL;return __real___imp_xrtMalloc(n);}
static void* callocProbe(size_t n,size_t z){if(probing&&++allocations==failAllocation)return NULL;return __real___imp_xrtCalloc(n,z);}
void* (*__wrap___imp_xrtMalloc)(size_t)=mallocProbe;
void* (*__wrap___imp_xrtCalloc)(size_t,size_t)=callocProbe;
extern BOOL (WINAPI* __real___imp_UnlockFileEx)(HANDLE,DWORD,DWORD,DWORD,LPOVERLAPPED);
static BOOL WINAPI unlockProbe(HANDLE h,DWORD reserved,DWORD low,DWORD high,LPOVERLAPPED at)
{if(failUnlock){failUnlock=0;SetLastError(ERROR_LOCK_VIOLATION);return FALSE;}return __real___imp_UnlockFileEx(h,reserved,low,high,at);}
BOOL (WINAPI* __wrap___imp_UnlockFileEx)(HANDLE,DWORD,DWORD,DWORD,LPOVERLAPPED)=unlockProbe;
static DWORD sourceVolume,sourceIndexLow,sourceIndexHigh;
extern BOOL (WINAPI* __real___imp_FlushFileBuffers)(HANDLE);
extern BOOL (WINAPI* __real___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI flushProbe(HANDLE h)
{
 wchar_t path[4096];DWORD n=GetFinalPathNameByHandleW(h,path,4096,0);
 if(failJournalFlush&&n&&n<4096&&wcsstr(path,L"edit-transaction.log")){failJournalFlush=0;SetLastError(ERROR_WRITE_FAULT);return FALSE;}
 return __real___imp_FlushFileBuffers(h);
}
static BOOL WINAPI writeProbe(HANDLE h,LPCVOID p,DWORD n,LPDWORD done,LPOVERLAPPED at)
{
 BY_HANDLE_FILE_INFORMATION i={0};
 if(partialSourceWrite&&GetFileInformationByHandle(h,&i)&&i.dwVolumeSerialNumber==sourceVolume&&i.nFileIndexLow==sourceIndexLow&&i.nFileIndexHigh==sourceIndexHigh){partialSourceWrite=0;CHECK(n>2);CHECK(__real___imp_WriteFile(h,p,2,done,at));SetLastError(ERROR_WRITE_FAULT);return FALSE;}
 return __real___imp_WriteFile(h,p,n,done,at);
}
BOOL (WINAPI* __wrap___imp_FlushFileBuffers)(HANDLE)=flushProbe;
BOOL (WINAPI* __wrap___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED)=writeProbe;

typedef struct fixture {
 xui_context context;xide_document_store store;xide_document* docs[4];
 char *paths[4],*uris[4],*directory;xide_workspace_edit* plan;
 xide_edit_transaction* work;xide_edit_transaction_ticket* ticket;
 uint64_t revisions[4];unsigned mask;int inspect,notices,closeOnNotice,closeStatus;bool consumed;
} fixture;
static HANDLE readHandle(const char* path)
{wchar_t* p=xidePathNativeWide(path);HANDLE h=p?CreateFileW(p,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL):INVALID_HANDLE_VALUE;free(p);return h;}
static void disk(const char* path,const char* text)
{size_t n=0;unsigned char* p=xideIoReadAllLimit(path,4096,&n);CHECK(p&&n==strlen(text)&&!memcmp(p,text,n));xrtFree(p);}
static void hash(const char* text,char out[65])
{xsha256 s;unsigned char digest[32];xrtSha256Init(&s);CHECK(xrtSha256Update(&s,text,strlen(text))&&xrtSha256Final(&s,digest));for(size_t i=0;i<32;++i)snprintf(out+i*2,3,"%02x",digest[i]);}
static void proof(xvalue* row,const char* path,const char* bytes,const char* normal)
{
 char a[65],b[65];hash(bytes,a);hash(normal,b);
 xvalue *extension=xrtValueObject(),*d=xrtValueObject(),*input=xrtValueObject();CHECK(extension&&d&&input);
 CHECK(putEdit(d,"path",xrtValueString(xrtStrView(path)))&&putEdit(d,"origin",xrtValueString(xrtStrView("disk")))&&putEdit(d,"normalizedSize",xrtValueInt((int64)strlen(normal)))&&putEdit(d,"normalizedSha256",xrtValueString(xrtStrView(b))));
 CHECK(putEdit(input,"path",xrtValueString(xrtStrView(path)))&&putEdit(input,"origin",xrtValueString(xrtStrView("disk")))&&putEdit(input,"rawSize",xrtValueInt((int64)strlen(bytes)))&&putEdit(input,"rawSha256",xrtValueString(xrtStrView(a)))&&putEdit(d,"input",input)&&putEdit(extension,"schema",xrtValueInt(1))&&putEdit(extension,"document",d)&&putEdit(row,"xlang",extension));
}
static void notice(xide_document* doc,void* user)
{
 fixture* f=user;(void)doc;if(!f->inspect)return;++f->notices;
 for(unsigned i=0;i<4;++i)if(f->docs[i]){CHECK(f->docs[i]->revision==f->revisions[i]+1);CHECK(!strcmp(xuiCodeEditGetText(f->docs[i]->editor),after[i])&&f->docs[i]->dirty);}
 if(f->closeOnNotice&&f->notices==1)f->closeStatus=xuiDestroyOwnerChecked(f->context,&f->consumed);
}
static void init(fixture* f,const char* root,unsigned serial,unsigned mask)
{
 memset(f,0,sizeof(*f));f->mask=mask;CHECK(xuiCreate(&f->context)==XUI_OK);xideDocumentsInit(&f->store,f->context,NULL,notice,f);
 char name[64];snprintf(name,sizeof(name),"mixed-%u",serial);char* directory=xidePathResolveLexical(root,name);CHECK(directory&&xrtDirCreateAll(directory));f->directory=xidePathResolveLexical(directory,"recovery");CHECK(f->directory);
 xvalue *value=xrtValueObject(),*rows=xrtValueArray(),*annotations=xrtValueObject(),*annotation=xrtValueObject();CHECK(value&&rows&&annotations&&annotation);
 xide_workspace_edit_input inputs[4]={0};xide_model_error e={0};
 for(unsigned i=0;i<4;++i){snprintf(name,sizeof(name),"source-%u.%s",i,i==1?"c":"xl");f->paths[i]=xidePathResolveLexical(directory,name);f->uris[i]=xideLspPathToUri(f->paths[i],&e);CHECK(f->paths[i]&&f->uris[i]);
  const char* saved=(mask&(1u<<i))?raw[i]:(i==1?"int alpha=1;\r\n":raw[i]);CHECK(xrtFileWriteAtomic(f->paths[i],(xbytesview){(const unsigned char*)saved,strlen(saved)}));
  if(mask&(1u<<i)){f->docs[i]=xideDocumentOpen(&f->store,f->paths[i],&e);CHECK(f->docs[i]);if(i==1)CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(f->docs[i]->editor),0,strlen(xuiCodeEditGetText(f->docs[i]->editor)),before[i])==XUI_OK);f->revisions[i]=f->docs[i]->revision;}
  inputs[i]=(xide_workspace_edit_input){f->uris[i],xlsTextCreate(before[i],strlen(before[i]),1024,NULL),f->docs[i]?f->docs[i]->id:0,f->docs[i]?f->revisions[i]:1,f->docs[i]!=NULL,9+(int)i};CHECK(inputs[i].text);
  xvalue* edits=xrtValueArray();unsigned start=i==1?4:i==3?5:0,end=start+(i==2?4:5);xvalue* edit=textEdit(0,start,end,"gamma");CHECK(edits&&edit&&putEdit(edit,"annotationId",xrtValueString(xrtStrView("rename")))&&xrtValueArrayAppendNew(edits,edit));xvalue* row=documentEdit(f->uris[i],inputs[i].hasVersion,inputs[i].version,edits);CHECK(row);if(!f->docs[i])proof(row,f->paths[i],saved,before[i]);CHECK(xrtValueArrayAppendNew(rows,row));
 }
 CHECK(putEdit(annotation,"label",xrtValueString(xrtStrView("rename group")))&&putEdit(annotation,"needsConfirmation",xrtValueBool(true))&&putEdit(annotations,"rename",annotation)&&putEdit(value,"changeAnnotations",annotations)&&putEdit(value,"documentChanges",rows));
 enum xide_workspace_edit_error error;f->plan=xideWorkspaceEditPrepare(value,inputs,4,0,0,NULL,NULL,&error);CHECK(f->plan&&!error);for(unsigned i=0;i<4;++i)xlsTextRelease(inputs[i].text);xrtValueRelease(value);free(directory);
}
typedef struct policy {xide_edit_transaction* work;size_t deny,cancel;int calls,restores,groups;} policy;
static int detached(void* user,size_t document,int action)
{policy* p=user;++p->calls;if(document==SIZE_MAX)++p->groups;if(action==XIDE_EDIT_FILE_RESTORE){++p->restores;return 1;}if(document==p->cancel){xideEditTransactionCancel(p->work);return 1;}return document!=p->deny;}
/* Worker job deliberately contains no fixture/store/ticket/widget/context. */
typedef struct worker {xide_workspace_edit* plan;const char* directory;xide_edit_transaction* work;policy policy;int step,result;DWORD thread;xide_model_error error;} worker;
static DWORD WINAPI runWorker(void* user)
{
 worker* w=user;w->thread=GetCurrentThreadId();
 if(w->step==0){w->work=xideEditTransactionPrepare(w->plan,w->directory,0,NULL,NULL,&w->error);w->result=w->work!=NULL;}
 if(w->step==1)w->result=xideEditTransactionStage(w->work,NULL,NULL,&w->error);
 if(w->step==2)w->result=xideEditTransactionApply(w->work,detached,&w->policy,NULL,NULL,&w->error);
 if(w->step==3)w->result=xideEditTransactionFinish(w->work,detached,&w->policy,&w->error);
 if(w->step==4)w->result=xideEditTransactionDestroy(w->work)==XUI_OK;
 return 0;
}
static int execute(worker* w)
{HANDLE h=CreateThread(NULL,0,runWorker,w,0,NULL);CHECK(h&&WaitForSingleObject(h,30000)==WAIT_OBJECT_0&&w->thread!=GetCurrentThreadId());CHECK(CloseHandle(h));return w->result;}
static worker job(fixture* f,int step)
{worker w={.plan=f->plan,.directory=f->directory,.work=f->work,.step=step};w.policy=(policy){.work=f->work,.deny=SIZE_MAX-1,.cancel=SIZE_MAX-1};return w;}
static int hostAccept(void* user)
{fixture* f=user;CHECK(xideEditTransactionTicketDestroy(f->ticket)==XUI_ERROR_BUSY);return XUI_OK;}
static int reject(void* user){(void)user;return XUI_ERROR_INVALID_STATE;}
static int opening(void* user)
{
 fixture* f=user;/* Cached/prepared app readers are governed by the required host fence.
                 * Simulate a host callback opening a prepared closed target. */
 xide_model_error e={0};xide_text_file text={0};CHECK(xideTextFileDecode((const unsigned char*)raw[0],strlen(raw[0]),&text,&e));
 CHECK(xideDocumentOpenPrepared(&f->store,f->paths[0],&text,xideDocumentsSourceEpoch(&f->store),&e));xideTextFileClear(&text);return XUI_OK;
}
static void captured(fixture* f)
{worker w=job(f,0);CHECK(execute(&w)&&!w.error.code);f->work=w.work;CHECK(xideEditTransactionCapture(&f->store,f->work,&f->ticket)==XUI_OK);}
static void staged(fixture* f)
{captured(f);xide_edit_transaction_status beforeStage,afterStage;CHECK(xideEditTransactionStatus(f->work,&beforeStage));worker w=job(f,1);int result=execute(&w);if(!result)fprintf(stderr,"stage error %d %s\n",w.error.code,w.error.message);CHECK(result);CHECK(xideEditTransactionStatus(f->work,&afterStage)&&afterStage.knownBytes<=256u*1024u*1024u);if(f->mask)CHECK(afterStage.knownBytes>beforeStage.knownBytes+96u*1024u*1024u);CHECK(xideEditTransactionStage(f->work,NULL,NULL,&w.error)==0);}
static xide_edit_transaction_status state(fixture* f)
{xide_edit_transaction_status s;CHECK(xideEditTransactionStatus(f->work,&s));return s;}
static void clear(fixture* f)
{
 CHECK(xideEditTransactionTicketDestroy(f->ticket)==XUI_OK);f->ticket=NULL;worker w=job(f,4);CHECK(execute(&w));f->work=NULL;
 xideWorkspaceEditRelease(f->plan);xideDocumentsClear(&f->store);bool consumed=false;CHECK(xuiDestroyOwnerChecked(f->context,&consumed)==XUI_OK&&consumed);
 for(unsigned i=0;i<4;++i){free(f->paths[i]);free(f->uris[i]);}free(f->directory);xrtClearError();
}
static void closedDisk(fixture* f,int changed)
{for(unsigned i=0;i<4;++i)if(!f->docs[i])disk(f->paths[i],changed?encoded[i]:i==1?"int alpha=1;\r\n":raw[i]);}
static void blocked(fixture* f)
{
 for(unsigned i=0;i<4;++i)if(!f->docs[i]){HANDLE h=readHandle(f->paths[i]);CHECK(h!=INVALID_HANDLE_VALUE);char b[1];DWORD got=0;CHECK(!ReadFile(h,b,1,&got,NULL)&&GetLastError()==ERROR_LOCK_VIOLATION);CHECK(CloseHandle(h));}
}
int main(int argc,char** argv)
{
 CHECK(argc==2&&xrtDirCreateAll(argv[1]));fixture f;unsigned serial=0;
 for(unsigned mode=0;mode<11;++mode){
  unsigned mask=mode==8?0:mode==9?15:10;init(&f,argv[1],serial++,mask);staged(&f);CHECK(state(&f).phase==XIDE_MIX_STAGED);blocked(&f);
  CHECK(xideEditTransactionArm(f.ticket,NULL,NULL)!=XUI_OK);CHECK(xideEditTransactionArm(f.ticket,hostAccept,&f)==XUI_OK);worker w=job(&f,2);
  if(mode==1)w.policy.deny=2;
  if(mode==2)w.policy.cancel=2;
  if(mode==7){HANDLE h=readHandle(f.paths[0]);CHECK(h!=INVALID_HANDLE_VALUE);BY_HANDLE_FILE_INFORMATION i;CHECK(GetFileInformationByHandle(h,&i));CHECK(CloseHandle(h));sourceVolume=i.dwVolumeSerialNumber;sourceIndexHigh=i.nFileIndexHigh;sourceIndexLow=i.nFileIndexLow;partialSourceWrite=1;}
  int applied=execute(&w);
  if(mode==1||mode==2||mode==7){CHECK(!applied&&state(&f).rolledBack&&!state(&f).needsRecovery&&!state(&f).committed);CHECK(w.policy.restores>0);closedDisk(&f,0);for(unsigned i=0;i<4;++i)if(f.docs[i])CHECK(!strcmp(xuiCodeEditGetText(f.docs[i]->editor),before[i]));clear(&f);continue;}
  if(!applied){fprintf(stderr,"apply error %d %s\n",w.error.code,w.error.message);}CHECK(applied&&state(&f).phase==XIDE_MIX_READY);blocked(&f);
  CHECK(xideEditTransactionDestroy(f.work)==XUI_ERROR_BUSY);
  if(mode==3)CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(f.docs[1]->editor),0,0,"user ")==XUI_OK);
  xide_open_edit_outcome out={0};f.inspect=mode!=3&&mode!=4;f.closeOnNotice=mode==5;int status=xideEditTransactionPublish(f.ticket,mode==4?reject:hostAccept,&f,&out);f.inspect=0;
  if(mode==3||mode==4){CHECK(status!=XUI_OK&&!out.committed);w=job(&f,3);CHECK(execute(&w)&&state(&f).rolledBack);closedDisk(&f,0);if(mode==3)CHECK(!strcmp(xuiCodeEditGetText(f.docs[1]->editor),"user int alpha=1;\n"));clear(&f);continue;}
  CHECK(out.committed&&state(&f).committed);CHECK(f.notices==(mask==0?0:mode==5?1:mask==15?4:2));
  if(mode==5)CHECK(f.closeStatus==XUI_STATUS_DEFERRED&&!f.consumed);
  xideEditTransactionCancel(f.work);if(mode==6)failJournalFlush=1;if(mode==10)failUnlock=1;w=job(&f,3);int finished=execute(&w);
  if(mode==6||mode==10)CHECK(!finished&&state(&f).needsRecovery&&state(&f).committed&&!state(&f).rolledBack);
  else CHECK(finished&&state(&f).phase==XIDE_MIX_COMMITTED&&state(&f).committed&&!state(&f).rolledBack);
  if(mode!=6&&mode!=10)closedDisk(&f,1);
  /* Retry never manufactures a new publication decision. */
  CHECK(xideEditTransactionPublish(f.ticket,hostAccept,&f,&out)!=XUI_OK&&!out.committed);
  char* committedPath=xidePathResolveLexical(NULL,f.paths[0]);CHECK(committedPath);clear(&f);disk(committedPath,mask==15?raw[0]:encoded[0]);free(committedPath);
 }
 /* Cached target appears during a host callback: repeat local fences. */
 init(&f,argv[1],serial++,10);staged(&f);CHECK(xideEditTransactionArm(f.ticket,opening,&f)!=XUI_OK);CHECK(state(&f).phase==XIDE_MIX_STAGED);clear(&f);
 /* Ticket abandonment after disk writes is a definite nonpublication. */
 init(&f,argv[1],serial++,10);staged(&f);CHECK(xideEditTransactionArm(f.ticket,hostAccept,&f)==XUI_OK);worker w=job(&f,2);CHECK(execute(&w));CHECK(xideEditTransactionTicketDestroy(f.ticket)==XUI_OK);f.ticket=NULL;CHECK(state(&f).phase==XIDE_MIX_UI_REJECTED);w=job(&f,3);CHECK(execute(&w)&&state(&f).rolledBack);closedDisk(&f,0);clear(&f);
 /* Acquisition conflict on second member unwinds the first byte lease. */
 init(&f,argv[1],serial++,10);captured(&f);HANDLE h=readHandle(f.paths[2]);CHECK(h!=INVALID_HANDLE_VALUE);OVERLAPPED at={0};CHECK(LockFileEx(h,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,MAXDWORD,MAXDWORD,&at));w=job(&f,1);CHECK(!execute(&w)&&state(&f).phase==XIDE_MIX_FAILED);disk(f.paths[0],raw[0]);CHECK(UnlockFileEx(h,0,MAXDWORD,MAXDWORD,&at)&&CloseHandle(h));clear(&f);
 /* Context destruction before Stage cancels the detached work. */
 init(&f,argv[1],serial++,10);captured(&f);CHECK(xideEditTransactionTicketDestroy(f.ticket)==XUI_OK);f.ticket=NULL;w=job(&f,1);CHECK(!execute(&w)&&state(&f).phase==XIDE_MIX_FAILED);closedDisk(&f,0);clear(&f);
 /* A failed journal acknowledgement before the first source mutation freezes
  * the transaction. It is never mistaken for permission to apply or rollback. */
 init(&f,argv[1],serial++,10);staged(&f);CHECK(xideEditTransactionArm(f.ticket,hostAccept,&f)==XUI_OK);failJournalFlush=1;w=job(&f,2);CHECK(!execute(&w)&&state(&f).needsRecovery&&!state(&f).committed&&!state(&f).applied);char* untouched=xidePathResolveLexical(NULL,f.paths[0]);CHECK(untouched);clear(&f);disk(untouched,raw[0]);free(untouched);
 size_t phaseGates[3]={0};
 for(int step=0;step<3;++step){
  init(&f,argv[1],serial++,10);if(step==2)captured(&f);else if(step==1){w=job(&f,0);CHECK(execute(&w));f.work=w.work;}
  allocations=failAllocation=0;probing=1;
  int okay;if(step==0){w=job(&f,0);okay=execute(&w);f.work=w.work;}else if(step==1)okay=xideEditTransactionCapture(&f.store,f.work,&f.ticket)==XUI_OK;else{w=job(&f,1);okay=execute(&w);}
  probing=0;CHECK(okay);phaseGates[step]=allocations;CHECK(phaseGates[step]>0);clear(&f);
  for(size_t atFault=1;atFault<=phaseGates[step];++atFault){
   init(&f,argv[1],serial++,10);if(step==2)captured(&f);else if(step==1){w=job(&f,0);CHECK(execute(&w));f.work=w.work;}
   allocations=0;failAllocation=atFault;probing=1;
   if(step==0){w=job(&f,0);okay=execute(&w);f.work=w.work;}else if(step==1)okay=xideEditTransactionCapture(&f.store,f.work,&f.ticket)==XUI_OK;else{w=job(&f,1);okay=execute(&w);}
   probing=0;CHECK(allocations>=atFault&&!okay);char* saved[2]={xidePathResolveLexical(NULL,f.paths[0]),xidePathResolveLexical(NULL,f.paths[2])};CHECK(saved[0]&&saved[1]);clear(&f);disk(saved[0],raw[0]);disk(saved[1],raw[2]);free(saved[0]);free(saved[1]);
  }
 }
 printf("workspace_edit_transaction_test passed: %u checks; cold IAT allocations %zu/%zu/%zu; actual worker prepare/stage/apply/finish, mixed C/xlang publication, closed-only/open-only, byte leases, partial I/O, reverse restore, cancellation, stale UI, reentrancy, cached target, closure, frozen journal and failed lease release\n",checks,phaseGates[0],phaseGates[1],phaseGates[2]);return 0;
}
