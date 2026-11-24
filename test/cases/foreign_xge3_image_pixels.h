#ifndef FOREIGN_XGE3_IMAGE_PIXELS_H
#define FOREIGN_XGE3_IMAGE_PIXELS_H
#include <xge3_runtime.h>
void fixturePixelBudget(int allowance);
void fixturePixelBegin(void);
void fixturePixelReport(int label);
void fixturePixelFormat(xge_image image,int unsupported);
void fixturePixelNativeCopy(const xge_image_t* image,void* output,size_t size);
void fixturePixelNativeBorrow(const xge_image_t* image);
void fixturePixelNativeVisit(const xge_image_t* image);
#endif
