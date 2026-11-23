#ifndef FOREIGN_XUI_EXTERNAL_DRAG_H
#define FOREIGN_XUI_EXTERNAL_DRAG_H

#include <xge3_runtime.h>

void fixtureExternalDragEnableTracking(void);
void fixtureExternalDragBeginSample(void);
void fixtureExternalDragEndSample(int language);
void fixtureExternalDragNativeSample(xui_context context, xui_data_object data);

#endif
