#ifndef XL6_TEST_XUI_CHOICE_CACHE_H
#define XL6_TEST_XUI_CHOICE_CACHE_H
#include <xge3_runtime.h>
int fixtureCacheProxy(xui_proxy*);
int fixtureRenderFont(xui_proxy,xui_font*);
int fixtureRenderBalanced(void);
void fixtureRenderBudget(int);
void fixtureCacheAtlas(xui_surface);
void fixtureCacheMutate(xui_surface);
void fixtureCacheBegin(void);
void fixtureCacheReport(int,int);
void fixtureCacheNativeIdle(xui_context);
void fixtureCacheExpectDraw(void);
void fixtureCachePrefixBegin(void);
void fixtureCachePrefixEnd(void);
void fixtureCacheHeapEqual(void);
#endif
