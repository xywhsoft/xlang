#ifndef FIXTURE_XGE3_AUDIO_RESOURCES_H
#define FIXTURE_XGE3_AUDIO_RESOURCES_H
#include <xge3_runtime.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Only present in the private qualification SDK, never the shipping engine. */
XGE_API int fixtureXgeAudioBegin(void);
XGE_API int fixtureXgeAudioMix(void);
XGE_API uint64_t fixtureXgeAudioOwners(void);
#ifdef __cplusplus
}
#endif
static const char* fixtureXgeAudioPath(void)
{ const char* path=getenv("XLANG_XGE_AUDIO_FIXTURE");return path ? path:""; }
static void fixtureXgeAudioHeap(size_t* count,size_t* bytes)
{ xmemdebugsnapshot s;memset(&s,0,sizeof(s));xrtMemDebugSnapshot(&s);*count=s.LiveCount;*bytes=s.LiveBytes; }
static int fixtureXgeAudioNativeCosts(void)
{
 size_t beforeCount,beforeBytes,afterCount,afterBytes;
 fixtureXgeAudioHeap(&beforeCount,&beforeBytes);
 xge_audio_group group=(xge_audio_group)xrtCalloc(1,sizeof(*group));
 if(!group)return XGE_ERROR_OUT_OF_MEMORY;
 int status=xgeAudioGroupInit(group);
 if(status){xrtFree(group);return status;}
 fixtureXgeAudioHeap(&afterCount,&afterBytes);
 printf("Native AudioGroup: %llu allocations / %llu bytes\n",
  (unsigned long long)(afterCount-beforeCount),(unsigned long long)(afterBytes-beforeBytes));
 status=xgeAudioGroupFreeChecked(group);
 if(!status)xrtFree(group);
 return status;
}
#endif
