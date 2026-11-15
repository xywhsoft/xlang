/* CPU test probes only; no alternate simulation or language business wrapper. */
#ifndef XRT_MODULE_ALL
#define XRT_MODULE_ALL
#endif
#include "extlibs/xrt/xrt.h"
#include "foreign_xge3_particles.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static xmemdebugsnapshot marked,before;
void fixtureParticlesBudget(int n)
{if(n<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
void fixtureParticlesMark(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&marked);}
static void balanced(const xmemdebugsnapshot* expected)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    assert(now.LiveCount==expected->LiveCount&&now.LiveBytes==expected->LiveBytes);
    assert(now.InvalidFreeCount==expected->InvalidFreeCount&&now.DoubleFreeCount==expected->DoubleFreeCount&&now.UseAfterFreeCount==expected->UseAfterFreeCount);
}
void fixtureParticlesBalanced(void){balanced(&marked);}
void fixtureParticlesNativeFailures(const xge3d_particle_desc_t* descriptor)
{
    for(int budget=0;budget<3;++budget){
        xmemdebugsnapshot start;xrtMemDebugSnapshot(&start);assert(xrtMemDebugFailAfter(budget));
        xge3d_particles* value=(xge3d_particles*)(uintptr_t)1;
        int status=xge3dParticlesCreate(descriptor,&value);xrtMemDebugFailClear();
        assert(budget<2 ? status==XGE_ERROR_OUT_OF_MEMORY&&!value:status==XGE_OK&&value);
        xge3dParticlesFree(value);xrtClearError();balanced(&start);
    }
    puts("Particles native constructor: 2 allocation failures and success balanced");
}
static void compare(xge3d_particles* actual,xge3d_particles* expected)
{
    xge3d_particle_info_t a={.size=sizeof(a)},b={.size=sizeof(b)};
    assert(xge3dParticlesInfo(actual,&a)==XGE_OK&&xge3dParticlesInfo(expected,&b)==XGE_OK);
    assert(a.alive==b.alive&&a.emitted==b.emitted&&a.dropped==b.dropped);
    xge3d_billboard_t aa[8],bb[8];size_t an=0,bn=0;
    assert(xge3dParticlesBillboards(actual,NULL,XGE3D_BILLBOARD_ALPHA,aa,8,&an)==XGE_OK);
    assert(xge3dParticlesBillboards(expected,NULL,XGE3D_BILLBOARD_ALPHA,bb,8,&bn)==XGE_OK&&an==bn);
    for(size_t i=0;i<an;++i){
        assert(aa[i].position.x==bb[i].position.x&&aa[i].position.y==bb[i].position.y&&aa[i].position.z==bb[i].position.z);
        assert(aa[i].width==bb[i].width&&aa[i].height==bb[i].height&&aa[i].rotation==bb[i].rotation);
        for(int c=0;c<4;++c){assert(aa[i].color[c]==bb[i].color[c]&&aa[i].uv[c]==bb[i].uv[c]);}
        assert(!aa[i].texture&&!bb[i].texture&&aa[i].blend==bb[i].blend&&aa[i].depth_test==bb[i].depth_test);
    }
}
void fixtureParticlesCompare(xge3d_particles* value,const xge3d_particle_desc_t* descriptor)
{
    xge3d_particles* reference=NULL;assert(xge3dParticlesCreate(descriptor,&reference)==XGE_OK);
    assert(xge3dParticlesEmit(reference,5)==XGE_OK);
    assert(xge3dParticlesSetPosition(reference,(xge3d_vec3_t){3,4,5})==XGE_OK);
    assert(xge3dParticlesEmit(reference,3)==XGE_OK&&xge3dParticlesUpdate(reference,.125f,0)==XGE_OK);
    assert(xge3dParticlesEmit(reference,10)==XGE_OK&&xge3dParticlesUpdate(reference,.125f,0)==XGE_OK);
    compare(value,reference);xge3dParticlesFree(reference);
}
void fixtureParticlesPair(xge3d_particles* a,xge3d_particles* b){compare(a,b);}
float fixtureParticlesInvalid(int kind){return kind==0?NAN:kind==1?INFINITY:-INFINITY;}
void fixtureParticlesHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureParticlesHeapReport(int native)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    printf("Particles heap %s: alloc=%llu free=%llu realloc=%llu; descriptor=%zu info=%zu\n",native?"native":"language",
        (unsigned long long)(now.AllocCount-before.AllocCount),(unsigned long long)(now.FreeCount-before.FreeCount),
        (unsigned long long)(now.ReallocCount-before.ReallocCount),sizeof(xge3d_particle_desc_t),sizeof(xge3d_particle_info_t));
    assert(now.AllocCount==before.AllocCount&&now.FreeCount==before.FreeCount&&now.ReallocCount==before.ReallocCount);
}
void fixtureParticlesNativeHot(xge3d_particles* value)
{
    for(int i=0;i<1000;++i){
        xge3d_particle_info_t info={.size=sizeof(info)};
        assert(xge3dParticlesSetPosition(value,(xge3d_vec3_t){3,4,5})==XGE_OK);
        assert(xge3dParticlesEmit(value,0)==XGE_OK&&xge3dParticlesUpdate(value,0,0)==XGE_OK);
        assert(xge3dParticlesInfo(value,&info)==XGE_OK&&info.alive==8&&info.emitted==8&&info.dropped==10);
    }
}
void fixtureParticlesNativeCold(const xge3d_particle_desc_t* descriptor)
{
    xmemdebugsnapshot a,b;xrtMemDebugSnapshot(&a);xge3d_particles* value=NULL;
    assert(xge3dParticlesCreate(descriptor,&value)==XGE_OK);xrtMemDebugSnapshot(&b);
    printf("Particles cold native: alloc=%llu bytes=%llu\n",(unsigned long long)(b.AllocCount-a.AllocCount),(unsigned long long)(b.LiveBytes-a.LiveBytes));
    xge3dParticlesFree(value);balanced(&a);
}
void fixtureParticlesColdReport(void)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    printf("Particles cold language: alloc=%llu bytes=%llu\n",(unsigned long long)(now.AllocCount-before.AllocCount),(unsigned long long)(now.LiveBytes-before.LiveBytes));
}
typedef struct Address {const void* pointer;bool found;} Address;
static bool live(const xmemdebugallocation* allocation,void* raw)
{Address* value=raw;if(allocation->Address==value->pointer){value->found=true;return false;}return true;}
static bool isLive(const void* pointer)
{Address value={pointer,false};xrtMemDebugVisitLive(live,&value);return value.found;}
static const void* retired;
int fixtureParticlesDuringCall(xge3d_particles* value,void(*callback)(void*),void* user)
{
    assert(isLive(value));callback(user);
    /* Check without dereferencing a freed allocation, even on a broken root. */
    assert(isLive(value));xge3d_particle_info_t info={.size=sizeof(info)};
    assert(xge3dParticlesInfo(value,&info)==XGE_OK);retired=value;return XGE_OK;
}
void fixtureParticlesRetired(void){assert(retired&&!isLive(retired));retired=NULL;}
