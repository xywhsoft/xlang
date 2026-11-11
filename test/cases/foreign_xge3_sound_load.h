#ifndef FIXTURE_XGE3_SOUND_LOAD_H
#define FIXTURE_XGE3_SOUND_LOAD_H
#include "foreign_xge3_audio_resources.h"
static int fixtureSoundLoadNativeCosts(void)
{
 size_t count0,bytes0,count1,bytes1;fixtureXgeAudioHeap(&count0,&bytes0);
 xge_async_owned request=NULL;int status=xgeAsyncSoundCreateOwned(&request,NULL);
 if(status)return status;
 fixtureXgeAudioHeap(&count1,&bytes1);
 printf("Native empty SoundLoad: %llu allocations / %llu bytes\n",
  (unsigned long long)(count1-count0),(unsigned long long)(bytes1-bytes0));
 return xgeAsyncLoadReleaseChecked(request);
}
#endif
