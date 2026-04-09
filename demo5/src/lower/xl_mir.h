#ifndef XL_MIR_H
#define XL_MIR_H

#include "../common/xl_common.h"
#include "../diag/xl_diag.h"
#include "xl_hir.h"

typedef enum XL_MirKind {
	XL_MIR_PROGRAM = 0,
	XL_MIR_FUNC,
	XL_MIR_BLOCK,
	XL_MIR_BIND,
	XL_MIR_BRANCH,
	XL_MIR_FORIN,
	XL_MIR_RETURN,
	XL_MIR_BREAK,
	XL_MIR_CONTINUE,
	XL_MIR_THROW,
	XL_MIR_YIELD,
	XL_MIR_IMPORT,
	XL_MIR_EXPORT,
	XL_MIR_CALL,
	XL_MIR_VALUE
} XL_MirKind;

typedef struct XL_MirNode {
	XL_MirKind eKind;
	char* sText;
	char* sTypeName;
	int64_t iIntValue;
	double fFloatValue;
	int bFlag;
	int iLine;
	int iColumn;
	struct XL_MirNode** ppChildren;
	size_t iChildCount;
	size_t iChildCapacity;
} XL_MirNode;

XL_MirNode* xlMirCreateNode(XL_MirKind eKind, const XL_HirNode* pHir);
void xlMirSetText(XL_MirNode* pNode, const char* sText);
void xlMirSetTypeName(XL_MirNode* pNode, const char* sTypeName);
void xlMirAppendChild(XL_MirNode* pNode, XL_MirNode* pChild);
void xlMirFree(XL_MirNode* pNode);
const char* xlMirKindName(XL_MirKind eKind);

#endif
