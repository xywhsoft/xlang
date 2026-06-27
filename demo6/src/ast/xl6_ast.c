#include "src/ast/xl6_ast.h"

const char* xl6AstKindName(XL6_AstKind kind)
{
    switch ( kind ) {
    case XL6_AST_MODULE: return "module";
    case XL6_AST_LIST: return "list";
    case XL6_AST_IMPORT: return "import";
    case XL6_AST_FUNC: return "func";
    case XL6_AST_ASYNC_FUNC: return "async_func";
    case XL6_AST_ANON_FUNC: return "anon_func";
    case XL6_AST_EXTERN_FUNC: return "extern_func";
    case XL6_AST_EXPORT_FUNC: return "export_func";
    case XL6_AST_EXTERN_VAR: return "extern_var";
    case XL6_AST_EXPORT_VAR: return "export_var";
    case XL6_AST_EXPORT_CONST: return "export_const";
    case XL6_AST_TYPE_ALIAS: return "type_alias";
    case XL6_AST_EXTERN_TYPE_ALIAS: return "extern_type_alias";
    case XL6_AST_CLASS: return "class";
    case XL6_AST_CUSTOM: return "custom";
    case XL6_AST_UNION: return "union";
    case XL6_AST_DERIVE: return "derive";
    case XL6_AST_FIELD: return "field";
    case XL6_AST_CREATE: return "create";
    case XL6_AST_CUSTOM_EXTERN_SPECIAL: return "custom_extern_special";
    case XL6_AST_CUSTOM_EXTERN_FUNC: return "custom_extern_func";
    case XL6_AST_TYPE_PARAMS: return "type_params";
    case XL6_AST_PARAM: return "param";
    case XL6_AST_RETURN_TYPES: return "return_types";
    case XL6_AST_BLOCK: return "block";
    case XL6_AST_VAR_DECL: return "var_decl";
    case XL6_AST_MULTI_VAR_DECL: return "multi_var_decl";
    case XL6_AST_CONST_DECL: return "const_decl";
    case XL6_AST_RETURN: return "return";
    case XL6_AST_YIELD: return "yield";
    case XL6_AST_EXPR_STMT: return "expr_stmt";
    case XL6_AST_TYPE: return "type";
    case XL6_AST_NAME: return "name";
    case XL6_AST_INT: return "int";
    case XL6_AST_FLOAT: return "float";
    case XL6_AST_STRING: return "string";
    case XL6_AST_TIME: return "time";
    case XL6_AST_BOOL: return "bool";
    case XL6_AST_NULL: return "null";
    case XL6_AST_CALL: return "call";
    case XL6_AST_MEMBER: return "member";
    case XL6_AST_SAFE_MEMBER: return "safe_member";
    case XL6_AST_INDEX: return "index";
    case XL6_AST_SAFE_INDEX: return "safe_index";
    case XL6_AST_GENERIC_INSTANT: return "generic_instant";
    case XL6_AST_ARG: return "arg";
    case XL6_AST_NAMED_ARG: return "named_arg";
    case XL6_AST_ASSIGN: return "assign";
    case XL6_AST_MULTI_ASSIGN: return "multi_assign";
    case XL6_AST_CONDITIONAL: return "conditional";
    case XL6_AST_BINARY: return "binary";
    case XL6_AST_UNARY: return "unary";
    case XL6_AST_AWAIT: return "await";
    case XL6_AST_ARRAY_LITERAL: return "array_literal";
    case XL6_AST_LIST_LITERAL: return "list_literal";
    case XL6_AST_SET_LITERAL: return "set_literal";
    case XL6_AST_DICT_LITERAL: return "dict_literal";
    case XL6_AST_TYPED_LITERAL: return "typed_literal";
    case XL6_AST_KEY_VALUE: return "key_value";
    case XL6_AST_IF: return "if";
    case XL6_AST_ELSEIF: return "elseif";
    case XL6_AST_WHILE: return "while";
    case XL6_AST_DO_WHILE: return "do_while";
    case XL6_AST_FOR: return "for";
    case XL6_AST_FOR_TARGETS: return "for_targets";
    case XL6_AST_TRY: return "try";
    case XL6_AST_CATCH: return "catch";
    case XL6_AST_THROW: return "throw";
    case XL6_AST_ASSERT: return "assert";
    case XL6_AST_BREAK: return "break";
    case XL6_AST_CONTINUE: return "continue";
    default: return "unknown";
    }
}

XL6_AstNode* xl6AstNew(XL6_AstKind kind, const XL6_Token* token)
{
    XL6_AstNode* node;

    node = (XL6_AstNode*)xrtCalloc(1, sizeof(*node));
    if ( node == NULL ) {
        return NULL;
    }
    node->kind = kind;
    if ( token != NULL ) {
        node->line = token->startLine;
        node->column = token->startColumn;
        if ( token->text != NULL && token->len > 0 ) {
            node->text = xrtCopyStr((char*)token->text, token->len);
        }
    }
    return node;
}

XL6_AstNode* xl6AstNewText(XL6_AstKind kind, const char* text, size_t len, const XL6_Token* token)
{
    XL6_AstNode* node;

    node = xl6AstNew(kind, token);
    if ( node == NULL ) {
        return NULL;
    }
    xrtFree(node->text);
    node->text = xrtCopyStr((char*)(text != NULL ? text : ""), len);
    return node;
}

XL6_AstNode* xl6AstNewBinary(const XL6_Token* op, XL6_AstNode* left, XL6_AstNode* right)
{
    XL6_AstNode* node;

    node = xl6AstNew(XL6_AST_BINARY, op);
    xl6AstAddChild(node, left);
    xl6AstAddChild(node, right);
    return node;
}

XL6_AstNode* xl6AstNewUnary(const XL6_Token* op, XL6_AstNode* expr)
{
    XL6_AstNode* node;

    node = xl6AstNew(XL6_AST_UNARY, op);
    xl6AstAddChild(node, expr);
    return node;
}

XL6_AstNode* xl6AstNewCall(XL6_AstNode* callee, XL6_AstNode* args, const XL6_Token* token)
{
    XL6_AstNode* node;

    node = xl6AstNew(XL6_AST_CALL, token);
    xl6AstAddChild(node, callee);
    xl6AstAddChild(node, args);
    return node;
}

XL6_AstNode* xl6AstNewGenericType(const XL6_Token* name, XL6_AstNode* args)
{
    XL6_AstNode* node;

    node = xl6AstNew(XL6_AST_TYPE, name);
    xl6AstAddChild(node, args);
    return node;
}

void xl6AstSetDoc(XL6_AstNode* node, const XL6_Token* token)
{
    if ( node == NULL || token == NULL ) {
        return;
    }
    xrtFree(node->doc);
    node->doc = xrtCopyStr((char*)(token->text != NULL ? token->text : ""), token->len);
}

void xl6AstAddChild(XL6_AstNode* parent, XL6_AstNode* child)
{
    XL6_AstNode** newChildren;
    size_t newCapacity;

    if ( (parent == NULL) || (child == NULL) ) {
        return;
    }
    if ( parent->childCount == parent->childCapacity ) {
        newCapacity = (parent->childCapacity == 0) ? 4 : parent->childCapacity * 2;
        newChildren = (XL6_AstNode**)xrtRealloc(parent->children, newCapacity * sizeof(XL6_AstNode*));
        if ( newChildren == NULL ) {
            return;
        }
        parent->children = newChildren;
        parent->childCapacity = newCapacity;
    }
    parent->children[parent->childCount++] = child;
}

void xl6AstMoveChildren(XL6_AstNode* dst, XL6_AstNode* src)
{
    size_t i;

    if ( (dst == NULL) || (src == NULL) ) {
        return;
    }
    for ( i = 0; i < src->childCount; ++i ) {
        xl6AstAddChild(dst, src->children[i]);
        src->children[i] = NULL;
    }
    src->childCount = 0;
}

void xl6AstFree(XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6AstFree(node->children[i]);
    }
    xrtFree(node->children);
    xrtFree(node->text);
    xrtFree(node->doc);
    xrtFree(node);
}
