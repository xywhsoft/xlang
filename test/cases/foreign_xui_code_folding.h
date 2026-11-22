#ifndef XL6_TEST_XUI_CODE_FOLDING_H
#define XL6_TEST_XUI_CODE_FOLDING_H
#include <xui3_code_fold_state.h>
void fixtureFoldBudget(int);
void fixtureFoldPrefixBegin(void);
void fixtureFoldPrefixEnd(void);
void fixtureFoldHeapEqual(void);
void fixtureFoldHotBegin(void);
void fixtureFoldHotReport(int);
void fixtureFoldHotAssertZero(void);
void fixtureFoldNativeHot(xui_code_fold_state);
void fixtureFoldNativeCreate(void);
void fixtureFoldNativeSet(xui_code_fold_state);
int fixtureFoldDuringCall(xui_code_fold_state,void (*)(void*),void*);
#endif
