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
#include "layout_shape_test.c"
