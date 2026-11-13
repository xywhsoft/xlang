#ifndef FOREIGN_XUI_ICON_CACHE_H
#define FOREIGN_XUI_ICON_CACHE_H
#include "test/cases/foreign_xui_render_backend.h"
int fixtureCacheProxy(xui_proxy* output);
void fixtureCacheObserveInput(int raster,const void* data,int size);
int fixtureCacheCopiedInputs(void);
int fixtureCacheLoads(int raster);
void fixtureCacheHeapBegin(void);
int fixtureCacheHeapUnchanged(void);
#endif
