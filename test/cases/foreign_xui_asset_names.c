#include "foreign_xui_asset_names.h"
#include <assert.h>
#include <string.h>
static xmemdebugsnapshot before;
void fixtureAssetNamesBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
int fixtureAssetNamesAllocations(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.ReallocCount==before.ReallocCount);return (int)(after.AllocCount-before.AllocCount);}
int fixtureAssetNameMatches(int index,xstrview value)
{
    const char* name=xuiBuiltinAssetGetName(index);xui_rect_t byName,byIndex;
    return name && value.Data && name!=value.Data && strlen(name)==value.Size &&
        !memcmp(name,value.Data,value.Size) &&
        xuiBuiltinAssetGetRect(name,&byName)==XUI_OK && xuiBuiltinAssetGetRectByIndex(index,&byIndex)==XUI_OK &&
        byName.fX==byIndex.fX && byName.fY==byIndex.fY && byName.fW==byIndex.fW && byName.fH==byIndex.fH;
}
int fixtureVectorNameMatches(int index,xstrview value)
{
    const char* name=xuiVectorIconGetName(index);
    return name && value.Data && name!=value.Data && strlen(name)==value.Size && !memcmp(name,value.Data,value.Size);
}
void fixtureAssetNamesBudget(int budget){assert(xrtMemDebugFailAfter(budget));}
