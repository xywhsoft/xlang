#ifndef XL6_CONSTRUCTOR_FLOW_H
#define XL6_CONSTRUCTOR_FLOW_H

#include "src/ast/xl6_ast.h"

/* A two-point may domain, independently quantified over every required field.
 * The conjunction proves complete construction; no path enumeration, runtime
 * flag, AST mutation, or allocation is needed by the solver. */
typedef uintptr_t XL6_ConstructorNode;
enum {
    XL6_CONSTRUCTOR_FIELD = 1u,
    XL6_CONSTRUCTOR_DIRECT_FIELD = 2u,
    XL6_CONSTRUCTOR_SELF = 4u,
    XL6_CONSTRUCTOR_MAY_RAISE = 8u,
    XL6_CONSTRUCTOR_BASE_CALL = 16u,
    XL6_CONSTRUCTOR_BASE_INITIALIZES = 32u,
    XL6_CONSTRUCTOR_BASE_EXPOSES = 64u
};
typedef struct XL6_ConstructorView {
    void* context;
    XL6_AstKind (*kind)(void*, XL6_ConstructorNode);
    size_t (*count)(void*, XL6_ConstructorNode);
    XL6_ConstructorNode (*child)(void*, XL6_ConstructorNode, size_t);
    const char* (*text)(void*, XL6_ConstructorNode);
    unsigned (*facts)(void*, XL6_ConstructorNode);
    void (*error)(void*, XL6_ConstructorNode, const char*);
    size_t maxVisits;
} XL6_ConstructorView;

/* entryInitialized is false for a pending slot. Expressions are checked with
 * requireNormalExit=false (declaration defaults execute before create).
 * Only normal/return exits discharge obligations; throwing never does. */
int xl6ConstructorFlowCheck(const XL6_ConstructorView*, XL6_ConstructorNode,
    int entryInitialized, int requireNormalExit);

#endif
