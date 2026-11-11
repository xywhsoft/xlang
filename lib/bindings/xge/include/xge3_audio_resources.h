#ifndef XL6_XGE3_AUDIO_RESOURCES_H
#define XL6_XGE3_AUDIO_RESOURCES_H
#include <xge3_runtime.h>
/* Only cold lifetime policies. All playback methods are direct declarations.
 * Sound/Music/Stream factories own their outer records; Group uses the generic
 * in-place factory and releases its public record here. Native voices retain
 * the private group backend, not this potentially shorter-lived public record. */
static bool xl6Xge3SoundDestroy(void* raw)
{ return xgeSoundFreeChecked((xge_sound)raw)==XGE_OK; }
static const XL6_ForeignViewDescriptorV1* xl6Xge3SoundPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.sound",0,1,NULL,NULL,xl6Xge3SoundDestroy,0,1,NULL,NULL,NULL,NULL,0,xl6Xge3SoundDestroy,NULL};
 return &policy;
}
#define xl6Xge3SoundPolicy (*xl6Xge3SoundPolicyGet())

static bool xl6Xge3AudioGroupDestroy(void* raw)
{
 if(xgeAudioGroupFreeChecked((xge_audio_group)raw)!=XGE_OK)return false;
 xrtFree(raw);return true;
}
static const XL6_ForeignViewDescriptorV1* xl6Xge3AudioGroupPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.audioGroup",0,1,NULL,NULL,xl6Xge3AudioGroupDestroy,0,1,NULL,NULL,NULL,NULL,0,xl6Xge3AudioGroupDestroy,NULL};
 return &policy;
}
#define xl6Xge3AudioGroupPolicy (*xl6Xge3AudioGroupPolicyGet())
static const XL6_ForeignViewDescriptorV1* xl6Xge3MusicPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.music",0,1,NULL,NULL,xl6Xge3SoundDestroy,0,1,NULL,NULL,NULL,NULL,0,xl6Xge3SoundDestroy,NULL};
 return &policy;
}
#define xl6Xge3MusicPolicy (*xl6Xge3MusicPolicyGet())
static const XL6_ForeignViewDescriptorV1* xl6Xge3AudioStreamPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.audioStream",0,1,NULL,NULL,xl6Xge3SoundDestroy,0,1,NULL,NULL,NULL,NULL,0,xl6Xge3SoundDestroy,NULL};
 return &policy;
}
#define xl6Xge3AudioStreamPolicy (*xl6Xge3AudioStreamPolicyGet())
#endif
