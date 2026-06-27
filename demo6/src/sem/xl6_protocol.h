#ifndef XL6_PROTOCOL_H
#define XL6_PROTOCOL_H

typedef enum XL6_ProtocolKind {
    XL6_PROTOCOL_NONE = 0,
    XL6_PROTOCOL_CREATE,
    XL6_PROTOCOL_DESTROY,
    XL6_PROTOCOL_TYPE_NAME,
    XL6_PROTOCOL_TO_STRING,
    XL6_PROTOCOL_HASH,
    XL6_PROTOCOL_EQUALS,
    XL6_PROTOCOL_COPY,
    XL6_PROTOCOL_CLONE,
    XL6_PROTOCOL_MOVE,
    XL6_PROTOCOL_COPYABLE,
    XL6_PROTOCOL_TO_VALUE,
    XL6_PROTOCOL_FROM_VALUE,
    XL6_PROTOCOL_ITEM,
    XL6_PROTOCOL_SET_ITEM,
    XL6_PROTOCOL_FIELD,
    XL6_PROTOCOL_SET_FIELD,
    XL6_PROTOCOL_CALL_FIELD,
    XL6_PROTOCOL_ADD,
    XL6_PROTOCOL_SUB,
    XL6_PROTOCOL_MUL,
    XL6_PROTOCOL_DIV,
    XL6_PROTOCOL_MOD,
    XL6_PROTOCOL_POW,
    XL6_PROTOCOL_NEG,
    XL6_PROTOCOL_POS,
    XL6_PROTOCOL_NOT,
    XL6_PROTOCOL_BIT_NOT,
    XL6_PROTOCOL_EQ,
    XL6_PROTOCOL_NE,
    XL6_PROTOCOL_LT,
    XL6_PROTOCOL_LE,
    XL6_PROTOCOL_GT,
    XL6_PROTOCOL_GE,
    XL6_PROTOCOL_APPROX,
    XL6_PROTOCOL_AND,
    XL6_PROTOCOL_OR,
    XL6_PROTOCOL_LEN,
    XL6_PROTOCOL_CONTAINS,
    XL6_PROTOCOL_ITER,
    XL6_PROTOCOL_NEXT,
    XL6_PROTOCOL_CALL
} XL6_ProtocolKind;

typedef struct XL6_ProtocolRule {
    const char* name;
    XL6_ProtocolKind kind;
    int minParams;
    int maxParams;
    int minReturns;
    int maxReturns;
    int allowVararg;
    const char* firstParamType;
    const char* firstReturnType;
} XL6_ProtocolRule;

const XL6_ProtocolRule* xl6ProtocolFindRule(const char* name);
int xl6ProtocolLooksReservedName(const char* name);
const char* xl6ProtocolKindName(XL6_ProtocolKind kind);

#endif
