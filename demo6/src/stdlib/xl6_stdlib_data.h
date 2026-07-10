#ifndef XL6_STDLIB_DATA_H
#define XL6_STDLIB_DATA_H

#include <stddef.h>

#include "src/ast/xl6_ast.h"

#define XL6_STDLIB_CONST_COUNT 46
#define XL6_STDLIB_FUNC_COUNT 738

typedef struct XL6_StdlibParam {
    const char* type;
    const char* name;
} XL6_StdlibParam;

typedef struct XL6_StdlibFunc {
    const char* ns;
    const char* name;
    const char* internalName;
    const char* cSymbol;
    const XL6_StdlibParam* params;
    size_t paramCount;
    const char* returnType;
    const char* sourceBody;
} XL6_StdlibFunc;

typedef struct XL6_StdlibConst {
    const char* ns;
    const char* name;
    XL6_AstKind kind;
    const char* value;
} XL6_StdlibConst;

extern const char* xl6StdlibCoreClassSource;
extern const char* xl6StdlibRandomClassSource;
extern const char* xl6StdlibBufferClassSource;
extern const char* xl6StdlibLoggerClassSource;
extern const char* xl6StdlibTemplateClassSource;
extern const char* xl6StdlibCryptoClassSource;
extern const char* xl6StdlibHttpServerClassSource;
extern const char* xl6StdlibHttpClientClassSource;
extern const char* xl6StdlibNetClassSource;
extern const char* xl6StdlibFileInfoClassSource;
extern const char* xl6StdlibFileClassSource;
extern const char* xl6StdlibRegexClassSource;
extern const char* xl6StdlibProcessClassSource;
extern const char* xl6StdlibTccClassSource;
extern const char* xl6StdlibXlangClassSource;

extern const XL6_StdlibConst xl6StdlibConsts[];
extern const XL6_StdlibFunc xl6StdlibFuncs[];
extern const size_t xl6StdlibConstCount;
extern const size_t xl6StdlibFuncCount;

#endif
