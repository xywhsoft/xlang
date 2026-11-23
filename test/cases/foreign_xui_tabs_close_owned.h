#ifndef FOREIGN_XUI_TABS_CLOSE_OWNED_H
#define FOREIGN_XUI_TABS_CLOSE_OWNED_H

#include "xge3_runtime.h"

int fixtureTabsCpuProxy(xui_proxy* output);
int fixtureTabsInstallRawClose(xui_widget tabs);
int fixtureTabsRawCloseCount(void);
void fixtureTabsArmOom(uint64_t prefix);
bool fixtureTabsClearOom(void);

#endif
