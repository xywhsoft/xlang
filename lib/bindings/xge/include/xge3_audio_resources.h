#ifndef XL6_XGE3_AUDIO_RESOURCES_H
#define XL6_XGE3_AUDIO_RESOURCES_H
#include <xge3_runtime.h>
/* The language shares one owned sound record. No native reference is exported;
 * playing voices retain their audio data, rather than this outer record. */
static bool xl6Xge3SoundDestroy(void* raw)
{ xgeSoundFree((xge_sound)raw);xrtFree(raw);return true; }
static const XL6_ForeignViewDescriptorV1* xl6Xge3SoundPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.sound",0,1,NULL,NULL,xl6Xge3SoundDestroy,0,1,NULL,NULL,NULL,NULL,0,xl6Xge3SoundDestroy,NULL};
 return &policy;
}
#define xl6Xge3SoundPolicy (*xl6Xge3SoundPolicyGet())
#endif
