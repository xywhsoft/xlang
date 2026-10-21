#ifndef XGE_TEXT_SHAPE_INTERNAL_H
#define XGE_TEXT_SHAPE_INTERNAL_H
#include "../xge.h"
/* Private synchronous visitor. A position is final only after the next glyph
   applies kerning. The borrowed position is valid during the visitor call. */
typedef int (*xge_internal_text_shape_visit_proc)(const xge_glyph_position_t*, void*);
int xgeInternalTextShapeVisit(const xge_text_shape_desc_t*, xge_glyph_run_t*,
    xge_internal_text_shape_visit_proc, void*);
#endif
