#ifndef XL6_AST_H
#define XL6_AST_H

#include "src/lexer/xl6_token.h"

typedef enum XL6_AstKind {
    XL6_AST_MODULE,
    XL6_AST_LIST,
    XL6_AST_IMPORT,
    XL6_AST_FUNC,
    XL6_AST_ASYNC_FUNC,
    XL6_AST_ANON_FUNC,
    XL6_AST_EXTERN_FUNC,
    XL6_AST_EXPORT_FUNC,
    XL6_AST_EXTERN_VAR,
    XL6_AST_EXPORT_VAR,
    XL6_AST_EXPORT_CONST,
    XL6_AST_TYPE_ALIAS,
    XL6_AST_EXTERN_TYPE_ALIAS,
    XL6_AST_CLASS,
    XL6_AST_UNION,
    XL6_AST_DERIVE,
    XL6_AST_FIELD,
    XL6_AST_GET,
    XL6_AST_LET,
    XL6_AST_CREATE,
    XL6_AST_NATIVE_EXTERN_SPECIAL,
    XL6_AST_NATIVE_EXTERN_FUNC,
    XL6_AST_TYPE_PARAMS,
    XL6_AST_GENERIC_CONSTRAINT,
    XL6_AST_FIELD_ATTR,
    XL6_AST_PARAM,
    XL6_AST_RETURN_TYPES,
    XL6_AST_BLOCK,
    XL6_AST_VAR_DECL,
    XL6_AST_MULTI_VAR_DECL,
    XL6_AST_CONST_DECL,
    XL6_AST_RETURN,
    XL6_AST_YIELD,
    XL6_AST_EXPR_STMT,
    XL6_AST_TYPE,
    XL6_AST_NAME,
    XL6_AST_INT,
    XL6_AST_FLOAT,
    XL6_AST_STRING,
    XL6_AST_INTERP_STRING,
    XL6_AST_INTERP_TEXT,
    XL6_AST_TIME,
    XL6_AST_BOOL,
    XL6_AST_NULL,
    XL6_AST_CALL,
    XL6_AST_MEMBER,
    XL6_AST_SAFE_MEMBER,
    XL6_AST_INDEX,
    XL6_AST_SAFE_INDEX,
    XL6_AST_GENERIC_INSTANT,
    XL6_AST_ARG,
    XL6_AST_NAMED_ARG,
    XL6_AST_ASSIGN,
    XL6_AST_MULTI_ASSIGN,
    XL6_AST_CONDITIONAL,
    XL6_AST_BINARY,
    XL6_AST_TYPE_IS,
    XL6_AST_TYPE_AS,
    XL6_AST_UNARY,
    XL6_AST_AWAIT,
    XL6_AST_ARRAY_LITERAL,
    XL6_AST_LIST_LITERAL,
    XL6_AST_SET_LITERAL,
    XL6_AST_DICT_LITERAL,
    XL6_AST_TYPED_LITERAL,
    XL6_AST_KEY_VALUE,
    XL6_AST_IF,
    XL6_AST_ELSEIF,
    XL6_AST_WHILE,
    XL6_AST_DO_WHILE,
    XL6_AST_FOR,
    XL6_AST_FOR_TARGETS,
    XL6_AST_TRY,
    XL6_AST_CATCH,
    XL6_AST_THROW,
    XL6_AST_ASSERT,
    XL6_AST_BREAK,
    XL6_AST_CONTINUE
} XL6_AstKind;

#define XL6_AST_FLAG_PUBLIC   0x00000001u
#define XL6_AST_FLAG_PRIVATE  0x00000002u
#define XL6_AST_FLAG_PROTECTED 0x00000004u
#define XL6_AST_FLAG_STATIC   0x00000008u
#define XL6_AST_FLAG_OVERRIDE 0x00000010u
#define XL6_AST_FLAG_FINAL    0x00000020u
#define XL6_AST_FLAG_VIRTUAL  0x00000040u
#define XL6_AST_FLAG_NATIVE   0x00000080u
#define XL6_AST_FLAG_ASYNC    0x00000100u
#define XL6_AST_FLAG_EXTERN   0x00000200u
#define XL6_AST_FLAG_BYVAL    0x00000400u
#define XL6_AST_FLAG_BYREF    0x00000800u
#define XL6_AST_FLAG_OUT      0x00001000u
/* 仅供语义规范化使用，源码不能构造该标记。 */
#define XL6_AST_FLAG_CLASS_HANDLE 0x80000000u

typedef struct XL6_AstNode {
    XL6_AstKind kind;
    unsigned int flags;
    char* text;
    char* doc;
    /* 语义阶段写入的规范化静态类型；由当前节点独占。 */
    struct XL6_AstNode* inferredType;
    int line;
    int column;
    struct XL6_AstNode** children;
    size_t childCount;
    size_t childCapacity;
} XL6_AstNode;

const char* xl6AstKindName(XL6_AstKind kind);
XL6_AstNode* xl6AstNew(XL6_AstKind kind, const XL6_Token* token);
XL6_AstNode* xl6AstNewText(XL6_AstKind kind, const char* text, size_t len, const XL6_Token* token);
XL6_AstNode* xl6AstNewBinary(const XL6_Token* op, XL6_AstNode* left, XL6_AstNode* right);
XL6_AstNode* xl6AstNewUnary(const XL6_Token* op, XL6_AstNode* expr);
XL6_AstNode* xl6AstNewCall(XL6_AstNode* callee, XL6_AstNode* args, const XL6_Token* token);
XL6_AstNode* xl6AstNewGenericType(const XL6_Token* name, XL6_AstNode* args);
void xl6AstSetDoc(XL6_AstNode* node, const XL6_Token* token);
void xl6AstAddChild(XL6_AstNode* parent, XL6_AstNode* child);
void xl6AstMoveChildren(XL6_AstNode* dst, XL6_AstNode* src);
void xl6AstFree(XL6_AstNode* node);

#endif
