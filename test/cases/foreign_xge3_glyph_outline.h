#ifndef FOREIGN_XGE3_GLYPH_OUTLINE_H
#define FOREIGN_XGE3_GLYPH_OUTLINE_H
#include <xge3_runtime.h>
const char* fixtureOutlinePath(void);
void fixtureOutlineBudget(int allowance);
void fixtureOutlineHeapBegin(void);
void fixtureOutlineHeapReport(int native);
void fixtureOutlineNativePerformance(xge_font font,xge_shape_ex shape,int glyph);
#endif
