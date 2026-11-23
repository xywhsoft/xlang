#ifndef FOREIGN_XUI_EVENT_VISIT_H
#define FOREIGN_XUI_EVENT_VISIT_H

#include <xge3_runtime.h>

void fixtureEventEnableTracking(void);
void fixtureEventBeginSample(void);
void fixtureEventEndSample(int language);
void fixtureEventNativeSample(xui_context context);

#endif
