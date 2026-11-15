#ifndef FOREIGN_XUI_PAGE_TEXT_H
#define FOREIGN_XUI_PAGE_TEXT_H
#include "foreign_xui_render_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
void fixturePageTextBegin(void);
void fixturePageTextEqual(void);
void fixturePageTextBudget(int allowance);
void fixturePageTextReset(void);
void fixturePageTextReport(int native);
void fixturePageTextNativeHot(xui_widget page);
void fixturePageTextNativeOracle(void);
#ifdef __cplusplus
}
#endif
#endif
