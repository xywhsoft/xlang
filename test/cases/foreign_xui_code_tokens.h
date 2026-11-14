#ifndef XL6_TEST_XUI_CODE_TOKENS_H
#define XL6_TEST_XUI_CODE_TOKENS_H
#include <xui3_code_token_buffer.h>
void fixtureTokenBudget(int);
void fixtureTokenPrefixBegin(void);
void fixtureTokenPrefixEnd(void);
void fixtureTokenHeapEqual(void);
void fixtureTokenHotBegin(void);
void fixtureTokenHotReport(int);
void fixtureTokenCheck(xui_code_token_buffer);
void fixtureTokenNativeHot(xui_code_token_buffer);
void fixtureTokenNativeCreate(void);
void fixtureTokenNativeSet(xui_code_token_buffer);
int fixtureTokenDuringCall(xui_code_token_buffer,void (*)(void*),void*);
#endif
