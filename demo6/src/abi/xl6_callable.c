#include "src/abi/xl6_callable.h"

#include "src/type/xl6_type_model.h"

const char* xl6CallableConventionName(XL6_CallableConvention convention)
{
    switch ( convention ) {
    case XL6_CALLABLE_CONV_XCALL: return "xcall";
    case XL6_CALLABLE_CONV_STDCALL: return "stdcall";
    case XL6_CALLABLE_CONV_CDECL:
    default:
        return "cdecl";
    }
}

const char* xl6CallableVarargModeName(XL6_CallableVarargMode mode)
{
    switch ( mode ) {
    case XL6_CALLABLE_VARARG_ARRAY: return "array";
    case XL6_CALLABLE_VARARG_DICT: return "dict";
    case XL6_CALLABLE_VARARG_NONE:
    default:
        return "none";
    }
}

int xl6CallableParamIsArrayVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6TypeTextIsArrayVararg(param->children[0]->text);
}

int xl6CallableParamIsDictVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6TypeTextIsDictVararg(param->children[0]->text);
}

int xl6CallableParamIsVararg(const XL6_AstNode* param)
{
    return xl6CallableParamIsArrayVararg(param) || xl6CallableParamIsDictVararg(param);
}

XL6_CallableVarargMode xl6CallableDetectVarargMode(const XL6_AstNode* params)
{
    int fixedCount;
    const XL6_AstNode* param;

    fixedCount = xl6CallableFixedParamCount(params);
    if ( params == NULL || params->childCount <= (size_t)fixedCount ) {
        return XL6_CALLABLE_VARARG_NONE;
    }
    param = params->children[fixedCount];
    if ( xl6CallableParamIsArrayVararg(param) ) {
        return XL6_CALLABLE_VARARG_ARRAY;
    }
    if ( xl6CallableParamIsDictVararg(param) ) {
        return XL6_CALLABLE_VARARG_DICT;
    }
    return XL6_CALLABLE_VARARG_NONE;
}

int xl6CallableFixedParamCount(const XL6_AstNode* params)
{
    int count;

    count = 0;
    if ( params == NULL ) {
        return 0;
    }
    while ( (size_t)count < params->childCount && !xl6CallableParamIsVararg(params->children[count]) ) {
        ++count;
    }
    return count;
}

int xl6CallableRequiredParamCount(const XL6_AstNode* params)
{
    int count;

    count = 0;
    if ( params == NULL ) {
        return 0;
    }
    while ( (size_t)count < params->childCount && !xl6CallableParamIsVararg(params->children[count]) ) {
        if ( params->children[count] != NULL && params->children[count]->childCount > 1 ) {
            break;
        }
        ++count;
    }
    return count;
}
