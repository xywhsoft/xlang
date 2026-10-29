#ifndef XLIR_INTERNAL_OWNED_PARAMS_H
#define XLIR_INTERNAL_OWNED_PARAMS_H

#include "src/ast/xl6_ast.h"
#include <string.h>

/* Test-only construction of the IR's owned-parameter ABI.  The language no
 * longer has source syntax for parameter modes, but the ownership verifier
 * still needs direct coverage of consuming call and activation edges. */
static size_t xlirTestMarkOwnedNamedParams(XL6_AstNode* root,
                                            const char* name,
                                            unsigned long long mask)
{
    size_t i;
    size_t marked = 0;
    if (root == NULL) return 0;
    if ((root->kind == XL6_AST_FUNC || root->kind == XL6_AST_ASYNC_FUNC ||
         root->kind == XL6_AST_EXPORT_FUNC) && root->childCount > 1 &&
        root->children[0] != NULL && root->children[0]->text != NULL &&
        strcmp(root->children[0]->text, name) == 0 && root->children[1] != NULL) {
        XL6_AstNode* params = root->children[1];
        for (i = 0; i < params->childCount; ++i) {
            XL6_AstNode* param = params->children[i];
            if (i >= 64 || (mask & (1ULL << i)) == 0 || param == NULL ||
                param->kind != XL6_AST_PARAM || param->childCount == 0 ||
                param->children[0] == NULL) continue;
            param->flags |= XL6_AST_FLAG_BYVAL;
            param->children[0]->flags |= XL6_AST_FLAG_BYVAL;
            ++marked;
        }
    }
    for (i = 0; i < root->childCount; ++i)
        marked += xlirTestMarkOwnedNamedParams(root->children[i], name, mask);
    return marked;
}

static size_t xlirTestMarkOwnedOuterFunctionTypes(XL6_AstNode* type)
{
    size_t i;
    size_t marked = 0;
    if (type == NULL) return 0;
    if (type->kind == XL6_AST_TYPE && type->text != NULL &&
        strcmp(type->text, "function") == 0 && type->childCount > 0 &&
        type->children[0] != NULL) {
        XL6_AstNode* params = type->children[0];
        for (i = 0; i < params->childCount; ++i) {
            if (params->children[i] == NULL) continue;
            params->children[i]->flags |= XL6_AST_FLAG_BYVAL;
            ++marked;
        }
        return marked; /* Nested callable parameters remain ordinary values. */
    }
    for (i = 0; i < type->childCount; ++i)
        marked += xlirTestMarkOwnedOuterFunctionTypes(type->children[i]);
    return marked;
}

static size_t xlirTestMarkOwnedGlobalFunctionTypes(XL6_AstNode* root,
                                                   const char* excludedName)
{
    size_t i;
    size_t marked = 0;
    if (root == NULL) return 0;
    for (i = 0; i < root->childCount; ++i) {
        XL6_AstNode* item = root->children[i];
        if (item != NULL && item->kind == XL6_AST_VAR_DECL &&
            item->childCount > 0 &&
            (excludedName == NULL || item->text == NULL ||
             strcmp(item->text, excludedName) != 0))
            marked += xlirTestMarkOwnedOuterFunctionTypes(item->children[0]);
    }
    return marked;
}

#endif
