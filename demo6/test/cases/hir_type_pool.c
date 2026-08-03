#include "src/ast/xl6_ast.h"
#include "src/ir/xl6_hir_type.h"

#include <stdio.h>

static XL6_AstNode* testType(
    const char* name,
    XL6_TypeId sourceTypeId,
    const char* abiName
)
{
    XL6_AstNode* type;

    type = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    if ( type != NULL ) {
        type->semanticTypeId = sourceTypeId;
        type->semanticTypeAbiId = abiName != NULL
            ? xl6TypeAbiIdText(abiName)
            : XL6_TYPE_ABI_ID_INVALID;
    }
    return type;
}

static XL6_AstNode* testGeneric(const char* name, XL6_AstNode* itemType)
{
    XL6_AstNode* type;
    XL6_AstNode* args;

    type = testType(name, 20, "xlang.array");
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( type == NULL || args == NULL ) {
        xl6AstFree(type);
        xl6AstFree(args);
        xl6AstFree(itemType);
        return NULL;
    }
    xl6AstAddChild(args, itemType);
    xl6AstAddChild(type, args);
    return type;
}

static XL6_AstNode* testGenericClassType(
    const char* name,
    XL6_TypeId sourceTypeId,
    const char* abiName,
    XL6_AstNode* itemType
)
{
    XL6_AstNode* type;
    XL6_AstNode* args;

    type = testType(name, sourceTypeId, abiName);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( type == NULL || args == NULL ) {
        xl6AstFree(type);
        xl6AstFree(args);
        xl6AstFree(itemType);
        return NULL;
    }
    xl6AstAddChild(args, itemType);
    xl6AstAddChild(type, args);
    return type;
}

static XL6_AstNode* testFunction(const char* paramName, const char* returnName)
{
    XL6_AstNode* type;
    XL6_AstNode* params;
    XL6_AstNode* returns;

    type = testType("function", XL6_TYPE_ID_INVALID, NULL);
    params = xl6AstNew(XL6_AST_LIST, NULL);
    returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    if ( type == NULL || params == NULL || returns == NULL ) {
        xl6AstFree(type);
        xl6AstFree(params);
        xl6AstFree(returns);
        return NULL;
    }
    xl6AstAddChild(
        params,
        testType(
            paramName,
            strcmp(paramName, "int") == 0 ? 1 : 2,
            strcmp(paramName, "int") == 0 ? "xrt.int64" : "xrt.string"
        )
    );
    xl6AstAddChild(
        returns,
        testType(
            returnName,
            strcmp(returnName, "int") == 0 ? 1 : 2,
            strcmp(returnName, "int") == 0 ? "xrt.int64" : "xrt.string"
        )
    );
    xl6AstAddChild(type, params);
    xl6AstAddChild(type, returns);
    return type;
}

int main(void)
{
    XL6_HirTypePool pool;
    XL6_HirTypePool reversePool;
    XL6_AstNode* root;
    XL6_AstNode* intTypeA;
    XL6_AstNode* intTypeB;
    XL6_AstNode* arrayInt;
    XL6_AstNode* arrayString;
    XL6_AstNode* funcA;
    XL6_AstNode* funcB;
    XL6_AstNode* funcC;
    XL6_AstNode* nodeA;
    XL6_AstNode* nodeB;
    XL6_AstNode* boxDecl;
    XL6_AstNode* boxInt;
    XL6_HirTypeId intA;
    XL6_HirTypeId intB;
    XL6_HirTypeId arrayA;
    XL6_HirTypeId arrayB;
    XL6_HirTypeId functionA;
    XL6_HirTypeId functionB;
    XL6_HirTypeId functionC;
    XL6_HirTypeId classA;
    XL6_HirTypeId classB;
    XL6_HirTypeId genericClass;
    XL6_AstNode* clonedArray;
    XL6_AstNode* clonedFunction;
    XL6_AstNode* clonedGenericClass;
    XL6_AstNode* reverseInt64;
    XL6_AstNode* reverseInt;
    XL6_AstNode* clonedReverseInt;
    int ok;

    xl6HirTypePoolInit(&pool);
    xl6HirTypePoolInit(&reversePool);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    intTypeA = testType("int", 1, "xrt.int64");
    intTypeB = testType("int64", 9, "xrt.int64");
    arrayInt = testGeneric("array", testType("int", 1, "xrt.int64"));
    arrayString = testGeneric("array", testType("string", 2, "xrt.string"));
    funcA = testFunction("int", "string");
    funcB = testFunction("int", "string");
    funcC = testFunction("string", "string");
    nodeA = testType("Node", 30, "module_a.Node");
    nodeB = testType("Node", 31, "module_b.Node");
    boxDecl = xl6AstNewText(XL6_AST_CLASS, "Box", 3, NULL);
    if ( boxDecl != NULL ) {
        boxDecl->semanticTypeId = 32;
        boxDecl->semanticTypeAbiId = xl6TypeAbiIdText("module.Box");
    }
    boxInt = testGenericClassType(
        "Box",
        32,
        "module.Box",
        testType("int", 1, "xrt.int64")
    );
    xl6AstAddChild(root, boxDecl);

    intA = xl6HirTypeInternAst(&pool, root, intTypeA);
    intB = xl6HirTypeInternAst(&pool, root, intTypeB);
    arrayA = xl6HirTypeInternAst(&pool, root, arrayInt);
    arrayB = xl6HirTypeInternAst(&pool, root, arrayString);
    functionA = xl6HirTypeInternAst(&pool, root, funcA);
    functionB = xl6HirTypeInternAst(&pool, root, funcB);
    functionC = xl6HirTypeInternAst(&pool, root, funcC);
    classA = xl6HirTypeInternAst(&pool, root, nodeA);
    classB = xl6HirTypeInternAst(&pool, root, nodeB);
    genericClass = xl6HirTypeInternAst(&pool, root, boxInt);
    clonedArray = xl6HirTypeCloneAst(&pool, arrayA);
    clonedFunction = xl6HirTypeCloneAst(&pool, functionA);
    clonedGenericClass = xl6HirTypeCloneAst(&pool, genericClass);
    reverseInt64 = testType("int64", 9, "xrt.int64");
    reverseInt = testType("int", 1, "xrt.int64");
    intA = xl6HirTypeInternAst(&reversePool, root, reverseInt64);
    intB = xl6HirTypeInternAst(&reversePool, root, reverseInt);
    clonedReverseInt = xl6HirTypeCloneAst(&reversePool, intA);

    ok = intA != XL6_HIR_TYPE_INVALID &&
         intA == intB &&
         arrayA != XL6_HIR_TYPE_INVALID &&
         arrayA != arrayB &&
         functionA != XL6_HIR_TYPE_INVALID &&
         functionA == functionB &&
         functionA != functionC &&
         classA != XL6_HIR_TYPE_INVALID &&
         classB != XL6_HIR_TYPE_INVALID &&
         classA != classB &&
         genericClass != XL6_HIR_TYPE_INVALID &&
         clonedArray != NULL &&
         strcmp(clonedArray->text, "array") == 0 &&
         clonedArray->childCount == 1 &&
         clonedArray->children[0]->childCount == 1 &&
         strcmp(clonedArray->children[0]->children[0]->text, "int") == 0 &&
         clonedFunction != NULL &&
         strcmp(clonedFunction->text, "function") == 0 &&
         clonedFunction->childCount == 2 &&
         clonedFunction->children[0]->childCount == 1 &&
         clonedFunction->children[1]->childCount == 1 &&
         strcmp(clonedFunction->children[1]->children[0]->text, "string") == 0 &&
         clonedGenericClass != NULL &&
         strcmp(clonedGenericClass->text, "__class_handle") == 0 &&
         (clonedGenericClass->flags & XL6_AST_FLAG_CLASS_HANDLE) != 0 &&
         clonedGenericClass->childCount == 1 &&
         clonedGenericClass->children[0]->childCount == 1 &&
         strcmp(clonedGenericClass->children[0]->children[0]->text, "Box") == 0 &&
         clonedGenericClass->children[0]->children[0]->childCount == 1 &&
         clonedGenericClass->children[0]->children[0]->children[0]->childCount == 1 &&
          strcmp(
              clonedGenericClass->children[0]->children[0]->children[0]->children[0]->text,
              "int"
          ) == 0 &&
          intA == intB &&
          clonedReverseInt != NULL &&
          strcmp(clonedReverseInt->text, "int") == 0 &&
          pool.count == 9;

    xl6AstFree(intTypeA);
    xl6AstFree(intTypeB);
    xl6AstFree(arrayInt);
    xl6AstFree(arrayString);
    xl6AstFree(funcA);
    xl6AstFree(funcB);
    xl6AstFree(funcC);
    xl6AstFree(nodeA);
    xl6AstFree(nodeB);
    xl6AstFree(boxInt);
    xl6AstFree(clonedArray);
    xl6AstFree(clonedFunction);
    xl6AstFree(clonedGenericClass);
    xl6AstFree(reverseInt64);
    xl6AstFree(reverseInt);
    xl6AstFree(clonedReverseInt);
    xl6AstFree(root);
    xl6HirTypePoolFree(&pool);
    xl6HirTypePoolFree(&reversePool);

    if ( !ok ) {
        fprintf(stderr, "HIR type pool test failed\n");
        return 1;
    }
    puts("HIR type pool test ok");
    return 0;
}
