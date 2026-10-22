#ifndef XL6_SEMANTIC_PLAN_H
#define XL6_SEMANTIC_PLAN_H

/*
 * 语义阶段只做一次决议，XLIR lowering 只能消费这里记录的结果。
 * 这些枚举只描述语言语义，不包含任何 C 代码生成细节。
 */

typedef enum XL6_SemCallKind {
    XL6_SEM_CALL_NONE = 0,
    XL6_SEM_CALL_DIRECT,
    XL6_SEM_CALL_GENERIC_DIRECT,
    XL6_SEM_CALL_FUNCTION_VALUE,
    XL6_SEM_CALL_DYNAMIC_FUNCTION,
    /* Runtime member lookup/invocation on an erased `var` receiver.  This is
     * deliberately distinct from invoking a dynamic value stored in a name or
     * field: the member name and receiver are part of the call contract. */
    XL6_SEM_CALL_DYNAMIC_METHOD,
    XL6_SEM_CALL_CONSTRUCTOR,
    XL6_SEM_CALL_METHOD,
    XL6_SEM_CALL_STATIC_METHOD,
    XL6_SEM_CALL_PROTOCOL,
    XL6_SEM_CALL_WITNESS,
    XL6_SEM_CALL_PROPERTY_GET,
    XL6_SEM_CALL_PROPERTY_SET,
    XL6_SEM_CALL_ENUM_VARIANT,
    XL6_SEM_CALL_BUILTIN,
    XL6_SEM_CALL_RUNTIME_INTRINSIC,
    XL6_SEM_CALL_BASE_CONSTRUCTOR,
    XL6_SEM_CALL_C_OWNED_TAKE
} XL6_SemCallKind;

typedef enum XL6_SemLvalueKind {
    XL6_SEM_LVALUE_NONE = 0,
    XL6_SEM_LVALUE_LOCAL,
    XL6_SEM_LVALUE_GLOBAL,
    XL6_SEM_LVALUE_FIELD,
    XL6_SEM_LVALUE_PROPERTY,
    XL6_SEM_LVALUE_INDEX,
    XL6_SEM_LVALUE_DYNAMIC_MEMBER,
    XL6_SEM_LVALUE_DISCARD
} XL6_SemLvalueKind;

typedef enum XL6_SemOwnershipAction {
    XL6_SEM_OWNERSHIP_NONE = 0,
    XL6_SEM_OWNERSHIP_COPY,
    XL6_SEM_OWNERSHIP_BORROW,
    XL6_SEM_OWNERSHIP_MOVE,
    XL6_SEM_OWNERSHIP_SHARE
} XL6_SemOwnershipAction;

typedef enum XL6_SemExitKind {
    XL6_SEM_EXIT_NONE = 0,
    XL6_SEM_EXIT_RETURN,
    XL6_SEM_EXIT_BREAK,
    XL6_SEM_EXIT_CONTINUE,
    XL6_SEM_EXIT_THROW
} XL6_SemExitKind;

#endif
