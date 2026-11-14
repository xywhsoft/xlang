#include "foreign_xui_code_tokens.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
static xmemdebugsnapshot prefix,hot;
static const xui_code_token_t tokens[2]={{0,3,1,2,0},{4,8,1,2,0}};
void fixtureTokenBudget(int value)
{if(value<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(value));}}
void fixtureTokenPrefixBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&prefix);}
void fixtureTokenPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureTokenHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 if(after.LiveCount!=prefix.LiveCount||after.LiveBytes!=prefix.LiveBytes)
  fprintf(stderr,"Token live memory changed: %zu/%zu -> %zu/%zu\n",prefix.LiveCount,prefix.LiveBytes,after.LiveCount,after.LiveBytes);
 assert(after.LiveCount==prefix.LiveCount&&after.LiveBytes==prefix.LiveBytes&&
  after.InvalidFreeCount==prefix.InvalidFreeCount&&after.DoubleFreeCount==prefix.DoubleFreeCount);
}
void fixtureTokenHotBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&hot);}
void fixtureTokenHotReport(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("CodeToken heap kind=%d alloc=%llu free=%llu realloc=%llu\n",kind,
  (unsigned long long)(after.AllocCount-hot.AllocCount),(unsigned long long)(after.FreeCount-hot.FreeCount),
  (unsigned long long)(after.ReallocCount-hot.ReallocCount));
}
void fixtureTokenCheck(xui_code_token_buffer buffer)
{
 xui_code_token_t output[2];int count=-1;
 assert(xuiCodeTokenBufferGetTokens(buffer,42,output,2,&count)==XUI_OK&&count==2);
 assert(!memcmp(tokens,output,sizeof(tokens)));
}
void fixtureTokenNativeHot(xui_code_token_buffer buffer)
{
 for(int i=0;i<1000;++i){
  uint32_t version;int start,end;
  assert(xuiCodeTokenBufferGetCount(buffer)==2);
  assert(xuiCodeTokenBufferGetVersion(buffer,&version)==XUI_OK&&version==42);
  assert(xuiCodeTokenBufferGetRange(buffer,&version,&start,&end)==XUI_OK&&version==42&&start==0&&end==INT_MAX);
 }
}
void fixtureTokenNativeCreate(void)
{xui_code_token_buffer buffer=NULL;assert(xuiCodeTokenBufferCreate(&buffer)==XUI_OK);xuiCodeTokenBufferDestroy(buffer);}
void fixtureTokenNativeSet(xui_code_token_buffer buffer)
{assert(xuiCodeTokenBufferSet(buffer,tokens,2,42)==XUI_OK);}
int fixtureTokenDuringCall(xui_code_token_buffer buffer,void (*callback)(void*),void* user)
{callback(user);return xuiCodeTokenBufferGetCount(buffer)==2 ? XUI_OK:XUI_ERROR_INVALID_STATE;}
