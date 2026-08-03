#include "src/cgen/xl6_cgen_entry.h"

#include <string.h>

static int xl6CgenEntryTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

int xl6CgenIsTopLevelRuntimeStmt(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind != XL6_AST_IMPORT &&
           node->kind != XL6_AST_FUNC &&
           node->kind != XL6_AST_ASYNC_FUNC &&
           node->kind != XL6_AST_EXTERN_FUNC &&
           node->kind != XL6_AST_EXPORT_FUNC &&
           node->kind != XL6_AST_EXTERN_VAR &&
           node->kind != XL6_AST_EXPORT_VAR &&
           node->kind != XL6_AST_EXPORT_CONST &&
           node->kind != XL6_AST_VAR_DECL &&
           node->kind != XL6_AST_CONST_DECL &&
           node->kind != XL6_AST_TYPE_ALIAS &&
           node->kind != XL6_AST_EXTERN_TYPE_ALIAS &&
           node->kind != XL6_AST_UNION &&
           node->kind != XL6_AST_ENUM &&
           node->kind != XL6_AST_PROTOCOL &&
           node->kind != XL6_AST_CLASS;
}

int xl6CgenFuncIsMainEntry(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;

    if ( node == NULL || node->kind != XL6_AST_FUNC || node->childCount == 0 ) {
        return 0;
    }
    nameNode = node->children[0];
    return nameNode != NULL && xl6CgenEntryTextEquals(nameNode->text, "main");
}

const XL6_AstNode* xl6CgenFindMainEntry(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xl6CgenFuncIsMainEntry(root->children[i]) ) {
            return root->children[i];
        }
    }
    return NULL;
}

int xl6CgenHasTopLevelRuntimeStmt(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xl6CgenIsTopLevelRuntimeStmt(root->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
