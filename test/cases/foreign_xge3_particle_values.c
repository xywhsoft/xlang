#include "foreign_xge3_particle_values.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureParticleHotBegin(void){xrtMemDebugSnapshot(&before);}
void fixtureParticleHotEnd(int native_reference)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    printf("%s particle values: zero XRT heap operations\n",native_reference?"Native":"Language");
}
double fixtureParticleNativeHot(const xge_particle_curve_t* curve,const xge_particle_gradient_t* gradient)
{
    double result=0;int i;
    for(i=0;i<1000;++i){
        xge_particle_emitter_t emitter;xgeParticleEmitterInit(&emitter);
        result+=(double)emitter.iMaxParticles;
        result+=xgeParticleCurveEval(curve,0.5f,-17.0f);
        result+=(double)(xgeParticleGradientEval(gradient,0.5f,0)&255u);
    }
    return result;
}
int fixtureParticleEmitterSentinels(const xge_particle_emitter_t* e)
{
    return e && e->sName[63]==65 && e->sTexture[127]==66 && e->sMaterial[63]==67 &&
        e->tSizeOverLife.arrKeys[7].fValue==33 && e->tSpeedOverLife.arrKeys[6].fInTangent==34 &&
        e->tAlphaOverLife.arrKeys[5].fOutTangent==35 && e->tColorOverLife.arrKeys[7].iColor==0x12345678u &&
        e->arrBursts[15].fTime==36 && e->arrBursts[15].iCount==37 &&
        e->arrSubEmitters[2].iEmitter==3 && e->arrSubEmitters[2].iCount==38 &&
        e->arrSubEmitters[2].fInheritVelocity==39 && e->tTextureRect.fW==40 && e->fFriction==41;
}
