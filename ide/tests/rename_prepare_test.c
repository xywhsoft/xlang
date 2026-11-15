#include "services/rename_prepare.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"rename_prepare_test:%d %s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static HANDLE entered,released;static const xvalue *pausedLeaf,*observedRoot;
static DWORD rootReleaseThread,wakeThread;static LONG rootReleases,wakes;
static bool readString(const xvalue* v,xstrview* out)
{if(v==pausedLeaf){CHECK(SetEvent(entered)&&WaitForSingleObject(released,30000)==WAIT_OBJECT_0);}return xrtValueGetString(v,out);}
static void releaseValue(xvalue* v)
{if(v==observedRoot){rootReleaseThread=GetCurrentThreadId();InterlockedIncrement(&rootReleases);}xrtValueRelease(v);}
#define xrtValueGetString readString
#define xrtValueRelease releaseValue
#include "../src/services/rename_prepare.c"
#undef xrtValueGetString
#undef xrtValueRelease
static void wake(void* unused){(void)unused;wakeThread=GetCurrentThreadId();InterlockedIncrement(&wakes);}
static int put(xvalue* v,const char* k,xvalue* x){return x && xrtValueObjectSetNew(v,xrtStrView(k),x);}
static xvalue* point(int line,int character)
{xvalue* p=xrtValueObject();CHECK(p&&put(p,"line",xrtValueInt(line))&&put(p,"character",xrtValueInt(character)));return p;}
static xvalue* response(const char* id,const char* name)
{
 xvalue *m=xrtValueObject(),*result=xrtValueObject(),*range=xrtValueObject();CHECK(m&&result&&range);
 CHECK(put(m,"jsonrpc",xrtValueString(xrtStrView("2.0")))&&put(m,"id",xrtValueString(xrtStrView(id))));
 int units=0;for(const unsigned char* s=(const unsigned char*)name;*s;++s)if((*s&0xc0)!=0x80)units+=*s>=0xf0?2:1;
 CHECK(put(range,"start",point(2,3))&&put(range,"end",point(2,3+units))&&put(result,"range",range)&&put(result,"placeholder",xrtValueString(xrtStrView(name)))&&put(m,"result",result));return m;
}
typedef struct delivery {xide_rename_prepare* q;xvalue* m;size_t bytes;int consumed;DWORD thread;} delivery;
static DWORD WINAPI deliver(void* raw)
{delivery* d=raw;d->thread=GetCurrentThreadId();d->consumed=xideRenamePrepareReceive(d->q,d->m,d->bytes);return 0;}
static void feed(xide_rename_prepare* q,xvalue* m,size_t bytes)
{
 delivery d={q,m,bytes,0,0};observedRoot=m;rootReleases=0;rootReleaseThread=0;
 HANDLE t=CreateThread(NULL,0,deliver,&d,0,NULL);CHECK(t&&WaitForSingleObject(t,30000)==WAIT_OBJECT_0&&CloseHandle(t));
 CHECK(d.consumed&&rootReleases==1&&rootReleaseThread==d.thread&&d.thread!=GetCurrentThreadId());observedRoot=NULL;
}
int main(void)
{
 xide_rename_prepare* q=xideRenamePrepareCreate(wake,NULL);CHECK(q);char id[40],old[40];uint64_t first=xideRenamePrepareArm(q,id,sizeof(id));CHECK(first);memcpy(old,id,sizeof(id));
 xide_rename_prepared out={0};feed(q,response(id,"数量😀"),200);CHECK(xideRenamePrepareTake(q,&out)&&out.valid&&out.token==first&&out.start.line==2&&out.end.character==7&&wakeThread!=GetCurrentThreadId());
 xstrview s={0};CHECK(xrtValueGetString(out.placeholder,&s)&&s.Size==strlen("数量😀")&&!memcmp(s.Data,"数量😀",s.Size));xideRenamePreparedClear(&out);
 xide_rename_prepared span={.valid=1,.start={.line=2,.character=3},.end={.line=2,.character=7},.placeholder=xrtValueString(xrtStrView("数量😀"))};int begin=-1,end=-1;
 CHECK(xideRenamePreparedSpan(&span,(xide_lsp_position){.line=2,.character=4},10,32,&begin,&end)&&begin==7&&end==17);
 CHECK(xideRenamePreparedSpan(&span,(xide_lsp_position){.line=2,.character=7},17,32,&begin,&end)&&begin==7&&end==17);
 CHECK(!xideRenamePreparedSpan(&span,(xide_lsp_position){.line=2,.character=6},15,32,&begin,&end));
 CHECK(!xideRenamePreparedSpan(&span,(xide_lsp_position){.line=3,.character=4},10,32,&begin,&end));
 CHECK(!xideRenamePreparedSpan(&span,(xide_lsp_position){.line=2,.character=4},10,16,&begin,&end));
 span.end.character=8;CHECK(!xideRenamePreparedSpan(&span,(xide_lsp_position){.line=2,.character=4},10,32,&begin,&end));xideRenamePreparedClear(&span);
 feed(q,response(id,"duplicate"),200);CHECK(!xideRenamePrepareTake(q,&out));
 first=xideRenamePrepareArm(q,id,sizeof(id));CHECK(first>1);feed(q,response(old,"late"),200);CHECK(!xideRenamePrepareTake(q,&out));feed(q,response(id,"fresh"),200);CHECK(xideRenamePrepareTake(q,&out)&&out.valid&&out.token==first);xideRenamePreparedClear(&out);
 xideRenamePrepareCancel(q);feed(q,response(id,"cancelled"),200);CHECK(!xideRenamePrepareTake(q,&out));
 char* longName=xrtMalloc(65537);CHECK(longName);memset(longName,'a',65536);longName[65536]=0;longName[65535]=0;
 CHECK(xideRenamePrepareArm(q,id,sizeof(id)));feed(q,response(id,longName),70000);CHECK(xideRenamePrepareTake(q,&out)&&out.valid);xideRenamePreparedClear(&out);
 longName[65535]='a';CHECK(xideRenamePrepareArm(q,id,sizeof(id)));feed(q,response(id,longName),70000);CHECK(xideRenamePrepareTake(q,&out)&&!out.valid);xideRenamePreparedClear(&out);xrtFree(longName);
 for(int kind=0;kind<7;++kind){
  CHECK(xideRenamePrepareArm(q,id,sizeof(id)));xvalue* m=response(id,"name");xvalue* result=xrtValueObjectGet(m,xrtStrView("result"));xvalue* range=xrtValueObjectGet(result,xrtStrView("range"));
  if(kind==0)CHECK(put(result,"placeholder",xrtValueString((xstrview){"bad\0name",8})));
  if(kind==1)CHECK(put(result,"placeholder",xrtValueString((xstrview){"\xff",1})));
  if(kind==2)CHECK(put(range,"end",point(3,8)));
  if(kind==3)CHECK(put(range,"end",point(2,2)));
  if(kind==4)CHECK(put(range,"start",point(-1,0)));
  if(kind==5)CHECK(put(m,"extra",xrtValueNull()));
  feed(q,m,kind==6?512u*1024u+1:200);CHECK(xideRenamePrepareTake(q,&out)&&!out.valid&&!out.placeholder&&*out.error);xideRenamePreparedClear(&out);
 }
 /* A decoder already outside the mailbox lock cannot publish after an owner
    cancellation/rearm. All source-free work stays on the transport thread. */
 CHECK(xideRenamePrepareArm(q,id,sizeof(id)));xvalue* m=response(id,"old_target");pausedLeaf=xrtValueObjectGet(xrtValueObjectGet(m,xrtStrView("result")),xrtStrView("placeholder"));
 entered=CreateEventW(NULL,TRUE,FALSE,NULL);released=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(entered&&released);delivery d={q,m,200,0,0};HANDLE t=CreateThread(NULL,0,deliver,&d,0,NULL);CHECK(t&&WaitForSingleObject(entered,30000)==WAIT_OBJECT_0);
 xideRenamePrepareCancel(q);CHECK(xideRenamePrepareArm(q,id,sizeof(id))&&SetEvent(released)&&WaitForSingleObject(t,30000)==WAIT_OBJECT_0&&CloseHandle(t));pausedLeaf=NULL;CHECK(!xideRenamePrepareTake(q,&out));CHECK(CloseHandle(entered)&&CloseHandle(released));
 feed(q,response(id,"new_target"),200);CHECK(xideRenamePrepareTake(q,&out)&&out.valid);xideRenamePreparedClear(&out);
 /* Large hostile subtrees are retired before the owner takes a small error. */
 CHECK(xideRenamePrepareArm(q,id,sizeof(id)));m=response(id,"name");xvalue* many=xrtValueArray();CHECK(many);int built=1;for(int i=0;i<100000&&built;++i)built=xrtValueArrayAppendNew(many,xrtValueInt(i));CHECK(built&&xrtValueCount(many)==100000);CHECK(put(m,"huge",many));feed(q,m,1024u*1024u);CHECK(xideRenamePrepareTake(q,&out)&&!out.valid);xideRenamePreparedClear(&out);
 m=response("other-component","unclaimed");CHECK(!xideRenamePrepareReceive(q,m,200));xrtValueRelease(m);
 m=response(id,"request");CHECK(put(m,"method",xrtValueString(xrtStrView("workspace/unknown")))&&!xideRenamePrepareReceive(q,m,200));xrtValueRelease(m);
 xideRenamePrepareDestroy(q);CHECK(wakes>0);printf("rename_prepare_test passed: %u checks; typed bounded metadata,exact IDs,late/duplicate/cancel handling,concurrent rearm and off-owner hostile DOM disposal.\n",checks);return 0;
}
