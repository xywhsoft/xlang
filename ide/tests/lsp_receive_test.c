#define _WIN32_WINNT 0x0601
#include "protocol/lsp_wire.h"
#include "services/language_client.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise the real receive queue, with allocation failure local to this
   translation unit. The framing parser and SDK allocator are not replaced. */
static int failNode;
static void* localCalloc(size_t count,size_t size) { return failNode ? NULL : calloc(count,size); }
#define calloc localCalloc
#include "../src/services/language_client.c"
#undef calloc
static int checks;
#define CHECK(e) do {++checks;if(!(e)){fprintf(stderr,"lsp_receive_test:%d %s\n",__LINE__,#e);return 0;}}while(0)
static int textIs(const xvalue* value,const char* expected)
{ xstrview s={0};return xrtValueGetString(value,&s) && s.Size==strlen(expected) && !memcmp(s.Data,expected,s.Size); }
static const xvalue* member(const xvalue* value,const char* name)
{ return xrtValueObjectGet(value,xrtStrView(name)); }
static char* rawFrame(const char* body,size_t length,size_t* size)
{
 char header[80];int n=snprintf(header,sizeof(header),"Content-Length: %zu\r\n\r\n",length);
 char* frame=malloc((size_t)n+length);if(frame){memcpy(frame,header,(size_t)n);memcpy(frame+n,body,length);*size=(size_t)n+length;}return frame;
}
typedef struct lifetime {CRITICAL_SECTION* lock;int drops,unlocked;} lifetime;
static DWORD WINAPI tryLock(void* user)
{
 lifetime* life=user;int okay=TryEnterCriticalSection(life->lock);
 if(okay) LeaveCriticalSection(life->lock);
 return (DWORD)okay;
}
static void dropped(ptr handle,ptr user)
{
 lifetime* life=user;if(handle!=life) return;++life->drops;
 if(life->lock) {
  HANDLE thread=CreateThread(NULL,0,tryLock,life,0,NULL);DWORD result=0;
  if(thread){if(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0)GetExitCodeThread(thread,&result);CloseHandle(thread);}
  life->unlocked=(int)result;
 }
}
static int attach(xvalue* root,lifetime* life)
{
 static const xvaluehandleops ops={.Drop=dropped};ptr value=life;
 return xrtValueObjectSetNew(root,xrtStrView("testLifetime"),xrtValueHandleTake(&value,&ops,life));
}
typedef struct sink {xide_language_client* client;xvalue* seen;size_t bytes;int calls,reject;lifetime* life;} sink;
static int queued(xvalue* root,size_t bytes,void* user)
{
 sink* s=user;s->seen=root;s->bytes=bytes;++s->calls;
 if(s->life && !attach(root,s->life)) return 0;
 return s->reject ? 0 : receive(root,bytes,s->client);
}
static void initClient(xide_language_client* c)
{ memset(c,0,sizeof(*c));InitializeCriticalSection(&c->lock); }
static void unitClient(xide_language_client* c)
{ clear(c->sendHead);clear(c->receiveHead);if(c->changedReady)xrtCondUnit(&c->changed);if(c->changedLockReady)xrtMutexUnit(&c->changedLock);DeleteCriticalSection(&c->lock); }
static int transportSignals(void)
{
 xide_language_client c;initClient(&c);c.changedLockReady=xrtMutexInit(&c.changedLock);c.changedReady=c.changedLockReady && xrtCondInit(&c.changed);CHECK(c.changedReady);
 xvalue* message=xrtValueObject();CHECK(message && xideLanguageClientSend(&c,message,NULL));
 CHECK(waitTransport(&c,0)==XWAIT_OK && waitTransport(&c,0)==XWAIT_TIMEOUT);
 xide_language_batch* batch=xideLanguageBatchCreate();CHECK(batch && xideLanguageBatchAppend(batch,"test/changed",message) && xideLanguageBatchSend(&c,batch));
 CHECK(waitTransport(&c,0)==XWAIT_OK && waitTransport(&c,0)==XWAIT_TIMEOUT);
 batch=xideLanguageBatchCreate();CHECK(batch && xideLanguageBatchSend(&c,batch) && waitTransport(&c,0)==XWAIT_TIMEOUT);
 c.state=XIDE_LS_FAILED;CHECK(!xideLanguageClientSend(&c,message,NULL) && waitTransport(&c,0)==XWAIT_TIMEOUT);
 batch=xideLanguageBatchCreate();CHECK(batch && xideLanguageBatchAppend(batch,"test/changed",message) && !xideLanguageBatchSend(&c,batch) && waitTransport(&c,0)==XWAIT_TIMEOUT);xideLanguageBatchDestroy(batch);
 signalTransport(&c);signalTransport(&c);CHECK(waitTransport(&c,0)==XWAIT_OK && waitTransport(&c,0)==XWAIT_TIMEOUT);
 xrtValueRelease(message);unitClient(&c);return 1;
}
static const char source[]=" \n { \"jsonrpc\" : \"2.0\", \"result\" : {\"name\":\"中文😀\",\"nested\":[true,null,7]} } \t ";
static int fragmentation(void)
{
 size_t length=0;char* frame=rawFrame(source,strlen(source),&length);CHECK(frame);
 for(size_t split=0;split<=length;++split) {
  xide_language_client c;xide_lsp_stream stream={0};xide_model_error error={0};initClient(&c);
  lifetime life={0};sink s={.client=&c,.life=&life};
  CHECK(xideLspStreamFeedOwned(&stream,frame,split,queued,&s,&error));
  CHECK(xideLspStreamFeedOwned(&stream,frame+split,length-split,queued,&s,&error));
  CHECK(s.calls==1 && s.bytes==strlen(source) && c.receiveCount==1 && c.receiveBytes==strlen(source));
  CHECK(c.receiveHead->value==s.seen && c.receiveHead->length==strlen(source) && !life.drops);
  /* Parsed strings must not alias an input frame or reusable stream buffer. */
  memset(stream.bytes,0xcc,stream.capacity);xideLspStreamClear(&stream);
  xvalue* result=xideLanguageClientNext(&c);
  CHECK(result==s.seen && !life.drops && !c.receiveCount && !c.receiveBytes && !xideLanguageClientHasPending(&c));
  CHECK(textIs(member(member(result,"result"),"name"),"中文😀") && xrtValueCount(member(member(result,"result"),"nested"))==3);
  xrtValueRelease(result);CHECK(life.drops==1);unitClient(&c);
 }
 free(frame);return 1;
}
typedef struct retained {xvalue* roots[8];size_t lengths[8];int count;} retained;
static int keepOwned(xvalue* root,size_t length,void* user)
{ retained* r=user;if(r->count==8) return 0;r->roots[r->count]=root;r->lengths[r->count++]=length;return 1; }
static int keepBorrowed(const xvalue* root,void* user)
{ retained* r=user;if(r->count==8) return 0;r->roots[r->count++]=xrtValueRetain((xvalue*)root);return 1; }
static int dropOwned(xvalue* root,size_t length,void* user)
{ if(!length || !attach(root,user)) return 0;xrtValueRelease(root);return 1; }
static int frames(void)
{
 size_t n=0;char* frame=rawFrame(source,strlen(source),&n);CHECK(frame);
 char* joined=malloc(n*3);CHECK(joined);for(int i=0;i<3;++i)memcpy(joined+n*i,frame,n);
 retained r={0};xide_lsp_stream stream={0};xide_model_error error={0};
 CHECK(xideLspStreamFeedOwned(&stream,joined,n*3,keepOwned,&r,&error) && r.count==3 && xideLspStreamEnd(&stream,&error));
 memset(joined,0xcc,n*3);xideLspStreamClear(&stream);
 for(int i=0;i<3;++i){CHECK(r.lengths[i]==strlen(source) && textIs(member(member(r.roots[i],"result"),"name"),"中文😀"));xrtValueRelease(r.roots[i]);}memset(&r,0,sizeof(r));
 CHECK(xideLspStreamFeed(&stream,frame,n,keepBorrowed,&r,&error) && r.count==1 && xideLspStreamEnd(&stream,&error));
 xideLspStreamClear(&stream);CHECK(textIs(member(member(r.roots[0],"result"),"name"),"中文😀"));xrtValueRelease(r.roots[0]);memset(&r,0,sizeof(r));
 memcpy(joined,frame,n);const char bad[]="Content-Length: 2\r\n\r\n[]";memcpy(joined+n,bad,sizeof(bad)-1);
 CHECK(!xideLspStreamFeedOwned(&stream,joined,n+sizeof(bad)-1,keepOwned,&r,&error) && r.count==1 && stream.failed && !xideLspStreamEnd(&stream,&error));
 xideLspStreamClear(&stream);CHECK(textIs(member(member(r.roots[0],"result"),"name"),"中文😀"));xrtValueRelease(r.roots[0]);memset(&r,0,sizeof(r));
 for(size_t i=0;i<n;++i)CHECK(xideLspStreamFeedOwned(&stream,frame+i,1,keepOwned,&r,&error));
 CHECK(r.count==1 && r.lengths[0]==strlen(source) && xideLspStreamEnd(&stream,&error));xideLspStreamClear(&stream);xrtValueRelease(r.roots[0]);
 lifetime life={0};CHECK(xideLspStreamFeedOwned(&stream,frame,n,dropOwned,&life,&error) && life.drops==1 && xideLspStreamEnd(&stream,&error));
 xideLspStreamClear(&stream);CHECK(life.drops==1);free(joined);free(frame);return 1;
}
static int rejection(void)
{
 size_t n=0;char* frame=rawFrame(source,strlen(source),&n);CHECK(frame);
 for(int mode=0;mode<4;++mode) {
  xide_language_client c;initClient(&c);xide_lsp_stream stream={0};xide_model_error error={0};
  lifetime life={.lock=&c.lock};sink s={.client=&c,.life=&life,.reject=mode==0};
  if(mode==1)c.receiveCount=QUEUE_COUNT;
  if(mode==2)c.receiveBytes=QUEUE_BYTES-strlen(source)+1;
  if(mode==3)failNode=1;
  CHECK(!xideLspStreamFeedOwned(&stream,frame,n,queued,&s,&error));failNode=0;
  CHECK(s.calls==1 && life.drops==1 && life.unlocked && !c.receiveHead && !c.receiveTail && stream.failed);
  CHECK(!xideLspStreamFeedOwned(&stream,frame,n,queued,&s,&error) && s.calls==1 && !xideLspStreamEnd(&stream,&error));
  c.receiveCount=c.receiveBytes=0;xideLspStreamClear(&stream);s.life=NULL;s.reject=0;
  CHECK(xideLspStreamFeedOwned(&stream,frame,n,queued,&s,&error) && s.calls==2);
  xvalue* root=xideLanguageClientNext(&c);CHECK(root==s.seen);xrtValueRelease(root);xideLspStreamClear(&stream);unitClient(&c);
 }
 free(frame);return 1;
}
static int bounds(void)
{
 xide_language_client c;initClient(&c);xide_lsp_stream stream={0};xide_model_error error={0};sink s={.client=&c};
 const char small[]="Content-Length: 2\r\n\r\n{}";
 for(size_t i=0;i<QUEUE_COUNT;++i)CHECK(xideLspStreamFeedOwned(&stream,small,sizeof(small)-1,queued,&s,&error));
 CHECK(c.receiveCount==QUEUE_COUNT && c.receiveBytes==QUEUE_COUNT*2 && !xideLspStreamFeedOwned(&stream,small,sizeof(small)-1,queued,&s,&error));
 CHECK(c.receiveCount==QUEUE_COUNT && c.receiveBytes==QUEUE_COUNT*2);unitClient(&c);xideLspStreamClear(&stream);
 initClient(&c);s=(sink){.client=&c};size_t size=8u*1024u*1024u,n=0;char* body=malloc(size);CHECK(body);memset(body,' ',size);body[0]='{';body[1]='}';char* frame=rawFrame(body,size,&n);free(body);CHECK(frame);
 for(size_t i=0;i<8;++i)CHECK(xideLspStreamFeedOwned(&stream,frame,n,queued,&s,&error));
 CHECK(c.receiveCount==8 && c.receiveBytes==QUEUE_BYTES);
 CHECK(!xideLspStreamFeedOwned(&stream,small,sizeof(small)-1,queued,&s,&error) && c.receiveCount==8 && c.receiveBytes==QUEUE_BYTES);
 unitClient(&c);xideLspStreamClear(&stream);free(frame);return 1;
}
static int invalid(void)
{
 const char* bad[]={"Content-Length: 0\r\n\r\n","Content-Length: -1\r\n\r\n","Content-Length: 8388609\r\n\r\n", "Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}", "Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-16\r\n\r\n{}", "Content-Length: 2\r\n\r\n[]", "Content-Length: 4\r\n\r\nnull", "Content-Length: 2\r\n\r\n{]", "Missing: 2\r\n\r\n{}", "Content-Length: 2\n\n{}", "Content-Length: 3\r\n\r\n{}", "Content-Length: 2\r\n\r\n\xff\xff"};
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) {
  retained r={0};xide_lsp_stream stream={0};xide_model_error error={0};
  int accepted=xideLspStreamFeedOwned(&stream,bad[i],strlen(bad[i]),keepOwned,&r,&error);
  CHECK((!accepted || !xideLspStreamEnd(&stream,&error)) && !r.count);xideLspStreamClear(&stream);
 }
 retained r={0};xide_lsp_stream stream={0};xide_model_error error={0};
 CHECK(!xideLspStreamFeedOwned(&stream,NULL,1,keepOwned,&r,&error) && !r.count);
 CHECK(!xideLspStreamFeedOwned(&stream,"",0,NULL,&r,&error) && !r.count);
 CHECK(!xideLspStreamFeedOwned(&stream,"x",2u*8u*1024u*1024u+8193u,keepOwned,&r,&error) && stream.failed && !r.count);
 xideLspStreamClear(&stream);return 1;
}
int main(void)
{
 if(!transportSignals() || !fragmentation() || !frames() || !rejection() || !bounds() || !invalid()) return 1;
 printf("lsp_receive_test passed: %d checks; exact parsed-root ownership and raw wire accounting, all split points, Unicode/nested values, borrowed compatibility, coalesced/partial/invalid frames, bounded count/bytes, allocation rollback and rejected-root cleanup outside the queue lock.\n",checks);return 0;
}
