#ifndef FOREIGN_XUI_CONTEXT_TEXT_SHAPE_H
#define FOREIGN_XUI_CONTEXT_TEXT_SHAPE_H
#include <xge3_runtime.h>
int fixtureShapeContextProxy(xui_proxy* output);
int fixtureShapeContextOtherProxy(xui_proxy* output);
void fixtureShapeContextBlockCaps(void);
int fixtureShapeContextCapsCalls(void);
void fixtureShapeContextCompare(xui_context context, xui_font font);
#endif
