/* Actual owned CPU Proxy/Font/Surface/Draw, plus observation of native caches.
 * These adapters belong to this test provider, not the production binding. */
#include "foreign_xui_choice_cache.h"
#include "foreign_xui_render_backend.c"
static xui_draw_surface_proc originalDraw;
static xui_surface_get_generation_proc originalGeneration;
static xui_surface atlas;
static unsigned dependencyDraws,generationQueries;
static xmemdebugsnapshot before,prefix;
static int countedDraw(xui_proxy p,xui_draw_context d,xui_surface s,xui_rect_t a,xui_rect_t b,uint32_t c,uint32_t f)
{if(s==atlas)++dependencyDraws;return originalDraw(p,d,s,a,b,c,f);}
static int countedGeneration(xui_proxy p,xui_surface s,uint32_t* value)
{if(s==atlas)++generationQueries;return originalGeneration(p,s,value);}
int fixtureCacheProxy(xui_proxy* output)
{
 int status=fixtureRenderProxy(output);if(status!=XUI_OK)return status;
 originalDraw=backend.tProxy.drawSurface;backend.tProxy.drawSurface=countedDraw;
 originalGeneration=backend.tProxy.surfaceGetGeneration;backend.tProxy.surfaceGetGeneration=countedGeneration;
 atlas=NULL;dependencyDraws=generationQueries=0;return XUI_OK;
}
void fixtureCacheAtlas(xui_surface surface){atlas=unwrap(surface,SURFACE);}
void fixtureCacheMutate(xui_surface surface)
{
 xui_proxy p=xuiSurfaceGetProxy(surface);assert(p);
 assert(p->surfaceUpdateRGBA(p,surface,(xui_rect_i_t){0,0,1,1},NULL,0)==XUI_OK);
}
void fixtureCacheBegin(void)
{dependencyDraws=generationQueries=0;assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureCacheReport(int kind,int native)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 /* The native dependency table deduplicates the same atlas across states. */
 unsigned expected=1000;
 assert(dependencyDraws==0&&generationQueries==expected);
 assert(after.AllocCount==before.AllocCount&&after.FreeCount==before.FreeCount&&after.ReallocCount==before.ReallocCount);
 printf("Choice idle kind=%d native=%d queries=%u alloc=0 free=0 realloc=0\n",kind,native,generationQueries);
}
void fixtureCacheNativeIdle(xui_context ui)
{for(int i=0;i<1000;++i)assert(xuiRenderPrepare(ui)==XUI_OK);}
void fixtureCacheExpectDraw(void){assert(dependencyDraws>0);}
void fixtureCachePrefixBegin(void){assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&prefix);}
void fixtureCachePrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureCacheHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==prefix.LiveCount&&after.LiveBytes==prefix.LiveBytes&&after.InvalidFreeCount==prefix.InvalidFreeCount&&after.DoubleFreeCount==prefix.DoubleFreeCount);
}
