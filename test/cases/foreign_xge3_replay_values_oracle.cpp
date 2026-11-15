#include "foreign_xge3_replay_values.h"
#include <type_traits>
#define VALUE_ABI(T) static_assert(std::is_standard_layout<T>::value && std::is_trivially_copyable<T>::value, #T)
VALUE_ABI(xge_touch_event_t);
VALUE_ABI(xui_property_grid_category_t);
VALUE_ABI(xui_code_search_result_t);
VALUE_ABI(xui_find_result_t);
VALUE_ABI(xui_code_document_batch_layout_t);
VALUE_ABI(xui_code_document_batch_source_t);
#undef VALUE_ABI
extern "C" xge_touch_event_t fixtureReplayCppTouch(xge_touch_event_t value)
{value.arrPoints[7].fY+=2;return value;}
extern "C" xui_property_grid_category_t fixtureReplayCppCategory(xui_property_grid_category_t value)
{value.bExpanded=1;return value;}
extern "C" xui_code_search_result_t fixtureReplayCppSearch(xui_code_search_result_t value)
{value.arrCaptures[9].iStart++;return value;}
extern "C" xui_find_result_t fixtureReplayCppFind(xui_find_result_t value)
{value.arrCaptures[9].iEnd+=3;return value;}
extern "C" xui_code_document_batch_layout_t fixtureReplayCppLayout(xui_code_document_batch_layout_t value)
{value.iCapture++;return value;}
extern "C" xui_code_document_batch_source_t fixtureReplayCppSource(xui_code_document_batch_source_t value)
{value.iUndoCount++;return value;}
