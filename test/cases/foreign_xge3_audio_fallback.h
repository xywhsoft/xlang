#ifndef FIXTURE_XGE3_AUDIO_FALLBACK_H
#define FIXTURE_XGE3_AUDIO_FALLBACK_H
#include "foreign_xge3_audio_resources.h"
static bool fixtureFallbackUsed(xge_sound sound)
{return sound && (sound->iFlags&XGE_SOUND_FALLBACK)!=0;}
static int fixtureFallbackNativePathCosts(void)
{
 size_t a,b,c,d;fixtureXgeAudioHeap(&a,&b);int status=xgeSoundFallbackSet(fixtureXgeAudioPath());
 if(status)return status;
 fixtureXgeAudioHeap(&c,&d);printf("Native fallback path: %llu allocations / %llu bytes\n",
  (unsigned long long)(c-a),(unsigned long long)(d-b));xgeSoundFallbackClear();return XGE_OK;
}
static int fixtureFallbackNativeVoiceCosts(void)
{
 size_t a,b,c,d;fixtureXgeAudioHeap(&a,&b);xge_sound sound=NULL;int status=xgeSoundFallbackCreateOwned(&sound);
 if(status)return status;
 fixtureXgeAudioHeap(&c,&d);printf("Native fallback voice: %llu allocations / %llu bytes\n",
  (unsigned long long)(c-a),(unsigned long long)(d-b));return xgeSoundFreeChecked(sound);
}
#endif
