#ifndef XL_MODULE_LINK_H
#define XL_MODULE_LINK_H

#include "../common/xl_common.h"
#include "../ast/xl_ast.h"
#include "../diag/xl_diag.h"

typedef struct XL_ModuleLinkResult {
	XL_AstNode* pLinkedRoot;
	char* sTextDump;
	char* sJsonDump;
} XL_ModuleLinkResult;

void xlModuleLinkResultInit(XL_ModuleLinkResult* pResult);
void xlModuleLinkResultFree(XL_ModuleLinkResult* pResult);
int xlModuleLinkFromFile(const char* sInputFile, XL_DiagContext* pDiag, int bTraceParser, XL_ModuleLinkResult* pResult);

#endif
