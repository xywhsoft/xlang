#ifndef XLANG_IDE_READING_STATE_H
#define XLANG_IDE_READING_STATE_H
#include "app/xide_app.h"
/* Stable node keys are UTF-8 JSON tuples; release them with xrtFree. */
char* xideReadingKey(const char* kind,const char* owner,const char* value);
double xideReadingNumber(const xvalue* value,double fallback);
xvalue* xideReadingCapture(xide_app* app,const xvalue* previous);
int xideReadingValid(const xvalue* value);
int xideReadingRestore(xide_app* app,const xvalue* value,int afterLayout);
void xideReadingReset(xide_app* app);
uint64_t xideReadingFingerprint(xide_app* app,uint64_t seed);
#endif
