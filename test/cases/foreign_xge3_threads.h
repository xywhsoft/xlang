#ifndef XL6_TEST_XGE3_THREADS_H
#define XL6_TEST_XGE3_THREADS_H
/* White-box acceptance only: actual package policies, runtime queue and XGE.
 * No language reference is consumed behind its owner's back. This helper
 * creates and transfers its own sole ForeignView reference to a raw worker. */
#include <xge3_runtime.h>
#include <xge3_gpu_buffer.h>
#include <xge3_shader.h>
#include <xge3_render_target.h>
#include <xge3_pass.h>
#include <__xl_runtime/xl6_foreign_context.h>
#include <assert.h>
#include <string.h>

typedef struct fixture_xge_worker {
    XL6_ForeignView* view;
    uint64_t creator;
    uint64_t graphicsOwners;
    bool passed;
} fixture_xge_worker;

static int32 fixture_xge_drop_worker(ptr data)
{
    fixture_xge_worker* state=data;
    assert(xrtThreadCurrentId()!=state->creator);
    XL6_ForeignViewCallV1 call={0};
    assert(!xl6ForeignViewCallBeginV1(state->view,&call,NULL,0));xrtClearError();
    assert(!xl6ForeignViewCloseV1(state->view));xrtClearError();
    assert(xl6ForeignContextReadyV1());
    xrtownershipscope warm={0};assert(xrtOwnershipMutationBegin(&warm) && xrtOwnershipScopeEnd(&warm));
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ForeignViewReleaseV1(state->view));state->view=NULL;
    assert(xl6ForeignViewPendingV1(state->creator)==1);
    assert(xl6ForeignViewDrainV1());
    assert(xl6ForeignViewPendingV1(state->creator)==1);
    assert(xgeGraphicsOwnedResourceCount()==state->graphicsOwners);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    state->passed=true;return 0;
}

static inline bool fixtureXgeForeignThreadRelease(int kind,const XL6_CodeOwner* code)
{
    assert(xl6ForeignViewPendingV1(0)==0 && xgeGraphicsOwnedResourceCount()==0);
    assert(xrtMemDebugEnabled() && xl6ForeignContextReadyV1());
    void* raw=NULL;const XL6_ForeignViewDescriptorV1* policy=NULL;
    if(kind==0){
        xge_texture value=NULL;assert(xgeTextureCreateRGBAOwned(&value,2,2,NULL,0)==XGE_OK);
        raw=value;policy=&xl6Xge3TexturePolicy;
    }else if(kind==1){
        xge_font value=NULL;xge_system_font_desc_t desc={0};desc.iSize=sizeof(desc);
        desc.iRole=XGE_SYSTEM_FONT_MONOSPACE;desc.fPixelSize=16;
        assert(xgeFontLoadSystemOwned(&value,&desc)==XGE_OK);raw=value;policy=&xl6Xge3FontPolicy;
    }else if(kind==2){
        xge_image value=xrtCalloc(1,sizeof(*value));assert(value);
        static const unsigned char png[]={0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0,0,0,0x0d,0x49,0x48,0x44,0x52,0,0,0,1,0,0,0,1,8,6,0,0,0,0x1f,0x15,0xc4,0x89,0,0,0,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0,5,0,1,0xff,0x89,0x99,0x3d,0x1d,0,0,0,0,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
        assert(xgeImageLoadMemory(value,png,sizeof(png))==XGE_OK);raw=value;policy=&xl6Xge3ImagePolicy;
    }else if(kind==3){
        xge_material value=xrtCalloc(1,sizeof(*value));assert(value);xgeMaterialInit(value);
        xge_texture texture=NULL;assert(xgeTextureCreateRGBAOwned(&texture,2,2,NULL,0)==XGE_OK);
        assert(xgeMaterialSetTextureChecked(value,texture)==XGE_OK);xgeTextureFree(texture);
        raw=value;policy=&xl6Xge3MaterialPolicy;
    }else if(kind==4){
        xge_buffer value=NULL;assert(xgeBufferCreateOwned(&value,XGE_BUFFER_VERTEX,XGE_BUFFER_STATIC,"abc",3)==XGE_OK);
        raw=value;policy=&xl6Xge3GpuBufferPolicy;
    }else if(kind==5 || kind==6){
        xge_render_target value=NULL;assert(xgeRenderTargetCreateOwned(&value,2,2)==XGE_OK);
        if(kind==5){raw=value;policy=&xl6Xge3TargetPolicy;}
        else{
            xge_pass pass=NULL;assert(xgePassCreateOwned(&pass,value,0,0)==XGE_OK);
            xgeRenderTargetFree(value);raw=pass;policy=&xl6Xge3PassPolicy;
        }
    }else if(kind==7){
        xge_shader_variant_set value=xrtCalloc(1,sizeof(*value));assert(value);
        assert(xgeShaderVariantSetInit(value,"void main(){}","void main(){}")==XGE_OK);
        raw=value;policy=&xl6Xge3ShaderVariantsPolicy;
    }else{
        assert(kind==8);xge_shader value=NULL;
        assert(xgeShaderCreateOwned(&value,"#version 330\nvoid main(){gl_Position=vec4(0.0);}","#version 330\nout vec4 color;void main(){color=vec4(1.0);}")==XGE_OK);
        raw=value;policy=&xl6Xge3ShaderPolicy;
    }
    fixture_xge_worker state={0};state.creator=xrtThreadCurrentId();
    state.graphicsOwners=xgeGraphicsOwnedResourceCount();
    state.view=xl6ForeignViewCreateRootV1(raw,policy,code);assert(state.view);
    xthread* worker=xrtThreadCreate(fixture_xge_drop_worker,&state,0);assert(worker);
    assert(xrtThreadWaitFor(worker,30000)==XWAIT_OK && xrtThreadExitCode(worker)==0);xrtThreadDestroy(worker);
    assert(state.passed && !state.view && xl6ForeignViewPendingV1(state.creator)==1);
    assert(xgeGraphicsOwnedResourceCount()==state.graphicsOwners);
    assert(xl6ForeignViewDrainV1());
    if(kind==8){
        /* A callback cannot drain native/code retirement while its C stack
         * is live. The generated checked Run boundary must deliver it. */
        assert(xl6ForeignCallbackActiveV1());
        assert(xl6ForeignViewPendingV1(state.creator)==1);
        assert(xgeGraphicsOwnedResourceCount()==state.graphicsOwners);
    }else{
        assert(!xl6ForeignViewPendingV1(state.creator));
        assert(xgeGraphicsOwnedResourceCount()==0);
    }
    return true;
}
static inline bool fixtureXgeThreadQueueIdle(void)
{return !xl6ForeignViewPendingV1(0) && !xgeGraphicsOwnedResourceCount();}
#endif
