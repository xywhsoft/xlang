#include "test/cases/foreign_xge3d_scene_defaults.h"
#include "xge3_runtime.h"

#include <assert.h>

void fixtureSceneNodeDefaults(void)
{
    xge3d_scene_node_desc_t legacy = xge3dSceneNodeDefault();
    xge3d_scene_node_defaults_t defaults = xge3dSceneNodeDefaultsGet();

    assert(legacy.name == NULL && legacy.mesh == NULL && legacy.material == NULL);
#if XGE3D_ENABLE_MODEL
    assert(legacy.model_uri == NULL);
#endif
    assert(defaults.parent == SIZE_MAX && defaults.flags == 0);
    assert(legacy.parent == defaults.parent && legacy.flags == defaults.flags);
    assert(legacy.transform.position.x == defaults.transform.position.x);
    assert(legacy.transform.position.y == defaults.transform.position.y);
    assert(legacy.transform.position.z == defaults.transform.position.z);
    assert(legacy.transform.rotation.x == defaults.transform.rotation.x);
    assert(legacy.transform.rotation.y == defaults.transform.rotation.y);
    assert(legacy.transform.rotation.z == defaults.transform.rotation.z);
    assert(legacy.transform.rotation.w == defaults.transform.rotation.w);
    assert(legacy.transform.scale.x == defaults.transform.scale.x);
    assert(legacy.transform.scale.y == defaults.transform.scale.y);
    assert(legacy.transform.scale.z == defaults.transform.scale.z);
}
