#ifndef FOREIGN_XGE3_FONT_XRF_BYTES_H
#define FOREIGN_XGE3_FONT_XRF_BYTES_H
#include <xge3_runtime.h>
const char* fixtureXrfPath(void);
void fixtureXrfBudget(int allowance);
void fixtureXrfBegin(void);
void fixtureXrfEnd(void);
void fixtureXrfNativeReference(xge_font font);
void fixtureXrfLanguageReference(void);
#endif
