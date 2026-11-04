#include "test/cases/foreign_region.h"
#include <__xl_runtime/xl_foreign_error.h>
#include <assert.h>
#include <string.h>
static int consume(int w,int h,const void* pixels,int stride)
{
 if(!pixels)return 0;
 const unsigned char* p=pixels;int sum=0;size_t row=(size_t)w*4;
 size_t step=stride>0 ? (size_t)stride:row;
 for(int y=0;y<h;++y)for(size_t x=0;x<row;++x)sum+=p[(size_t)y*step+x];
 return sum;
}
static int createRGBA(RegionRoot* r,int* out,int w,int h,const void* p,int stride,unsigned flags)
{assert(r && r->depth && !flags);++r->calls;*out=consume(w,h,p,stride);return 0;}
static int updateRGBA(RegionRoot* r,RegionRoot* surface,RegionRect rect,const void* p,int stride)
{assert(r==surface && r->depth>=2 && !rect.x && !rect.y);++r->calls;return consume(rect.width,rect.height,p,stride);}
static int readRGBA(RegionRoot* r,RegionRoot* surface,void* pixels,int stride)
{
 assert(r==surface && r->depth>=2 && pixels);++r->calls;
 unsigned char* p=pixels;size_t step=stride>0 ? (size_t)stride:8;
 if(r->mode==3){p[0]=99;return -13;}
 for(size_t y=0;y<2;++y)memset(p+y*step,(int)(y+1),8);
 return 0;
}
static int surfaceGetDesc(RegionRoot* r,RegionRoot* surface,RegionDesc* out)
{
 assert(r==surface && r->depth>=2);++r->queries;
 if(r->mode==6 || r->mode==7 || r->mode==9){
  if(r->mode==6)assert(xrtMemDebugFailAfter(0));
  else xrtSetErrorTake(xrtErrorCreate(XERR_STATE,"region-primary",73,"observer primary"));
  xl6ForeignCallbackFail();xrtClearError();assert(!xrtGetError());
  *out=(RegionDesc){2,2};
  if(r->mode==9){xrtSetErrorTake(xrtErrorCreate(XERR_RANGE,"region-secondary",87,"observer secondary"));return -9;}
  return 0;
 }
 if(r->mode==1 || r->mode==5){out->width=0x7fffffff;out->height=0x7fffffff;
  if(r->mode==5)assert(xrtMemDebugFailAfter(0));
  return -7;}
 *out=(RegionDesc){8/4,2};return 0;
}
int region_root_create(RegionRoot** out)
{
 *out=xrtCalloc(1,sizeof(**out));if(!*out)return -1;
 **out=(RegionRoot){.createRGBA=createRGBA,.updateRGBA=updateRGBA,.readRGBA=readRGBA,.surfaceGetDesc=surfaceGetDesc};return 0;
}
void region_root_destroy(RegionRoot* r){if(!r)return;r->closing=true;if(!r->depth)xrtFree(r);}
int region_root_mode(RegionRoot* r,int mode){assert(r && r->depth);r->mode=mode;r->surfaceGetDesc=mode==4 ? NULL:surfaceGetDesc;return mode;}
int region_root_queries(RegionRoot* r){assert(r && r->depth);return r->queries;}
int region_root_calls(RegionRoot* r){assert(r && r->depth);return r->calls;}
int region_free_read(int w,int h,const void* p,int stride){return consume(w,h,p,stride);}
int region_wide_read(uint64_t w,uint64_t h,const void* p,uint64_t stride)
{assert(w==2 && h==2 && p && !stride);return 1;}
void region_reset_failure(void){assert(xrtMemDebugFailAfter(-1));}
int region_callback_read(int w,int h,const void* p,int stride,int(*callback)(const uint8_t*,size_t,void*),void* user)
{assert(callback && p);return callback(p,(size_t)(h-1)*(stride>0 ? (size_t)stride:(size_t)w*4)+(size_t)w*4,user);}
static bool begin(void* raw,void* token)
{RegionRoot* r=raw;RegionScope* s=token;if(r->closing || s->root)return false;s->root=r;++r->depth;
 if(r->mode==8){r->mode=0;xrtSetErrorTake(xrtErrorCreate(XERR_STATE,"region-primary",83,"admission primary"));xl6ForeignCallbackFail();xrtClearError();}
 return true;}
static bool end(void* token)
{RegionScope* s=token;RegionRoot* r=s->root;assert(r && r->depth);s->root=NULL;--r->depth;if(r->closing)region_root_destroy(r);return true;}
static bool destroy(void* raw){region_root_destroy(raw);return true;}
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user){(void)raw;(void)visit;(void)user;return true;}
static bool busy(const void* raw){return ((const RegionRoot*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 regionPolicy={sizeof(regionPolicy),XL6_FOREIGN_VIEW_V1_TAG,
 "binding.region",0,1,NULL,NULL,destroy,sizeof(RegionScope),_Alignof(RegionScope),begin,end,trace,busy};
