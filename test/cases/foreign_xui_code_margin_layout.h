#ifndef FOREIGN_XUI_CODE_MARGIN_LAYOUT_H
#define FOREIGN_XUI_CODE_MARGIN_LAYOUT_H
#include <xui3_code_margin.h>
#ifdef __cplusplus
extern "C" {
#endif
void fixtureMarginLayoutProbe(xui_code_margin_model model, xui_rect_t viewport);
void fixtureMarginLayoutBegin(void);
void fixtureMarginLayoutReport(int mode);
void fixtureMarginLayoutBudget(int budget);
#ifdef __cplusplus
}
#endif
#endif
