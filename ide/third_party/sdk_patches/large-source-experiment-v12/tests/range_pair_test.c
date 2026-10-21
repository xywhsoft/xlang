#include "range_models.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long checks, oldCalls, newCalls;
static int failAlloc, failCopy;
#define CHECK(c) do { ++checks; if (!(c)) { fprintf(stderr, "Failure line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
static void* checkedRealloc(void* p, size_t n) { return failAlloc ? NULL : xrtRealloc(p, n); }
static int checkedCopy(xui_code_document d, int a, int b, char* out, int cap, int* len)
{
    if (failCopy) { if (cap > 1) out[0] = '!'; return XUI_ERROR; }
    return xuiCodeDocumentCopyRange(d, a, b, out, cap, len);
}
#undef xrtRealloc
#define xrtRealloc checkedRealloc
#define xuiCodeDocumentCopyRange checkedCopy
#include "old_range_model.c"
#include "new_range_model.c"
#include "range_patterns.inc"

static void same(diag_old_data* a, diag_new_data* b, const char* ta, const char* tb,
                 int start, int end, float cw, float vw)
{
    int x0, x1, y0, y1;
    diagOldVisible(a, ta, start, end, cw, vw, &x0, &x1);
    diagNewVisible(b, tb, start, end, cw, vw, &y0, &y1);
    CHECK(x0 == y0 && x1 == y1);
    diagOldVisible(a, ta, start, end, cw, vw, NULL, NULL);
    diagNewVisible(b, tb, start, end, cw, vw, NULL, NULL);
}

static void refresh(diag_old_data* a, diag_new_data* b, int start, int end,
                    const char** ta, const char** tb)
{
    CHECK(diagOldRead(a, start, end, ta) == XUI_OK);
    CHECK(diagNewRead(b, start, end, tb) == XUI_OK);
    CHECK(memcmp(*ta, *tb, (size_t)(end - start)) == 0);
}

static void warm(diag_old_data* a, diag_new_data* b, const char* ta, const char* tb, int length)
{
    same(a, b, ta, tb, 0, length, 1.0f, 79.0f);
    same(a, b, ta, tb, 0, length, 1.0f, 190.0f);
}

int main(void)
{
    diag_old_data oldData = {0};
    diag_new_data newData = {0};
    const int tabs[] = {0, 1, 4, 16, 256};
    const char *ta, *tb;
    char* large;
    int n, tab, j, start, end, actualLength, normalizedPatterns = 0;
    unsigned long long oldBefore, newBefore, avoided;
    xui_code_document alternateOld, alternateNew, originalOld, originalNew;

    for (n = 0; n < (int)(sizeof(patterns) / sizeof(patterns[0])); ++n)
    for (tab = 0; tab < 5; ++tab) {
        oldData.iTabColumns = newData.iTabColumns = tabs[tab];
        for (start = 0; start <= sizes[n]; ++start)
        for (end = start; end <= sizes[n]; ++end)
        for (j = 0; j < 12; ++j) {
            oldData.fScrollX = newData.fScrollX = (float)(j * j * 17);
            same(&oldData, &newData, patterns[n], patterns[n], start, end, 1.0f, 79.0f);
            same(&oldData, &newData, patterns[n], patterns[n], start, end, 7.3f, 701.5f);
        }
    }
    same(&oldData, &newData, "abc", "abc", 0, 3, 0, 10);
    same(&oldData, &newData, "abc", "abc", 0, 3, 1, 0);
    same(&oldData, &newData, NULL, NULL, 0, 3, 1, 10);
    diagOldVisible(NULL, "abc", 0, 3, 1, 10, &start, &end);
    diagNewVisible(NULL, "abc", 0, 3, 1, 10, &n, &j);
    CHECK(start == n && end == j);
    CHECK(xuiCodeDocumentCreate(&oldData.pDocument) == XUI_OK);
    CHECK(xuiCodeDocumentCreate(&newData.pDocument) == XUI_OK);
    large = (char*)malloc(65538);
    CHECK(large != NULL);
    memset(large, 'a', 65537);
    large[65537] = 0;
    CHECK(xuiCodeDocumentSetText(oldData.pDocument, large) == XUI_OK);
    CHECK(xuiCodeDocumentSetText(newData.pDocument, large) == XUI_OK);
    refresh(&oldData, &newData, 0, 65537, &ta, &tb);
    oldData.iTabColumns = newData.iTabColumns = 4;
    oldData.fScrollX = newData.fScrollX = 60000;
    warm(&oldData, &newData, ta, tb, 65537);
    CHECK(newData.bVisibleRangeCached && newData.bVisibleRangeSecondCached);
    oldBefore = oldCalls;
    newBefore = newCalls;
    for (j = 0; j < 32; ++j) warm(&oldData, &newData, ta, tb, 65537);
    avoided = oldCalls - oldBefore;
    CHECK(avoided > 3800000);
    CHECK(newCalls == newBefore);

    /* Borrowed text is traversed, but cannot overwrite either owned result. */
    same(&oldData, &newData, "outside\t中文 é", "outside\t中文 é", 0,
         (int)strlen("outside\t中文 é"), 1.0f, 120.0f);
    newBefore = newCalls;
    warm(&oldData, &newData, ta, tb, 65537);
    CHECK(newCalls == newBefore);
    newBefore = newCalls;
    same(&oldData, &newData, ta, tb, 0, 65537, 1.0f, 220.0f);
    CHECK(newCalls > newBefore);
    newBefore = newCalls;
    same(&oldData, &newData, ta, tb, 0, 65537, 1.0f, 79.0f);
    CHECK(newCalls > newBefore);
    oldData.fScrollX = newData.fScrollX = 30000;
    newBefore = newCalls;
    warm(&oldData, &newData, ta, tb, 65537);
    CHECK(newCalls > newBefore);
    oldData.iTabColumns = newData.iTabColumns = 8;
    newBefore = newCalls;
    warm(&oldData, &newData, ta, tb, 65537);
    CHECK(newCalls > newBefore);
    CHECK(xuiCodeDocumentReplace(oldData.pDocument, 0, 1, "\t") == XUI_OK);
    CHECK(xuiCodeDocumentReplace(newData.pDocument, 0, 1, "\t") == XUI_OK);
    refresh(&oldData, &newData, 0, 65537, &ta, &tb);
    CHECK(!oldData.bVisibleRangeCached && !newData.bVisibleRangeCached);
    newBefore = newCalls;
    warm(&oldData, &newData, ta, tb, 65537);
    CHECK(newCalls > newBefore);
    refresh(&oldData, &newData, 1, 65537, &ta, &tb);
    CHECK(!oldData.bVisibleRangeCached && !newData.bVisibleRangeCached);
    warm(&oldData, &newData, ta, tb, 65536);

    failCopy = 1;
    CHECK(diagOldRead(&oldData, 0, 65537, &ta) != XUI_OK);
    CHECK(diagNewRead(&newData, 0, 65537, &tb) != XUI_OK);
    CHECK(!oldData.bVisibleRangeCached && !newData.bVisibleRangeCached);
    failCopy = 0;
    oldData.iReadVersion = newData.iReadVersion = 0;
    refresh(&oldData, &newData, 0, 65537, &ta, &tb);
    warm(&oldData, &newData, ta, tb, 65537);
    oldData.iReadBufferCapacity = newData.iReadBufferCapacity = 0;
    failAlloc = 1;
    CHECK(diagOldRead(&oldData, 1, 65537, &ta) == XUI_ERROR_OUT_OF_MEMORY);
    CHECK(diagNewRead(&newData, 1, 65537, &tb) == XUI_ERROR_OUT_OF_MEMORY);
    CHECK(!oldData.bVisibleRangeCached && !newData.bVisibleRangeCached);
    failAlloc = 0;
    oldData.iReadBufferCapacity = newData.iReadBufferCapacity = 65538;
    refresh(&oldData, &newData, 0, 65537, &ta, &tb);
    warm(&oldData, &newData, ta, tb, 65537);

    /* Different live document identity with equal versions, ranges and bytes. */
    CHECK(xuiCodeDocumentCreate(&alternateOld) == XUI_OK);
    CHECK(xuiCodeDocumentCreate(&alternateNew) == XUI_OK);
    CHECK(xuiCodeDocumentSetText(alternateOld, large) == XUI_OK);
    CHECK(xuiCodeDocumentSetText(alternateNew, large) == XUI_OK);
    CHECK(xuiCodeDocumentReplace(alternateOld, 0, 1, "\t") == XUI_OK);
    CHECK(xuiCodeDocumentReplace(alternateNew, 0, 1, "\t") == XUI_OK);
    CHECK(xuiCodeDocumentGetVersion(alternateOld) == oldData.iReadVersion);
    CHECK(xuiCodeDocumentGetVersion(alternateNew) == newData.iReadVersion);
    originalOld = oldData.pDocument;
    originalNew = newData.pDocument;
    oldData.pDocument = alternateOld;
    newData.pDocument = alternateNew;
    newBefore = newCalls;
    warm(&oldData, &newData, ta, tb, 65537);
    CHECK(newCalls > newBefore);
    newBefore = newCalls;
    warm(&oldData, &newData, ta, tb, 65537);
    CHECK(newCalls == newBefore);
    xuiCodeDocumentDestroy(originalOld);
    xuiCodeDocumentDestroy(originalNew);

    /* Repeated owned Unicode windows exercise both slots with real documents. */
    for (n = 0; n < validPatternCount; ++n) {
        CHECK(xuiCodeDocumentSetText(oldData.pDocument, patterns[n]) == XUI_OK);
        CHECK(xuiCodeDocumentSetText(newData.pDocument, patterns[n]) == XUI_OK);
        actualLength = xuiCodeDocumentGetLength(oldData.pDocument);
        CHECK(actualLength == xuiCodeDocumentGetLength(newData.pDocument));
        if (actualLength != sizes[n]) ++normalizedPatterns;
        refresh(&oldData, &newData, 0, actualLength, &ta, &tb);
        for (tab = 0; tab < 5; ++tab) {
            oldData.iTabColumns = newData.iTabColumns = tabs[tab];
            for (j = 0; j < 12; ++j) {
                oldData.fScrollX = newData.fScrollX = (float)(j * j * 17);
                warm(&oldData, &newData, ta, tb, actualLength);
                newBefore = newCalls;
                warm(&oldData, &newData, ta, tb, actualLength);
                CHECK(newCalls == newBefore);
            }
        }
    }
    CHECK(normalizedPatterns == 1);
    xuiCodeDocumentDestroy(oldData.pDocument);
    xuiCodeDocumentDestroy(newData.pDocument);
    xrtFree(oldData.sReadBuffer);
    xrtFree(newData.sReadBuffer);
    free(large);
    printf("viewport pair comparison passed: %llu assertions; %llu old repeat grapheme calls; 0 new repeat grapheme calls; %zu additional private widget bytes; %d normalized owned patterns\n",
           checks, avoided, sizeof(diag_new_data) - sizeof(diag_old_data), normalizedPatterns);
    return 0;
}
