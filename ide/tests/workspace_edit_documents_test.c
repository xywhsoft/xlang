#include "workspace_edit_fixture.h"
#include "editor/workspace_edit_documents.h"
#include "platform/path.h"
#include <windows.h>
#undef CHECK
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"open-edit:%d: %s\n",__LINE__,#e);exit(1);}}while(0)
static int probing;static size_t calls,failAt;
extern void* (*__real___imp_xrtMalloc)(size_t);
extern void* (*__real___imp_xrtCalloc)(size_t,size_t);
static void* probeMalloc(size_t size){if(probing && ++calls==failAt)return NULL;return __real___imp_xrtMalloc(size);}
static void* probeCalloc(size_t n,size_t size){if(probing && ++calls==failAt)return NULL;return __real___imp_xrtCalloc(n,size);}
void* (*__wrap___imp_xrtMalloc)(size_t)=probeMalloc;
void* (*__wrap___imp_xrtCalloc)(size_t,size_t)=probeCalloc;
static const char *before[2]={"abcDEFghi\n","uvwXYrst\n"},*after[2]={"abcXYghi\n","uvwXYZrst\n"};
static const char* requestBeforeOverride;
typedef struct fixture {
 xui_context context;xide_document_store store;xide_document* docs[2];char *paths[2],*uris[2];
 xide_workspace_edit* plan;xide_open_edit_ticket* ticket;xide_open_edit_work* work;
 uint64_t revisions[2];int inspect,notices,guardCalls,mode;int closeStatus;bool closeConsumed;
} fixture;
static void caret(xide_document* d,int anchor,int point)
{xui_code_selection_t s={.iAnchorOffset=anchor,.iCaretOffset=point,.iPreferredColumn=2};CHECK(xuiCodeSelectionSetState(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),&s)==XUI_OK);}
static void notice(xide_document* d,void* user)
{
 fixture* f=user;(void)d;if(!f->inspect)return;++f->notices;
 for(unsigned i=0;i<2;++i){CHECK(f->docs[i]->revision==f->revisions[i]+1);CHECK(!strcmp(xuiCodeEditGetText(f->docs[i]->editor),after[i]));CHECK(f->docs[i]->dirty==(i==0) && xuiCodeDocumentGetDirty(xuiCodeEditGetDocument(f->docs[i]->editor))==(i==0));}
 if(f->notices==1){
  if(f->mode==1)caret(f->docs[1],1,1);
  if(f->mode==2){xide_model_error error={0};char target[4096];CHECK(snprintf(target,sizeof(target),"%s.saved.xl",f->paths[1])<(int)sizeof(target));CHECK(xideDocumentSave(f->docs[1],target,0,&error));}
  if(f->mode==3)f->closeStatus=xuiDestroyOwnerChecked(f->context,&f->closeConsumed);
  if(f->mode==4){xide_model_error error={0};CHECK(xideDocumentSetLanguage(f->docs[1],"c",&error));}
 }
}
static void prepare(fixture* f,int secondClosed,int secondUnversioned)
{
 xvalue *root=xrtValueObject(),*changes=xrtValueArray();CHECK(root && changes);xide_workspace_edit_input inputs[2];
 for(unsigned i=0;i<2;++i){const char* body=!i && requestBeforeOverride?requestBeforeOverride:before[i];xls_text* text=xlsTextCreate(body,strlen(body),1024,NULL);CHECK(text);inputs[i]=(xide_workspace_edit_input){f->uris[i],text,f->docs[i]->id,f->docs[i]->revision,!secondUnversioned||!i,9+(int)i};xvalue* edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,3,i?5:6,i?"XYZ":"XY")) && xrtValueArrayAppendNew(changes,documentEdit(f->uris[i],inputs[i].hasVersion,inputs[i].version,edits)));}
 CHECK(putEdit(root,"documentChanges",changes));enum xide_workspace_edit_error error;f->plan=xideWorkspaceEditPrepare(root,inputs,2,0,0,NULL,NULL,&error);CHECK(f->plan && !error);for(unsigned i=0;i<2;++i)xlsTextRelease(inputs[i].text);xrtValueRelease(root);
 if(secondClosed){xide_model_error modelError={0};CHECK(xideDocumentClose(f->docs[1],1,&modelError));}
}
static void init(fixture* f,const char* directory,unsigned serial,int closed,int unversioned)
{
 memset(f,0,sizeof(*f));CHECK(xuiCreate(&f->context)==XUI_OK);xideDocumentsInit(&f->store,f->context,NULL,notice,f);xide_model_error error={0};
 for(unsigned i=0;i<2;++i){char name[64];snprintf(name,sizeof(name),"open-edit-%u-%u.%s",serial,i,i?"xl":"c");f->paths[i]=xidePathResolveLexical(directory,name);CHECK(f->paths[i]);const char* saved=i?after[i]:before[i];CHECK(xrtFileWriteAtomic(f->paths[i],(xbytesview){(const unsigned char*)saved,strlen(saved)}));f->docs[i]=xideDocumentOpen(&f->store,f->paths[i],&error);CHECK(f->docs[i]);f->uris[i]=xideLspPathToUri(f->docs[i]->path,&error);CHECK(f->uris[i]);if(i)CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(f->docs[i]->editor),0,(int)strlen(saved),before[i])==XUI_OK);f->revisions[i]=f->docs[i]->revision;caret(f->docs[i],i?5:7,i?5:4);}
 xui_code_selection_t extra={.iAnchorOffset=0,.iCaretOffset=0,.iPreferredColumn=3};CHECK(xuiCodeSelectionAdd(xuiCodeEditGetSelection(f->docs[0]->editor),&extra)==XUI_OK);prepare(f,closed,unversioned);
}
static void unchanged(fixture* f)
{for(unsigned i=0;i<2;++i)if(f->docs[i]->open){CHECK(f->docs[i]->revision==f->revisions[i]);CHECK(!strcmp(xuiCodeEditGetText(f->docs[i]->editor),before[i]));CHECK(!f->docs[i]->preparedChange);}CHECK(!f->notices);}
static void clear(fixture* f)
{
 CHECK(xideOpenEditTicketDestroy(f->ticket)==XUI_OK);f->ticket=NULL;xideOpenEditWorkDestroy(f->work);f->work=NULL;xideWorkspaceEditRelease(f->plan);f->plan=NULL;
 xideDocumentsClear(&f->store);if(f->context){bool consumed=false;CHECK(xuiDestroyOwnerChecked(f->context,&consumed)==XUI_OK && consumed);}
 for(unsigned i=0;i<2;++i){free(f->paths[i]);free(f->uris[i]);}xrtClearError();
}
typedef struct worker {xide_open_edit_work* work;int status;DWORD thread;int destroy;HANDLE ready,go;} worker;
static DWORD WINAPI stage(void* user)
{worker* w=user;w->thread=GetCurrentThreadId();if(w->ready){SetEvent(w->ready);if(WaitForSingleObject(w->go,30000)!=WAIT_OBJECT_0)return 1;}w->status=xideOpenEditStage(w->work,NULL,NULL);if(w->destroy)xideOpenEditWorkDestroy(w->work);return 0;}
static void staged(fixture* f)
{CHECK(xideOpenEditCapture(&f->store,f->plan,0,&f->ticket,&f->work)==XUI_OK);worker w={.work=f->work};HANDLE thread=CreateThread(NULL,0,stage,&w,0,NULL);CHECK(thread && WaitForSingleObject(thread,30000)==WAIT_OBJECT_0 && w.thread!=GetCurrentThreadId() && w.status==XUI_OK);CloseHandle(thread);CHECK(xideOpenEditStage(f->work,NULL,NULL)!=XUI_OK);}
static int guard(void* user){fixture* f=user;++f->guardCalls;return XUI_OK;}
static int reject(void* user){fixture* f=user;++f->guardCalls;return XUI_ERROR_INVALID_STATE;}
static int movingGuard(void* user){fixture* f=user;++f->guardCalls;caret(f->docs[1],0,0);CHECK(xideOpenEditTicketDestroy(f->ticket)==XUI_ERROR_BUSY);return XUI_OK;}
static int savingGuard(void* user){fixture* f=user;++f->guardCalls;xide_model_error e={0};CHECK(xideDocumentSave(f->docs[1],NULL,0,&e));return XUI_OK;}
static DWORD WINAPI wrongOwner(void* user){fixture* f=user;CHECK(xideOpenEditTicketDestroy(f->ticket)==XUI_ERROR_BUSY);xide_open_edit_outcome out;CHECK(xideOpenEditCommit(f->ticket,f->work,guard,f,&out)==XUI_ERROR_BUSY && !out.committed);return 0;}
static void committed(fixture* f,int mode)
{
 staged(f);HANDLE other=CreateThread(NULL,0,wrongOwner,f,0,NULL);CHECK(other && WaitForSingleObject(other,30000)==WAIT_OBJECT_0);CloseHandle(other);f->mode=mode;f->inspect=1;xide_open_edit_outcome out;CHECK(xideOpenEditCommit(f->ticket,f->work,guard,f,&out)==XUI_OK && out.committed && (mode==3 || !out.readingStatus) && f->guardCalls==1);
 CHECK(f->notices==(mode==3?1:mode==2||mode==4?3:2) || (fprintf(stderr,"callback mode%d notices%d\n",mode,f->notices),0));for(unsigned i=0;i<2;++i)CHECK(!f->docs[i]->preparedChange);
 if(mode==3){CHECK(f->closeStatus==XUI_STATUS_DEFERRED && !f->closeConsumed);CHECK(out.readingRestored+out.readingSkipped==2);}
 else{
  CHECK(out.readingRestored==(mode?1:2) && out.readingSkipped==(mode?1:0));xui_code_selection_t s;CHECK(xuiCodeSelectionGetAt(xuiCodeEditGetSelection(f->docs[0]->editor),0,&s)==XUI_OK && s.iAnchorOffset==6 && s.iCaretOffset==3 && s.iPreferredColumn==2 && (s.iFlags&XUI_CODE_SELECTION_REVERSED));CHECK(xuiCodeSelectionGetAt(xuiCodeEditGetSelection(f->docs[0]->editor),1,&s)==XUI_OK && !s.iAnchorOffset && !s.iCaretOffset && s.iPreferredColumn==3 && (s.iFlags&XUI_CODE_SELECTION_INACTIVE));CHECK(xuiCodeSelectionGetAt(xuiCodeEditGetSelection(f->docs[1]->editor),0,&s)==XUI_OK && s.iCaretOffset==(mode==1?1:mode?5:6));
 }
 CHECK(xideOpenEditCommit(f->ticket,f->work,guard,f,&out)!=XUI_OK && !out.committed && f->guardCalls==1);f->inspect=0;
 if(!mode){uint64_t revision=f->docs[0]->revision;CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(f->docs[0]->editor))==XUI_OK && !f->docs[0]->dirty && f->docs[0]->revision==revision+1 && !strcmp(xuiCodeEditGetText(f->docs[0]->editor),before[0]));CHECK(xuiCodeDocumentRedo(xuiCodeEditGetDocument(f->docs[0]->editor))==XUI_OK && f->docs[0]->dirty && f->docs[0]->revision==revision+2);}
}
typedef struct stop {size_t calls,at;} stop;
static int cancelled(void* user){stop* s=user;return ++s->calls==s->at;}
int main(int argc,char** argv)
{
 CHECK(argc==2 && xrtDirCreateAll(argv[1]));unsigned serial=0;fixture f;xide_open_edit_outcome out;
 for(int mode=0;mode<5;++mode){init(&f,argv[1],serial++,0,0);committed(&f,mode);clear(&f);}
 init(&f,argv[1],serial++,0,0);staged(&f);xide_document* swapped=f.store.items[0];f.store.items[0]=f.store.items[1];f.store.items[1]=swapped;f.inspect=1;CHECK(xideOpenEditCommit(f.ticket,f.work,NULL,&f,&out)==XUI_ERROR_INVALID_ARGUMENT && !out.committed);CHECK(xideOpenEditCommit(f.ticket,f.work,guard,&f,&out)==XUI_OK && out.committed && out.readingRestored==2 && !out.readingSkipped && f.notices==2);f.inspect=0;clear(&f);
 for(int mode=0;mode<12;++mode){init(&f,argv[1],serial++,0,0);staged(&f);int (*check)(void*)=guard;
  if(mode==0)check=reject;
  if(mode==1)check=movingGuard;
  if(mode==2)check=savingGuard;
  if(mode==3)caret(f.docs[0],0,0);
  if(mode==4){xide_model_error e={0};CHECK(xideDocumentSave(f.docs[1],NULL,0,&e));}
  if(mode==5)CHECK(xuiCodeEditSetReadonly(f.docs[1]->editor,1)==XUI_OK);
  if(mode==6)f.docs[1]->externalState=1;
  if(mode==7)++f.docs[1]->languageOverride;
  if(mode==8)++f.docs[1]->revision;
  if(mode==9){xide_model_error e={0};CHECK(xideDocumentClose(f.docs[1],1,&e) && xideDocumentOpen(&f.store,f.paths[1],&e)==f.docs[1]);}
  if(mode==10)++f.docs[1]->id;
  if(mode==11){const unsigned char raw[]="abcDEFghi\r\n";xide_text_file decoded={0};xide_model_error e={0};uint64_t revision=f.docs[0]->revision;uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(f.docs[0]->editor));CHECK(xideTextFileDecode(raw,sizeof(raw)-1,&decoded,&e) && xideDocumentAcceptExternal(f.docs[0],&decoded,0,xideDocumentsSourceEpoch((f.docs[0])->store),&e));CHECK(f.docs[0]->revision==revision && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(f.docs[0]->editor))==version && !f.docs[0]->dirty);}
  CHECK(xideOpenEditCommit(f.ticket,f.work,check,&f,&out)!=XUI_OK && !out.committed);if(mode==8)--f.docs[1]->revision;if(mode==9){CHECK(!f.notices && !strcmp(xuiCodeEditGetText(f.docs[0]->editor),before[0]));}else unchanged(&f);clear(&f);
 }
 for(int mode=0;mode<2;++mode){init(&f,argv[1],serial++,mode==0,mode==1);CHECK(xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work)!=XUI_OK && !f.ticket && !f.work);unchanged(&f);clear(&f);}
 requestBeforeOverride="abcQEFghi\n";init(&f,argv[1],serial++,0,0);requestBeforeOverride=NULL;staged(&f);CHECK(xideOpenEditCommit(f.ticket,f.work,guard,&f,&out)!=XUI_OK && !out.committed && !f.guardCalls);unchanged(&f);clear(&f);
 init(&f,argv[1],serial++,0,0);staged(&f);xideDocumentsClear(&f.store);CHECK(xideOpenEditCommit(f.ticket,f.work,guard,&f,&out)!=XUI_OK && !out.committed && !f.guardCalls);clear(&f);
 for(unsigned i=0;i<2;++i){init(&f,argv[1],serial++,0,0);f.docs[1]->revision=i?UINT64_MAX:0;CHECK(xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work)!=XUI_OK && !f.ticket && !f.work);f.docs[1]->revision=f.revisions[1];unchanged(&f);clear(&f);}
 init(&f,argv[1],serial++,0,0);xideWorkspaceEditRelease(f.plan);xvalue* empty=parseEdit("{\"documentChanges\":[]}");enum xide_workspace_edit_error emptyError;f.plan=xideWorkspaceEditPrepare(empty,NULL,0,0,0,NULL,NULL,&emptyError);xrtValueRelease(empty);CHECK(f.plan && !emptyError);staged(&f);CHECK(xideOpenEditCommit(f.ticket,f.work,guard,&f,&out)==XUI_OK && out.committed && !out.readingRestored && !out.readingSkipped && f.guardCalls==1);unchanged(&f);clear(&f);
 /* Combined capture+worker quota, including retained plan and baseline storage.
  * Find the actual admission boundary without duplicating private struct sizes. */
 init(&f,argv[1],serial++,0,0);size_t low=1,high=1024u*1024u;
 while(low<high){size_t budget=low+(high-low)/2;int result=xideOpenEditCapture(&f.store,f.plan,budget,&f.ticket,&f.work);if(result==XUI_OK)result=xideOpenEditStage(f.work,NULL,NULL);CHECK(result==XUI_OK || result==XUI_ERROR_OUT_OF_MEMORY);CHECK(xideOpenEditTicketDestroy(f.ticket)==XUI_OK);f.ticket=NULL;xideOpenEditWorkDestroy(f.work);f.work=NULL;xrtClearError();if(result==XUI_OK)high=budget;else low=budget+1;}
 CHECK(low>1 && xideOpenEditCapture(&f.store,f.plan,low,&f.ticket,&f.work)==XUI_OK && xideOpenEditStage(f.work,NULL,NULL)==XUI_OK);unchanged(&f);clear(&f);
 init(&f,argv[1],serial++,0,0);calls=failAt=0;probing=1;int status=xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work);probing=0;CHECK(status==XUI_OK && calls>5);size_t captureCalls=calls;clear(&f);
 for(size_t i=1;i<=captureCalls;++i){init(&f,argv[1],serial++,0,0);calls=0;failAt=i;probing=1;status=xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work);probing=0;CHECK(status!=XUI_OK && !f.ticket && !f.work && calls>=i);unchanged(&f);clear(&f);}
 init(&f,argv[1],serial++,0,0);CHECK(xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work)==XUI_OK);calls=failAt=0;probing=1;status=xideOpenEditStage(f.work,NULL,NULL);probing=0;CHECK(status==XUI_OK && calls==3);size_t stageCalls=calls;clear(&f);
 for(size_t i=1;i<=stageCalls;++i){init(&f,argv[1],serial++,0,0);CHECK(xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work)==XUI_OK);calls=0;failAt=i;probing=1;status=xideOpenEditStage(f.work,NULL,NULL);probing=0;CHECK(status!=XUI_OK && calls==i);CHECK(xideOpenEditCommit(f.ticket,f.work,guard,&f,&out)!=XUI_OK && !out.committed);unchanged(&f);clear(&f);}
 init(&f,argv[1],serial++,0,0);CHECK(xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work)==XUI_OK);stop observed={0,SIZE_MAX};CHECK(xideOpenEditStage(f.work,cancelled,&observed)==XUI_OK && observed.calls>5);clear(&f);
 for(size_t i=1;i<=observed.calls;++i){init(&f,argv[1],serial++,0,0);CHECK(xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work)==XUI_OK);stop s={0,i};CHECK(xideOpenEditStage(f.work,cancelled,&s)!=XUI_OK && s.calls==i);unchanged(&f);clear(&f);}
 /* Worker has only detached data. Dispose its owner ticket and GUI before Stage. */
 init(&f,argv[1],serial++,0,0);CHECK(xideOpenEditCapture(&f.store,f.plan,0,&f.ticket,&f.work)==XUI_OK);worker w={.work=f.work,.destroy=1,.ready=CreateEventW(NULL,TRUE,FALSE,NULL),.go=CreateEventW(NULL,TRUE,FALSE,NULL)};HANDLE thread=CreateThread(NULL,0,stage,&w,0,NULL);CHECK(w.ready && w.go && thread && WaitForSingleObject(w.ready,30000)==WAIT_OBJECT_0);CHECK(xideOpenEditTicketDestroy(f.ticket)==XUI_OK);f.ticket=NULL;xideWorkspaceEditRelease(f.plan);f.plan=NULL;xideDocumentsClear(&f.store);bool consumed=false;CHECK(xuiDestroyOwnerChecked(f.context,&consumed)==XUI_OK && consumed);f.context=NULL;SetEvent(w.go);CHECK(WaitForSingleObject(thread,30000)==WAIT_OBJECT_0 && w.status==XUI_OK);CloseHandle(thread);CloseHandle(w.ready);CloseHandle(w.go);f.work=NULL;clear(&f);
 printf("workspace_edit_documents_test passed: %u checks; %zu cold capture faults, %zu map faults, %zu cancellation points; real worker/GUI publication, all-or-none fences, mapped reading, callback changes, owner close and GUI-free retirement\n",checks,captureCalls,stageCalls,observed.calls);return 0;
}
