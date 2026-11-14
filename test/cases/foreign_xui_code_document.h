#ifndef FOREIGN_XUI_CODE_DOCUMENT_H
#define FOREIGN_XUI_CODE_DOCUMENT_H
#include "xge3_runtime.h"
void fixtureDocumentBudget(int allowance);
void fixtureDocumentHeapBegin(void);
void fixtureDocumentHeapReport(int native);
void fixtureDocumentNativeSetup(xui_code_document document);
void fixtureDocumentNativeHot(xui_code_document document);
void fixtureDocumentNativeClear(xui_code_document document);
int fixtureDocumentUtf8(void);
#endif
