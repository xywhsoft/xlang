#ifndef FOREIGN_XUI_CUSTOM_ICON_H
#define FOREIGN_XUI_CUSTOM_ICON_H
#include "test/cases/foreign_xui_render_backend.h"
int fixtureCustomOtherProxy(xui_proxy* output);
typedef struct fixture_custom_quad {xui_surface_vertex_t vertices[4];} fixture_custom_quad;
fixture_custom_quad fixtureCustomQuad(void);
void fixtureCustomMesh(void* vertices,size_t vertexBytes,void* indices,size_t indexBytes);
void fixtureCustomDashes(void* output,size_t bytes);
void fixtureCustomDrawBegin(void);
void fixtureCustomDrawEnd(void);
void fixtureCustomHeapBegin(void);
int fixtureCustomHeapUnchanged(void);
int fixtureCustomNative(xui_icon_category category,xui_icon* output);
void fixtureCustomPerformanceResult(int native);
#endif
