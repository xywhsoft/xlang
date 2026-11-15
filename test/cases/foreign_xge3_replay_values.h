#pragma once
#include "xge3_runtime.h"
#if defined(__cplusplus)
#define REPLAY_ASSERT(c) static_assert(c, #c)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define REPLAY_ASSERT(c) _Static_assert(c, #c)
#else
#define REPLAY_ASSERT(c)
#endif
REPLAY_ASSERT(sizeof(((xge_touch_event_t*)0)->arrPoints)/sizeof(xge_touch_point_t)==8);
REPLAY_ASSERT(sizeof(((xui_code_search_result_t*)0)->arrCaptures)/sizeof(xui_code_range_t)==10);
REPLAY_ASSERT(sizeof(((xui_find_result_t*)0)->arrCaptures)/sizeof(xui_code_range_t)==10);
REPLAY_ASSERT(sizeof(((xui_property_grid_category_t*)0)->sId)==64);
REPLAY_ASSERT(sizeof(((xui_property_grid_category_t*)0)->sName)==96);
#undef REPLAY_ASSERT
#ifdef __cplusplus
extern "C" {
#endif
xge_touch_event_t fixtureReplayTouch(xge_touch_event_t);
int fixtureReplayCategory(const xui_property_grid_category_t*,xui_property_grid_category_t*);
int fixtureReplaySearch(const xui_code_search_result_t*,xui_code_search_result_t*);
int fixtureReplayFind(const xui_find_result_t*,xui_find_result_t*);
xui_code_document_batch_layout_t fixtureReplayLayout(xui_code_document_batch_layout_t);
int fixtureReplaySource(const xui_code_document_batch_source_t*,xui_code_document_batch_source_t*);
void fixtureReplayBegin(void);
void fixtureReplayReport(int);
void fixtureReplayNativeLoop(void);
void fixtureReplayConsume(int);
void fixtureReplayBudget(int);
xge_touch_event_t fixtureReplayCppTouch(xge_touch_event_t);
xui_property_grid_category_t fixtureReplayCppCategory(xui_property_grid_category_t);
xui_code_search_result_t fixtureReplayCppSearch(xui_code_search_result_t);
xui_find_result_t fixtureReplayCppFind(xui_find_result_t);
xui_code_document_batch_layout_t fixtureReplayCppLayout(xui_code_document_batch_layout_t);
xui_code_document_batch_source_t fixtureReplayCppSource(xui_code_document_batch_source_t);
#ifdef __cplusplus
}
#endif
