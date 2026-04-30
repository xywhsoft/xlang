#ifndef XL_CONVERT_H
#define XL_CONVERT_H

typedef enum XL_ConvKind {
	XL_CONV_NONE = 0,
	XL_CONV_EXACT,
	XL_CONV_ALIAS,
	XL_CONV_NUMERIC_WIDEN,
	XL_CONV_BOX_DYNAMIC,
	XL_CONV_NULL_REF,
	XL_CONV_OBJECT_REF
} XL_ConvKind;

typedef struct XL_Conversion {
	XL_ConvKind eKind;
	int iCost;
	int bRequiresRuntime;
	const char* sReason;
} XL_Conversion;

int xlConvIsIntegerTypeName(const char* sTypeName);
int xlConvIsFloatTypeName(const char* sTypeName);
int xlConvIsDynamicTargetTypeName(const char* sTypeName);
int xlConvIsReferenceLikeTypeName(const char* sTypeName);
int xlConvTypeHasBaseName(const char* sTypeName, const char* sBaseName);
int xlTypeCanConvertImplicit(const char* sExpectedType, const char* sActualType, XL_Conversion* pOut);
const char* xlConvKindName(XL_ConvKind eKind);

#endif
