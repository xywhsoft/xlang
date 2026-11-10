#ifndef FIXTURE_XGE3_SCENE_H
#define FIXTURE_XGE3_SCENE_H
#include <xge3_scene.h>
#include <string.h>
static inline int fixtureSceneEvent(int type)
{
 xge_event_t event;memset(&event,0,sizeof(event));event.iType=type;event.iParam1=42;event.fX=7.5f;
 event.iPointerId=UINT64_C(0xfedcba9876543210);event.iCodepoint=0x4f60;event.fTime=1.25;
 return xgeSceneDispatchEvent(&event);
}
static inline void fixtureSceneHeap(size_t* count,size_t* bytes)
{
 xmemdebugsnapshot snapshot;memset(&snapshot,0,sizeof(snapshot));xrtMemDebugSnapshot(&snapshot);
 *count=snapshot.LiveCount;*bytes=snapshot.LiveBytes;
}
#endif
