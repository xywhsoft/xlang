#ifndef FOREIGN_XGE_BACKEND_INFO_H
#define FOREIGN_XGE_BACKEND_INFO_H

#include <xge3_runtime.h>

void backendInstallLegacy(void);
void backendChangeLegacyName(void);
void backendCheckOtherThread(xge_platform_backend_ref platform,xge_graphics_backend_ref graphics);
int backendWindowMatches(void* window);
void backendCheckWindowAbi(void);
void backendFailNextAllocation(void);
int backendClearFailure(void);
void backendBeginMeasure(void);
void backendEndMeasure(int language);
void backendMeasureNative(void);

#endif
