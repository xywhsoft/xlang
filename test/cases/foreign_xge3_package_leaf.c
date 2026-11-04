/* Pure CPU native resource lifetimes; no render/audio device is initialized. */
#include <xge3_runtime.h>
#include <assert.h>
#include <stdio.h>
static XL6_ForeignView* own(void* raw,const XL6_ForeignViewDescriptorV1* policy)
{XL6_ForeignView* view=xl6ForeignViewCreateRootV1(raw,policy,NULL);assert(view);return view;}
static void close_view(XL6_ForeignView* view)
{assert(xl6ForeignViewCloseV1(view));assert(xl6ForeignViewReleaseV1(view));}
int main(void)
{
    assert(!xl6Xge3RuntimeBindV1());
#if XGE_ENABLE_SHAPE_EX
    xge_shape_ex shape=NULL;assert(xgeShapeExCreate(&shape)==XGE_OK);
    assert(xgeShapeExMoveTo(shape,0,0)==XGE_OK && xgeShapeExLineTo(shape,3,4)==XGE_OK);
    assert(xgeShapeExAddRef(shape)==XGE_OK);
    close_view(own(shape,&xl6Xge3ShapePolicy));
    int refs=0;assert(xgeShapeExRefCountGet(shape,&refs)==XGE_OK && refs==1);
    xge_shape_ex_path_measure measure=NULL;
    assert(xgeShapeExPathMeasureCreate(&measure,shape,NULL,0)==XGE_OK);
    XL6_ForeignView* measured=own(measure,&xl6Xge3PathMeasurePolicy);
    xgeShapeExDestroy(shape); /* The flattened measurement owns its own data. */
    float length=0;assert(xgeShapeExPathMeasureGetLength(measure,&length)==XGE_OK && length>4.999f && length<5.001f);
    close_view(measured);
    xge_shape_ex_scene scene=NULL;assert(xgeShapeExSceneCreate(&scene)==XGE_OK);
    assert(xgeShapeExCreate(&shape)==XGE_OK && xgeShapeExSceneAdd(scene,shape)==XGE_OK);
    xgeShapeExDestroy(shape); /* Scene retains the child native reference. */
    assert(xgeShapeExSceneAddRef(scene)==XGE_OK);
    close_view(own(scene,&xl6Xge3ShapeScenePolicy));
    int count=0;assert(xgeShapeExSceneRefCountGet(scene,&refs)==XGE_OK && refs==1);
    assert(xgeShapeExSceneGetCount(scene,&count)==XGE_OK && count==1);xgeShapeExSceneDestroy(scene);
#endif
#if XGE_ENABLE_AUDIO
    static const unsigned char wav[]={
        'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
        1,0,1,0,0x40,0x1f,0,0,0x80,0x3e,0,0,2,0,16,0,'d','a','t','a',4,0,0,0,0,0,0xff,0x7f};
    xge_audio_clip_decode_t decode={0};decode.data=wav;decode.bytes=sizeof(wav);
    xge_audio_clip* clip=NULL;assert(xgeAudioClipDecodeMemory(&decode,&clip)==XGE_OK && clip);
    xge_audio_clip_info_t info=xgeAudioClipInfo(clip);assert(info.channels==1 && info.sample_rate==8000 && info.frames==2);
    assert(xgeAudioClipAddRef(clip)==XGE_OK);close_view(own(clip,&xl6Xge3AudioClipPolicy));
    const float* samples=xgeAudioClipSamples(clip);assert(samples && samples[0]==0 && samples[1]>.99f);xgeAudioClipFree(clip);
#endif
    puts("native leaf policies: shared Shape/Scene/AudioClip refs and independent PathMeasure CPU data: PASS");return 0;
}
