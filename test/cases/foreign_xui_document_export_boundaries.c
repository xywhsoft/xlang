#include "foreign_xui_document_export_boundaries.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xmemdebugsnapshot budget_before, hot_before;
static uint64_t reference_allocs, reference_frees, reference_reallocs;

xui_doc_range_t fixtureDocumentFirstBlock(xui_document_snapshot snapshot)
{
    xui_doc_range_t range;
    memset(&range, 0, sizeof(range));
    range.tAnchor.iSize = range.tCaret.iSize = sizeof(range.tAnchor);
    range.tAnchor.iDocumentId = range.tCaret.iDocumentId = xuiDocumentSnapshotGetIdentity(snapshot);
    range.tAnchor.iRevision = range.tCaret.iRevision = xuiDocumentSnapshotGetRevision(snapshot);
    range.tAnchor.iKind = range.tCaret.iKind = XUI_DOC_POSITION_GAP;
    range.tAnchor.iNodeId = range.tCaret.iNodeId = XUI_DOCUMENT_ROOT;
    range.tCaret.iOffset = 1;
    return range;
}

void fixtureDocumentExportBudget(int budget)
{
    assert(xrtMemDebugEnabled());
    xrtMemDebugSnapshot(&budget_before);
    if (budget >= 0) assert(xrtMemDebugFailAfter((uint64_t)budget));
}
void fixtureDocumentExportBudgetEnd(void)
{
    xrtMemDebugFailClear();
    xrtClearError();
}
void fixtureDocumentExportBalanced(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == budget_before.LiveCount && after.LiveBytes == budget_before.LiveBytes);
    assert(after.InvalidFreeCount == budget_before.InvalidFreeCount && after.DoubleFreeCount == budget_before.DoubleFreeCount);
}
void fixtureDocumentExportHotBegin(void) { xrtMemDebugSnapshot(&hot_before); }
void fixtureDocumentExportHotEnd(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount - hot_before.AllocCount == reference_allocs);
    assert(after.FreeCount - hot_before.FreeCount == reference_frees);
    assert(after.ReallocCount - hot_before.ReallocCount == reference_reallocs);
    assert(after.LiveCount == hot_before.LiveCount && after.LiveBytes == hot_before.LiveBytes);
    printf("Document language text: %llu allocations, %llu frees, %llu reallocations; equal native workload\n",
        (unsigned long long)reference_allocs, (unsigned long long)reference_frees,
        (unsigned long long)reference_reallocs);
}
void fixtureDocumentExportNative(xui_document_snapshot snapshot,
    xui_document_fragment fragment, xui_doc_range_t range, uint64_t table)
{
    xmemdebugsnapshot after;
    fixtureDocumentExportHotBegin();
    for (unsigned i = 0; i < 100; ++i) {
        char* data = NULL; uint64_t size = 0;
        assert(xuiDocumentExportHtml(snapshot, &data, &size) == XUI_OK && size);
        xuiDocumentFreeBuffer(data);
        assert(xuiDocumentSnapshotCopyPlainText(snapshot, &data, &size) == XUI_OK && size);
        xuiDocumentFreeBuffer(data);
        assert(xuiDocumentSnapshotCopyRange(snapshot, &range, &data, &size) == XUI_OK && size);
        xuiDocumentFreeBuffer(data);
        assert(xuiDocumentSnapshotCopyTableMatrix(snapshot, table, 0, 0, 1, 2, &data, &size) == XUI_OK && size);
        xuiDocumentFreeBuffer(data);
        assert(xuiDocumentFragmentExportHtml(fragment, &data, &size) == XUI_OK && size);
        xuiDocumentFreeBuffer(data);
    }
    xrtMemDebugSnapshot(&after);
    reference_allocs = after.AllocCount - hot_before.AllocCount;
    reference_frees = after.FreeCount - hot_before.FreeCount;
    reference_reallocs = after.ReallocCount - hot_before.ReallocCount;
    assert(after.LiveCount == hot_before.LiveCount && after.LiveBytes == hot_before.LiveBytes);
    printf("Document native text: %llu allocations, %llu frees, %llu reallocations\n",
        (unsigned long long)reference_allocs, (unsigned long long)reference_frees,
        (unsigned long long)reference_reallocs);
}
