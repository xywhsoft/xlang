#ifndef XL_HIR_H
#define XL_HIR_H

#include "../common/xl_common.h"
#include "../ast/xl_ast.h"
#include "../diag/xl_diag.h"

typedef enum XL_HirKind {
	XL_HIR_PROGRAM = 0,
	XL_HIR_FUNC,
	XL_HIR_PARAM,
	XL_HIR_BLOCK,
	XL_HIR_CONST,
	XL_HIR_VAR,
	XL_HIR_IF,
	XL_HIR_FORIN,
	XL_HIR_RETURN,
	XL_HIR_BREAK,
	XL_HIR_CONTINUE,
	XL_HIR_THROW,
	XL_HIR_YIELD,
	XL_HIR_IMPORT,
	XL_HIR_EXPORT,
	XL_HIR_CALL,
	XL_HIR_MEMBER,
	XL_HIR_INDEX,
	XL_HIR_BINARY,
	XL_HIR_UNARY,
	XL_HIR_NAME,
	XL_HIR_LITERAL
} XL_HirKind;

typedef struct XL_HirNode {
	XL_HirKind eKind;
	char* sText;
	char* sTypeName;
	int64_t iIntValue;
	double fFloatValue;
	int bFlag;
	int iLine;
	int iColumn;
	struct XL_HirNode** ppChildren;
	size_t iChildCount;
	size_t iChildCapacity;
} XL_HirNode;

typedef struct XL_LowerResult XL_LowerResult;

XL_HirNode* xlHirCreateNode(XL_HirKind eKind, const XL_AstNode* pAst);
void xlHirSetText(XL_HirNode* pNode, const char* sText);
void xlHirSetTypeName(XL_HirNode* pNode, const char* sTypeName);
void xlHirAppendChild(XL_HirNode* pNode, XL_HirNode* pChild);
void xlHirFree(XL_HirNode* pNode);
const char* xlHirKindName(XL_HirKind eKind);

#endif
