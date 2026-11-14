#ifndef XL6_TEST_XUI_TAG_BATCH_H
#define XL6_TEST_XUI_TAG_BATCH_H
#include <xge3_runtime.h>
int fixtureLayoutProxy(xui_proxy*);
int fixtureLayoutFont(xui_proxy,xui_font*);
void fixtureTagBudget(int);
void fixtureTagCleanup(void);
void fixtureTagPrefixBegin(void);
void fixtureTagPrefixEnd(void);
void fixtureTagHeapEqual(void);
void fixtureTagHotBegin(void);
void fixtureTagHotReport(int);
void fixtureTagNativeHot(xui_widget);
#endif
