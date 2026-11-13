#ifndef FOREIGN_XUI_ASSET_NAMES_H
#define FOREIGN_XUI_ASSET_NAMES_H
#include "xge3_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
void fixtureAssetNamesBegin(void);
int fixtureAssetNamesAllocations(void);
int fixtureAssetNameMatches(int index,xstrview value);
int fixtureVectorNameMatches(int index,xstrview value);
void fixtureAssetNamesBudget(int budget);
#ifdef __cplusplus
}
#endif
#endif
