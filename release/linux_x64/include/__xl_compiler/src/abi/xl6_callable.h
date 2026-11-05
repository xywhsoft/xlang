#ifndef XL6_CALLABLE_H
#define XL6_CALLABLE_H

#include "src/ast/xl6_ast.h"

typedef enum XL6_CallableConvention {
    XL6_CALLABLE_CONV_XCALL,
    XL6_CALLABLE_CONV_CDECL,
    XL6_CALLABLE_CONV_STDCALL
} XL6_CallableConvention;

typedef enum XL6_CallableVarargMode {
    XL6_CALLABLE_VARARG_NONE,
    XL6_CALLABLE_VARARG_ARRAY,
    XL6_CALLABLE_VARARG_DICT,
    XL6_CALLABLE_VARARG_C
} XL6_CallableVarargMode;

const char* xl6CallableConventionName(XL6_CallableConvention convention);
const char* xl6CallableVarargModeName(XL6_CallableVarargMode mode);
int xl6CallableParamIsArrayVararg(const XL6_AstNode* param);
int xl6CallableParamIsDictVararg(const XL6_AstNode* param);
int xl6CallableParamIsCVararg(const XL6_AstNode* param);
int xl6CallableParamIsVararg(const XL6_AstNode* param);
XL6_CallableVarargMode xl6CallableDetectVarargMode(const XL6_AstNode* params);
int xl6CallableFixedParamCount(const XL6_AstNode* params);
int xl6CallableRequiredParamCount(const XL6_AstNode* params);

#endif
