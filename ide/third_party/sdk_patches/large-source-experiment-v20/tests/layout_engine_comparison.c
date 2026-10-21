static unsigned long long paintCollectStarts,paintCollectFast,paintCollectFallback;
static int paintCollectMaxCandidates;
static void paintCollectTrace(int event,int count) {
 if(event==1)++paintCollectStarts;
 if(event==2)++paintCollectFast;
 if(event==3)++paintCollectFallback;
 if(count>paintCollectMaxCandidates)paintCollectMaxCandidates=count;
}
#define XGE_TEXT_PAINT_TRACE(event,count) paintCollectTrace(event,count)
static unsigned long long emojiMemoHits,emojiMemoMisses,emojiMemoBypass;
static void emojiMemoTrace(int event,int count) {
 (void)count;if(event==1)++emojiMemoHits;else if(event==2)++emojiMemoMisses;else if(event==3)++emojiMemoBypass;
}
#define XGE_TEXT_EMOJI_MATCH_TRACE(event,count) emojiMemoTrace(event,count)
static unsigned long long glyphMetricHits,glyphMetricMisses,glyphMetricAtlas,glyphMetricBypass;
static void glyphMetricTrace(int event) {
 if(event==1)++glyphMetricHits;else if(event==2)++glyphMetricMisses;else if(event==3)++glyphMetricAtlas;else if(event==4)++glyphMetricBypass;
}
#define XGE_TEXT_GLYPH_METRIC_TRACE(event) glyphMetricTrace(event)
#include "xge.c"
#include "src/xui_proxy_xge.c"
static unsigned long long rowColumnSeeds,rowColumnPoints,rowColumnResets,rowColumnFailures;
static int rowColumnFailAllocations;
static void rowColumnTrace(int event,int value) {
 (void)value;if(event==1)++rowColumnSeeds;else if(event==2)++rowColumnPoints;else if(event==3)++rowColumnResets;else if(event==4)++rowColumnFailures;
}
static void* rowColumnAllocate(void* pointer,size_t size) {
 if(rowColumnFailAllocations){--rowColumnFailAllocations;return NULL;}return xrtRealloc(pointer,size);
}
#define XUI_CODE_EDIT_COLUMN_INDEX_TRACE(event,value) rowColumnTrace(event,value)
#define XUI_CODE_EDIT_COLUMN_INDEX_REALLOC(pointer,size) rowColumnAllocate(pointer,size)
#include "src/xui_code_edit.c"
#include "platform/dpi_proxy.h"
#include "layout_old_shape.c"
#include "layout_old_layout.c"
#include "layout_stream_checks.c"
#include "layout_widget_checks.c"
#include "editor_kern_old_stream_v1.c"
#include "editor_kern_checks_v1.c"
#include "editor_cursor_checks_v2.c"
#include "editor_cursor_layout_checks_v2.c"
#include "editor_paint_stream_checks_v2.c"
#include "editor_paint_collect_checks_v2.c"
#include "editor_emoji_match_checks_v1.c"
#include "editor_glyph_metrics_checks_v1.c"
#include "editor_row_index_reference_v1.c"
#include "editor_row_index_checks_v1.c"
#include "layout_shape_test.c"
