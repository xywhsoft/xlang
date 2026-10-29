#ifndef XLANG_ANALYSIS_AST_PACKAGE_FIELDS_H
#define XLANG_ANALYSIS_AST_PACKAGE_FIELDS_H
#include "src/ast/xl6_ast.h"
#include <string.h>
/* Set only by the digest-guarded private AST header. Legacy frontend objects
 * keep their original layout and contain none of these fields. This covers
 * representation copying,not semantic admission or native-ID rebinding. */
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static inline int xlsAstPackageBare(const XL6_AstNode* node)
{return !node->compiledPackageLink && !node->compiledPackageError && !node->compiledPackageRoot && !node->semanticReceiverSymbolId;}
static inline int xlsAstPackageOwnedEmpty(const XL6_AstNode* node)
{return !node->compiledPackageLink;}
static inline void xlsAstPackageClearOwned(XL6_AstNode* node)
{node->compiledPackageLink=NULL;}
static inline int xlsAstPackageScalarsEqual(const XL6_AstNode* a,const XL6_AstNode* b)
{return a->semanticReceiverSymbolId==b->semanticReceiverSymbolId && a->compiledPackageError==b->compiledPackageError && a->compiledPackageRoot==b->compiledPackageRoot;}
static inline int xlsAstPackageLinkEqual(const XL6_AstNode* a,const XL6_AstNode* b)
{return (!!a->compiledPackageLink==!!b->compiledPackageLink) && (!a->compiledPackageLink || !strcmp(a->compiledPackageLink,b->compiledPackageLink));}
#else
#define xlsAstPackageBare(node) 1
#define xlsAstPackageOwnedEmpty(node) 1
#define xlsAstPackageClearOwned(node) ((void)0)
#define xlsAstPackageScalarsEqual(a,b) 1
#define xlsAstPackageLinkEqual(a,b) 1
#endif
#endif
