#ifndef FOREIGN_XGE3_IMAGE_ALPHA_H
#define FOREIGN_XGE3_IMAGE_ALPHA_H
#include <xge3_runtime.h>
int fixtureAlphaPixels(xge_image image,int converted);
void fixtureAlphaInvalid(xge_image image,int invalid);
void fixtureAlphaBudget(int allowance);
void fixtureAlphaHeapBegin(void);
void fixtureAlphaHeapReport(int native);
void fixtureAlphaNativePerformance(xge_image image);
#endif
