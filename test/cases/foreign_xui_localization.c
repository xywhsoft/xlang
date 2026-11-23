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

void fixtureLocaleCheckNativeSlots(xui_language language, uint64_t nonemptyIndex)
{
    const xarray* slots = xuiGetLanguageTextArray(language);
    uint64_t count = xuiLanguageGetTextCount(language);
    assert(slots && count == slots->Count);
    for (uint64_t index = 0; index < count; ++index) {
        const xui_language_text_t* original = xrtArrayConstGet(slots, (size_t)index);
        xui_language_text_view_t entry = {0};
        assert(xuiLanguageGetTextAt(language, index, &entry) == XUI_OK);
        assert(entry.iTextId == original->iTextId && entry.sText == original->sText);
    }
    xui_language_text_view_t unchanged = {.iTextId = 99, .sText = NULL};
    assert(xuiLanguageGetTextAt(language, count, &unchanged) == XUI_ERROR_INVALID_ARGUMENT);
    assert(xuiLanguageGetTextAt(language, UINT64_MAX, &unchanged) == XUI_ERROR_INVALID_ARGUMENT);
    assert(xuiLanguageGetTextAt(language, 0, NULL) == XUI_ERROR_INVALID_ARGUMENT);
    assert(unchanged.iTextId == 99 && unchanged.sText == NULL);

    fixtureLocaleBegin();
    for (int i = 0; i < 128; ++i) {
        xui_language_text_view_t entry = {0};
        assert(xuiLanguageGetTextAt(language, nonemptyIndex, &entry) == XUI_OK);
        assert(entry.sText && entry.sText[0]);
    }
    fixtureLocaleReportEntries(1);
}

void fixtureLocaleReportEntries(int native)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(fixtureLocaleLiveUnchanged());
    printf("Localization indexed %s: alloc=%llu free=%llu realloc=%llu\n", native ? "native" : "language",
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

bool fixtureLocaleAllocationFailed(void)
{
    return xrtMemDebugFailTriggered();
}
