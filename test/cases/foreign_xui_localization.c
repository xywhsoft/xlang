#include "foreign_xui_localization.h"
#include <assert.h>
#include <stdio.h>
int fixtureLocaleRemove(xui_language language)
{bool removed=false;int status=xuiLanguageRollbackCreate(language,&removed);assert(status!=XUI_OK || removed);return status;}
/* The fixture creates fewer than 128 custom IDs, including failed attempts.
 * Use public lookup instead of exposing or depending on Context layout. */
unsigned int fixtureLocalePackCount(xui_context context)
{
    unsigned count=0;
    for(int i=0;i<XUI_LANGUAGE_COUNT;++i)count+=xuiGetLanguagePack(context,i)!=NULL;
    for(int i=XUI_LANGUAGE_CUSTOM_BASE;i<XUI_LANGUAGE_CUSTOM_BASE+128;++i)
        count+=xuiGetLanguagePack(context,i)!=NULL;
    return count;
}
static xmemdebugsnapshot before;
void fixtureLocaleBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
int fixtureLocaleLiveUnchanged(void)
{xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);return now.LiveCount==before.LiveCount && now.LiveBytes==before.LiveBytes && now.InvalidFreeCount==before.InvalidFreeCount && now.DoubleFreeCount==before.DoubleFreeCount;}
void fixtureLocaleReport(int native)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    printf("Localization heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(now.AllocCount-before.AllocCount),(unsigned long long)(now.FreeCount-before.FreeCount),
        (unsigned long long)(now.ReallocCount-before.ReallocCount));
}
void fixtureLocaleNativeHot(xui_language language)
{for(int i=0;i<1000;++i){assert(xuiGetLanguageId(language)>=0);assert(xuiLanguageGetTextCount(language)>0);}}
