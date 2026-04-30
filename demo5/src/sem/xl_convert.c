#include "xl_convert.h"

#include <string.h>

static void xlConvSet(
	XL_Conversion* pOut,
	XL_ConvKind eKind,
	int iCost,
	int bRequiresRuntime,
	const char* sReason
)
{
	if ( pOut == NULL ) {
		return;
	}
	pOut->eKind = eKind;
	pOut->iCost = iCost;
	pOut->bRequiresRuntime = bRequiresRuntime;
	pOut->sReason = sReason;
}

static int xlConvIsGenericPlaceholderName(const char* sTypeName)
{
	return (sTypeName != NULL) &&
		(sTypeName[0] >= 'A') && (sTypeName[0] <= 'Z') &&
		(sTypeName[1] == '\0');
}

static int xlConvIsFunctionTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) && (strncmp(sTypeName, "function<", 9) == 0);
}

int xlConvTypeHasBaseName(const char* sTypeName, const char* sBaseName)
{
	size_t iBaseLen;

	if ( (sTypeName == NULL) || (sBaseName == NULL) || (sBaseName[0] == '\0') ) {
		return 0;
	}
	if ( strcmp(sTypeName, sBaseName) == 0 ) {
		return 1;
	}
	iBaseLen = strlen(sBaseName);
	return (strncmp(sTypeName, sBaseName, iBaseLen) == 0) && (sTypeName[iBaseLen] == '<');
}

int xlConvIsIntegerTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) &&
		((strcmp(sTypeName, "int") == 0) ||
		(strcmp(sTypeName, "int8") == 0) ||
		(strcmp(sTypeName, "uint8") == 0) ||
		(strcmp(sTypeName, "int16") == 0) ||
		(strcmp(sTypeName, "uint16") == 0) ||
		(strcmp(sTypeName, "int32") == 0) ||
		(strcmp(sTypeName, "uint32") == 0) ||
		(strcmp(sTypeName, "int64") == 0) ||
		(strcmp(sTypeName, "uint64") == 0));
}

int xlConvIsFloatTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) &&
		((strcmp(sTypeName, "float") == 0) ||
		(strcmp(sTypeName, "float32") == 0) ||
		(strcmp(sTypeName, "float64") == 0));
}

static int xlConvIsVarCompatibleTypeName(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return 0;
	}
	return (strcmp(sTypeName, "var") == 0) ||
		xlConvIsFunctionTypeName(sTypeName) ||
		(strcmp(sTypeName, "null") == 0) ||
		(strcmp(sTypeName, "bool") == 0) ||
		xlConvIsIntegerTypeName(sTypeName) ||
		xlConvIsFloatTypeName(sTypeName) ||
		(strcmp(sTypeName, "string") == 0) ||
		(strcmp(sTypeName, "object") == 0) ||
		(strcmp(sTypeName, "any") == 0) ||
		(strcmp(sTypeName, "point") == 0) ||
		(strcmp(sTypeName, "time") == 0) ||
		xlConvTypeHasBaseName(sTypeName, "future") ||
		(strcmp(sTypeName, "task") == 0) ||
		(strcmp(sTypeName, "generator") == 0) ||
		(strcmp(sTypeName, "channel") == 0) ||
		xlConvTypeHasBaseName(sTypeName, "array") ||
		xlConvTypeHasBaseName(sTypeName, "list") ||
		xlConvTypeHasBaseName(sTypeName, "set") ||
		xlConvTypeHasBaseName(sTypeName, "dict");
}

int xlConvIsDynamicTargetTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) &&
		((strcmp(sTypeName, "var") == 0) || (strcmp(sTypeName, "any") == 0));
}

int xlConvIsReferenceLikeTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) &&
		((strcmp(sTypeName, "string") == 0) ||
		(strcmp(sTypeName, "error") == 0) ||
		(strcmp(sTypeName, "point") == 0) ||
		(strcmp(sTypeName, "object") == 0) ||
		xlConvIsDynamicTargetTypeName(sTypeName) ||
		xlConvIsFunctionTypeName(sTypeName) ||
		xlConvTypeHasBaseName(sTypeName, "array") ||
		xlConvTypeHasBaseName(sTypeName, "list") ||
		xlConvTypeHasBaseName(sTypeName, "set") ||
		xlConvTypeHasBaseName(sTypeName, "dict") ||
		xlConvTypeHasBaseName(sTypeName, "future") ||
		(strcmp(sTypeName, "task") == 0) ||
		(strcmp(sTypeName, "generator") == 0) ||
		(strcmp(sTypeName, "channel") == 0));
}

int xlTypeCanConvertImplicit(const char* sExpectedType, const char* sActualType, XL_Conversion* pOut)
{
	xlConvSet(pOut, XL_CONV_NONE, 0, 0, "not convertible");

	if ( (sExpectedType == NULL) || (sExpectedType[0] == '\0') ||
		(sActualType == NULL) || (sActualType[0] == '\0') ) {
		xlConvSet(pOut, XL_CONV_EXACT, 0, 0, "unknown type accepted by legacy resolver");
		return 1;
	}
	if ( strcmp(sExpectedType, sActualType) == 0 ) {
		xlConvSet(pOut, XL_CONV_EXACT, 0, 0, "exact type");
		return 1;
	}
	if ( xlConvIsGenericPlaceholderName(sExpectedType) || xlConvIsGenericPlaceholderName(sActualType) ) {
		xlConvSet(pOut, XL_CONV_EXACT, 0, 0, "generic placeholder");
		return 1;
	}
	if ( xlConvIsDynamicTargetTypeName(sExpectedType) && xlConvIsVarCompatibleTypeName(sActualType) ) {
		xlConvSet(pOut, XL_CONV_BOX_DYNAMIC, 3, 1, "box to dynamic");
		return 1;
	}
	if ( (strcmp(sActualType, "null") == 0) && xlConvIsReferenceLikeTypeName(sExpectedType) ) {
		xlConvSet(pOut, XL_CONV_NULL_REF, 1, 0, "null to reference-like type");
		return 1;
	}
	if ( xlConvIsIntegerTypeName(sExpectedType) && xlConvIsIntegerTypeName(sActualType) &&
		((strcmp(sExpectedType, "int") == 0) || (strcmp(sActualType, "int") == 0)) ) {
		xlConvSet(pOut, XL_CONV_ALIAS, 1, 0, "integer family alias");
		return 1;
	}
	if ( xlConvIsFloatTypeName(sExpectedType) && xlConvIsFloatTypeName(sActualType) &&
		((strcmp(sExpectedType, "float") == 0) || (strcmp(sActualType, "float") == 0)) ) {
		xlConvSet(pOut, XL_CONV_ALIAS, 1, 0, "float family alias");
		return 1;
	}
	if ( xlConvIsFloatTypeName(sExpectedType) && xlConvIsIntegerTypeName(sActualType) ) {
		xlConvSet(pOut, XL_CONV_NUMERIC_WIDEN, 2, 0, "integer to float widening");
		return 1;
	}

	return 0;
}

const char* xlConvKindName(XL_ConvKind eKind)
{
	switch ( eKind ) {
		case XL_CONV_EXACT: return "exact";
		case XL_CONV_ALIAS: return "alias";
		case XL_CONV_NUMERIC_WIDEN: return "numeric_widen";
		case XL_CONV_BOX_DYNAMIC: return "box_dynamic";
		case XL_CONV_NULL_REF: return "null_ref";
		case XL_CONV_OBJECT_REF: return "object_ref";
		case XL_CONV_NONE:
		default:
			return "none";
	}
}
