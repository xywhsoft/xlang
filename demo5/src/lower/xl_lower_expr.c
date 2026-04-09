#include "xl_lower.h"

static XL_HirKind __xlExprKind(const XL_AstNode* pNode)
{
	switch ( pNode->eKind ) {
		case XL_AST_CALL:
			return XL_HIR_CALL;
		case XL_AST_MEMBER:
			return XL_HIR_MEMBER;
		case XL_AST_INDEX:
			return XL_HIR_INDEX;
		case XL_AST_BINARY:
			return XL_HIR_BINARY;
		case XL_AST_UNARY:
			return XL_HIR_UNARY;
		case XL_AST_IDENT:
			return XL_HIR_NAME;
		default:
			return XL_HIR_LITERAL;
	}
}

XL_HirNode* xlLowerExprToHir(const XL_AstNode* pNode, XL_DiagContext* pDiag, int bTraceLower)
{
	XL_HirNode* pHir;
	XL_HirNode* pArgs;
	size_t i;

	XL_UNUSED(bTraceLower);
	XL_UNUSED(pDiag);

	if ( pNode == NULL ) {
		return NULL;
	}

	pHir = xlHirCreateNode(__xlExprKind(pNode), pNode);
	if ( pHir == NULL ) {
		return NULL;
	}

	xlHirSetText(pHir, pNode->sText);
	switch ( pNode->eKind ) {
		case XL_AST_INT:
			xlHirSetTypeName(pHir, "int");
			break;
		case XL_AST_FLOAT:
			xlHirSetTypeName(pHir, "float");
			break;
		case XL_AST_STRING:
		case XL_AST_TEMPLATE:
			xlHirSetTypeName(pHir, "string");
			break;
		case XL_AST_BOOL:
			xlHirSetTypeName(pHir, "bool");
			break;
		default:
			break;
	}
	if ( pNode->eKind == XL_AST_INT ) {
		pHir->iIntValue = pNode->iIntValue;
	} else if ( pNode->eKind == XL_AST_FLOAT ) {
		pHir->fFloatValue = pNode->fFloatValue;
	} else {
		pHir->bFlag = pNode->bFlag;
	}

	if ( pNode->eKind == XL_AST_CALL ) {
		xlHirAppendChild(pHir, xlLowerExprToHir(pNode->ppChildren[0], pDiag, bTraceLower));

		pArgs = xlHirCreateNode(XL_HIR_BLOCK, pNode);
		if ( pArgs != NULL ) {
			xlHirSetText(pArgs, "args");
			for ( i = 0; (pNode->ppChildren[1] != NULL) && (i < pNode->ppChildren[1]->iChildCount); ++i ) {
				xlHirAppendChild(pArgs, xlLowerExprToHir(pNode->ppChildren[1]->ppChildren[i], pDiag, bTraceLower));
			}
			xlHirAppendChild(pHir, pArgs);
		}
		return pHir;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlHirAppendChild(pHir, xlLowerExprToHir(pNode->ppChildren[i], pDiag, bTraceLower));
	}

	return pHir;
}
