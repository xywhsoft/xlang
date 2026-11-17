#include "foreign_xge3d_scene_controls.h"
/* Test-only field observability, from the exact source frozen with the SDK.
 * No private function is called or exported by the language library. */
#include "src/xge/3d/xge3d_internal.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot warm;
void fixture3dControlsBudget(int allowance)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    if(allowance<0){xrtMemDebugFailClear();xrtClearError();}
    else assert(xrtMemDebugFailAfter(allowance));
}
void fixture3dControlsTriangle(xge3d_scene* scene,xge3d_node_t node)
{
    assert(xge3dSceneAccessBegin(scene)==XGE_OK);
    xge3d_vertex_t vertices[3]={0};
    vertices[0].position=(xge3d_vec3_t){-1,-1,0};
    vertices[1].position=(xge3d_vec3_t){1,-1,0};
    vertices[2].position=(xge3d_vec3_t){0,1,0};
    xge3d_mesh_desc_t desc={vertices,3,NULL,0,0};xge3d_mesh* mesh=NULL;
    assert(xge3dMeshCreate(&desc,&mesh)==XGE_OK);
    assert(xge3dNodeSetMesh(scene,node,mesh)==XGE_OK);xge3dMeshFree(mesh);
    assert(xge3dSceneAccessEnd(scene)==XGE_OK);
}
int fixture3dControlsTriggered(void)
{return xrtMemDebugFailTriggered() ? 1:0;}
void fixture3dControlsAppearance(xge3d_scene* scene,xge3d_node_t node,int light)
{
    assert(xge3dSceneAccessBegin(scene)==XGE_OK);
    xge3d_node_data* value=xrtSlotMapGet(&scene->slots,node.slot);
    assert(value && value->has_light==light);
    assert(value->color[0]==.25f && value->color[1]==.5f && value->color[2]==.75f && value->color[3]==1);
    if(light)assert(value->light.type==XGE3D_LIGHT_DIRECTIONAL && value->light.intensity==2 && value->light.direction.z==-1);
    assert(xge3dSceneAccessEnd(scene)==XGE_OK);
}
void fixture3dControlsWarmBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&warm);}
void fixture3dControlsWarmEnd(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(warm.AllocCount==after.AllocCount && warm.ReallocCount==after.ReallocCount && warm.FreeCount==after.FreeCount);
    puts("SCENE_CONTROLS_WARM_XRT_ALLOCATIONS=0");
}
void fixture3dControlsNativeWarm(xge3d_scene* scene,xge3d_node_t node)
{
    const xge3d_ray_t ray={{0,0,3},{0,0,-7}};const float rgba[4]={.25f,.5f,.75f,1};
    for(int i=0;i<1000;++i){
        assert(xge3dSceneAccessBegin(scene)==XGE_OK);xge3d_hit_t hit;uint32_t flags;
        assert(xge3dSceneRaycast(scene,&ray,5,&hit)==XGE_OK && hit.node.slot==node.slot && hit.distance==3);
        assert(xge3dSceneAccessEnd(scene)==XGE_OK);
        assert(xge3dSceneAccessBegin(scene)==XGE_OK);
        assert(xge3dNodeGetShadowFlags(scene,node,&flags)==XGE_OK && flags==XGE3D_SHADOW_RECEIVE);
        assert(xge3dSceneAccessEnd(scene)==XGE_OK);
        assert(xge3dSceneAccessBegin(scene)==XGE_OK);
        assert(xge3dNodeSetColor(scene,node,rgba)==XGE_OK);
        assert(xge3dSceneAccessEnd(scene)==XGE_OK);
    }
}
int fixture3dControlsNativeMain(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xge3d_scene* scene=NULL;assert(xge3dSceneCreateOwned(&scene)==XGE_OK);
    assert(xge3dSceneAccessBegin(scene)==XGE_OK);xge3d_node_t node={0};
    assert(xge3dNodeCreate(scene,(xge3d_node_t){0},&node)==XGE_OK);
    assert(xge3dSceneAccessEnd(scene)==XGE_OK);
    fixture3dControlsTriangle(scene,node);
    assert(xge3dSceneAccessBegin(scene)==XGE_OK);
    assert(xge3dNodeSetShadowFlags(scene,node,XGE3D_SHADOW_RECEIVE)==XGE_OK);
    assert(xge3dScenePrepareSpatial(scene)==XGE_OK);assert(xge3dSceneAccessEnd(scene)==XGE_OK);
    fixture3dControlsWarmBegin();fixture3dControlsNativeWarm(scene,node);fixture3dControlsWarmEnd();
    assert(xge3dSceneReleaseChecked(scene)==XGE_OK);xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    puts("SCENE_CONTROLS_NATIVE_OK");return 0;
}
