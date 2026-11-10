#include "native_sequence_projection.h"
#include <xrt_decl.h>
#include <limits.h>
struct NativeProjectionOwner { int references; NativeProjectionRow rows[2]; NativeProjectionPacked packed[2]; };
static int live,privateAnchor;
static void privateCallback(void) { ++privateAnchor; }
int nativeProjectionCreate(NativeProjectionOwner** out)
{
 if(!out)return -1;
 *out=NULL;NativeProjectionOwner* owner=xrtCalloc(1,sizeof(*owner));if(!owner)return -2;
 owner->references=1;
 for(int i=0;i<2;++i){
  owner->rows[i]=(NativeProjectionRow){&privateAnchor,10+i,{&privateAnchor,20+i,0.5+i},(unsigned long long)(30+i),privateCallback};
  owner->packed[i]=(NativeProjectionPacked){40+i,&privateAnchor,2.5+i};
 }
 ++live;*out=owner;return 0;
}
int nativeProjectionRetain(const NativeProjectionOwner* value)
{
 NativeProjectionOwner* owner=(NativeProjectionOwner*)value;
 if(!owner || owner->references<=0 || owner->references==INT_MAX)return 0;
 return ++owner->references;
}
void nativeProjectionRelease(NativeProjectionOwner* owner)
{if(owner && --owner->references==0){--live;xrtFree(owner);}}
bool nativeProjectionDestroy(void* owner)
{nativeProjectionRelease(owner);return true;}
int nativeProjectionRows(const NativeProjectionOwner* owner,const NativeProjectionRow** data,size_t* count)
{
 if(data)*data=NULL;
 if(count)*count=0;
 if(!owner || owner->references<=0 || !data || !count)return -1;
 *data=owner->rows;*count=2;return 0;
}
int nativeProjectionLive(void) {return live;}
int nativeProjectionFailAfter(int budget)
{
 if(budget<0 || (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)))return 0;
 return xrtMemDebugFailAfter(budget);
}
int nativeProjectionFailClear(void)
{int hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();return hit;}
int nativeProjectionPacked(const NativeProjectionOwner* owner,const NativeProjectionPacked** data,size_t* count)
{
 if(data)*data=NULL;
 if(count)*count=0;
 if(!owner || owner->references<=0 || !data || !count)return -1;
 *data=owner->packed;*count=2;return 0;
}
