#include "src/type/xl6_type_model.h"

#include "src/common/xl6_common.h"

typedef struct XL6_TypeStringBuilder {
    char* data;
    size_t len;
    size_t capacity;
    int ok;
} XL6_TypeStringBuilder;

static int xl6TypeTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static int xl6TypeIsAliasDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_TYPE_ALIAS || node->kind == XL6_AST_EXTERN_TYPE_ALIAS);
}

static const XL6_AstNode* xl6TypeFindAlias(const XL6_AstNode* root, const char* name)
{
    size_t i;

    if ( root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( xl6TypeIsAliasDecl(node) && node->text != NULL && strcmp(node->text, name) == 0 ) {
            return node;
        }
    }
    return NULL;
}

const XL6_AstNode* xl6TypeResolveAlias(const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    int guard;

    /*
        类型别名在 canonical type 中是透明的。
        guard 防止异常 AST 或未来手写元数据造成无限展开。
    */
    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* aliasNode;
        if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
            return typeNode;
        }
        aliasNode = xl6TypeFindAlias(root, typeNode->text);
        if ( aliasNode == NULL || aliasNode->childCount == 0 ) {
            return typeNode;
        }
        typeNode = aliasNode->children[0];
    }
    return typeNode;
}

static void xl6TypeStringBuilderInit(XL6_TypeStringBuilder* builder)
{
    memset(builder, 0, sizeof(*builder));
    builder->ok = 1;
}

static void xl6TypeStringBuilderFree(XL6_TypeStringBuilder* builder)
{
    if ( builder == NULL ) {
        return;
    }
    xrtFree(builder->data);
    memset(builder, 0, sizeof(*builder));
}

static int xl6TypeStringBuilderGrow(XL6_TypeStringBuilder* builder, size_t addLen)
{
    char* newData;
    size_t required;
    size_t newCapacity;

    if ( builder == NULL || !builder->ok ) {
        return 0;
    }
    required = builder->len + addLen + 1;
    if ( required <= builder->capacity ) {
        return 1;
    }
    newCapacity = builder->capacity == 0 ? 64 : builder->capacity;
    while ( newCapacity < required ) {
        newCapacity *= 2;
    }
    newData = (char*)xrtRealloc(builder->data, newCapacity);
    if ( newData == NULL ) {
        builder->ok = 0;
        return 0;
    }
    builder->data = newData;
    builder->capacity = newCapacity;
    return 1;
}

static void xl6TypeStringBuilderAppendRange(XL6_TypeStringBuilder* builder, const char* text, size_t len)
{
    if ( builder == NULL || text == NULL || !xl6TypeStringBuilderGrow(builder, len) ) {
        return;
    }
    if ( len > 0 ) {
        memcpy(builder->data + builder->len, text, len);
    }
    builder->len += len;
    builder->data[builder->len] = '\0';
}

static void xl6TypeStringBuilderAppend(XL6_TypeStringBuilder* builder, const char* text)
{
    if ( text != NULL ) {
        xl6TypeStringBuilderAppendRange(builder, text, strlen(text));
    }
}

static void xl6TypeStringBuilderAppendChar(XL6_TypeStringBuilder* builder, char ch)
{
    xl6TypeStringBuilderAppendRange(builder, &ch, 1);
}

static char* xl6TypeStringBuilderTake(XL6_TypeStringBuilder* builder)
{
    char* out;

    if ( builder == NULL || !builder->ok ) {
        return NULL;
    }
    if ( builder->data == NULL ) {
        builder->data = xrtCopyStr("", 0);
        builder->capacity = builder->data != NULL ? 1 : 0;
        builder->ok = builder->data != NULL;
    }
    out = builder->data;
    builder->data = NULL;
    builder->len = 0;
    builder->capacity = 0;
    return out;
}

static void xl6TypeAppendCanonicalBody(XL6_TypeStringBuilder* builder, const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    size_t i;
    const XL6_AstNode* args;
    const XL6_AstNode* returns;

    typeNode = xl6TypeResolveAlias(root, typeNode);
    if ( typeNode == NULL ) {
        xl6TypeStringBuilderAppend(builder, "<missing-type>");
        return;
    }

    if ( xl6TypeTextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        args = typeNode->children[0];
        returns = typeNode->children[1];
        xl6TypeStringBuilderAppend(builder, "func(");
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6TypeStringBuilderAppendChar(builder, ',');
                }
                xl6TypeAppendCanonicalBody(builder, root, args->children[i]);
            }
        }
        xl6TypeStringBuilderAppendChar(builder, ')');
        if ( returns != NULL && returns->childCount > 0 ) {
            xl6TypeStringBuilderAppend(builder, " as ");
            if ( returns->childCount > 1 ) {
                xl6TypeStringBuilderAppendChar(builder, '(');
            }
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6TypeStringBuilderAppendChar(builder, ',');
                }
                xl6TypeAppendCanonicalBody(builder, root, returns->children[i]);
            }
            if ( returns->childCount > 1 ) {
                xl6TypeStringBuilderAppendChar(builder, ')');
            }
        }
        return;
    }

    xl6TypeStringBuilderAppend(builder, typeNode->text != NULL ? typeNode->text : "<missing-type>");
    if ( typeNode->childCount == 0 ) {
        return;
    }

    args = typeNode->children[0];
    if ( args == NULL || args->kind != XL6_AST_LIST ) {
        return;
    }
    xl6TypeStringBuilderAppendChar(builder, '<');
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            xl6TypeStringBuilderAppendChar(builder, ',');
        }
        xl6TypeAppendCanonicalBody(builder, root, args->children[i]);
    }
    xl6TypeStringBuilderAppendChar(builder, '>');
}

char* xl6TypeToCanonicalString(const XL6_AstNode* root, const XL6_AstNode* typeNode)
{
    XL6_TypeStringBuilder builder;
    char* out;

    xl6TypeStringBuilderInit(&builder);
    xl6TypeAppendCanonicalBody(&builder, root, typeNode);
    out = xl6TypeStringBuilderTake(&builder);
    xl6TypeStringBuilderFree(&builder);
    return out;
}

int xl6TypeTextIsArrayVararg(const char* text)
{
    return xl6TypeTextEquals(text, "array<var>");
}

int xl6TypeTextIsDictVararg(const char* text)
{
    return xl6TypeTextEquals(text, "dict<var>");
}

int xl6TypeTextIsVararg(const char* text)
{
    return xl6TypeTextIsArrayVararg(text) || xl6TypeTextIsDictVararg(text);
}
