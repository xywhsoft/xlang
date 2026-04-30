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
	XL_MIR_TRY,
	XL_MIR_WHILE,
	XL_MIR_FORIN,
	XL_MIR_RETURN,
	XL_MIR_BREAK,
	XL_MIR_CONTINUE,
	XL_MIR_THROW,
	XL_MIR_ASSERT,
	XL_MIR_YIELD,
	XL_MIR_IMPORT,
	XL_MIR_EXPORT,
	XL_MIR_CALL,
	XL_MIR_MEMBER,
	XL_MIR_VALUE
} XL_MirKind;

#define XL_MIR_FLAG_STORAGE_VAR 0x10000

typedef struct XL_MirNode {
	XL_MirKind eKind;
	char* sText;
	char* sAbiName;
	char* sExternName;
	char* sTypeName;
	char* sConversion;
	int iScopeId;
	int iSymbolIndex;
	int iFuncIndex;
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
void xlMirSetAbiName(XL_MirNode* pNode, const char* sAbiName);
void xlMirSetExternName(XL_MirNode* pNode, const char* sExternName);
void xlMirSetTypeName(XL_MirNode* pNode, const char* sTypeName);
void xlMirSetConversion(XL_MirNode* pNode, const char* sConversion);
void xlMirAppendChild(XL_MirNode* pNode, XL_MirNode* pChild);
void xlMirFree(XL_MirNode* pNode);
const char* xlMirKindName(XL_MirKind eKind);

#endif
