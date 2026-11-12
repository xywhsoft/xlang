#ifndef FOREIGN_XUI_DOCUMENT_EXPORT_BOUNDARIES_H
#define FOREIGN_XUI_DOCUMENT_EXPORT_BOUNDARIES_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
xui_doc_range_t fixtureDocumentFirstBlock(xui_document_snapshot snapshot);
void fixtureDocumentExportBudget(int budget);
void fixtureDocumentExportBudgetEnd(void);
void fixtureDocumentExportBalanced(void);
void fixtureDocumentExportNative(xui_document_snapshot snapshot,
    xui_document_fragment fragment, xui_doc_range_t range, uint64_t table);
void fixtureDocumentExportHotBegin(void);
void fixtureDocumentExportHotEnd(void);
#ifdef __cplusplus
}
#endif
#endif
