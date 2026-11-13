#ifndef FOREIGN_XUI_LOCALIZATION_H
#define FOREIGN_XUI_LOCALIZATION_H
#include <xge3_runtime.h>
int fixtureLocaleRemove(xui_language language);
unsigned int fixtureLocalePackCount(xui_context context);
void fixtureLocaleBegin(void);
int fixtureLocaleLiveUnchanged(void);
void fixtureLocaleReport(int native);
void fixtureLocaleNativeHot(xui_language language);
#endif
