#define XRT_MODULE_ALL
#include "foreign_xge3d_scene_resolver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct ResolverState {
    xge3d_scene *scene;
    xge3d_model *model;
    xge3d_node_t anchor;
    unsigned calls;
} ResolverState;

static int resolve(const xstrview *uri, xge3d_scene_model_output_t *output, void *user)
{
    ResolverState *state = user;
    const char *expected = state->calls++ == 0 ? "first" : "second";
    assert(uri->Size == strlen(expected) && memcmp(uri->Data, expected, uri->Size) == 0);
    assert(xge3dSceneNodeCount(state->scene) == 1);
    xge3d_node_t nested = state->anchor;
    assert(xge3dSceneLoadOwned(state->scene, state->anchor, NULL, 0, NULL, 0, NULL, &nested) == XGE_ERROR_BUSY);
    assert(!nested.slot);
    return xge3dSceneModelOutputSet(output, state->model);
}

void fixture3dResolverNative(xge3d_scene *scene, xge3d_node_t anchor, xge3d_model *model)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xge3d_resource_info_t resources = {.size = sizeof(resources)};
    assert(xge3dModelResourceInfo(model, &resources) == XGE_OK);
    const uint64_t references = resources.references;
    xge3d_scene_node_desc_t rows[] = {xge3dSceneNodeDefault(), xge3dSceneNodeDefault()};
    rows[0].name = rows[0].model_uri = "first";
    rows[1].name = rows[1].model_uri = "second";
    rows[1].parent = 0;

    unsigned faults = 0, resolved_faults = 0, successes = 0;
    for (int allowance = 0; allowance < 48; ++allowance) {
        ResolverState state = {scene, model, anchor, 0};
        xge3d_node_t root = anchor;
        assert(xrtMemDebugFailAfter(allowance));
        int status = xge3dSceneLoadWithResolver(scene, anchor, resolve, &state, rows, 2, NULL, 0, &root);
        xrtMemDebugFailClear();
        xrtClearError();
        if (status == XGE_OK) {
            assert(state.calls == 2 && root.slot && root.slot != anchor.slot);
            assert(xge3dNodeDestroy(scene, root) == XGE_OK);
            ++successes;
        } else {
            assert(status == XGE_ERROR_OUT_OF_MEMORY && !root.slot);
            ++faults;
            if (state.calls == 2) ++resolved_faults;
        }
        assert(xge3dSceneNodeCount(scene) == 1 && xge3dNodeChildCount(scene, anchor) == 0);
        assert(xge3dModelResourceInfo(model, &resources) == XGE_OK && resources.references == references);
    }
    assert(faults > 0 && resolved_faults > 0 && successes > 0);
    printf("SCENE_RESOLVER_NATIVE_OK faults=%u after-resolution=%u successes=%u\n", faults, resolved_faults, successes);
}
