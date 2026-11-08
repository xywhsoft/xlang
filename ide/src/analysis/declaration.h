#ifndef XLS_ANALYSIS_DECLARATION_H
#define XLS_ANALYSIS_DECLARATION_H
#include "analysis/index.h"

/* Source declaration roles, independent of frontend enum values and wire
 * SymbolKinds. Scope-only nodes still hide callable locals. No IDs are
 * invented for syntax-backed fields that lack executable SymbolIds. */
enum xls_declaration_kind {
 XLS_DECL_NONE,XLS_DECL_FUNCTION,XLS_DECL_CLASS,XLS_DECL_INTERFACE,
 XLS_DECL_ENUM,XLS_DECL_ENUM_MEMBER,XLS_DECL_RECORD,XLS_DECL_ALIAS,
 XLS_DECL_VARIABLE,XLS_DECL_CONSTANT,XLS_DECL_FIELD,XLS_DECL_PROPERTY,
 XLS_DECL_SPECIAL,XLS_DECL_TYPE_PARAMETER
};
enum xls_declaration_name {XLS_DECL_NAME_NONE,XLS_DECL_NAME_TOKEN,XLS_DECL_NAME_CHILD};
typedef struct xls_declaration_shape {
 enum xls_declaration_kind kind;enum xls_declaration_name name;unsigned callable;
} xls_declaration_shape;
#define XLS_DECLARATION_COMMON_KINDS(X) \
 X(FUNC,"func",FUNCTION,CHILD,1) X(ASYNC_FUNC,"async_func",FUNCTION,CHILD,1) \
 X(EXTERN_FUNC,"extern_func",FUNCTION,CHILD,1) X(EXPORT_FUNC,"export_func",FUNCTION,CHILD,1) \
 X(ANON_FUNC,"anon_func",NONE,NONE,1) \
 X(CLASS,"class",CLASS,TOKEN,0) X(PROTOCOL,"protocol",INTERFACE,TOKEN,0) \
 X(ENUM,"enum",ENUM,TOKEN,0) X(ENUM_VARIANT,"enum_variant",ENUM_MEMBER,TOKEN,0) \
 X(UNION,"union",RECORD,TOKEN,0) X(TYPE_ALIAS,"type_alias",ALIAS,TOKEN,0) \
 X(EXTERN_TYPE_ALIAS,"extern_type_alias",ALIAS,TOKEN,0) \
 X(VAR_DECL,"var_decl",VARIABLE,TOKEN,0) X(CONST_DECL,"const_decl",CONSTANT,TOKEN,0) \
 X(EXTERN_VAR,"extern_var",VARIABLE,CHILD,0) X(EXPORT_VAR,"export_var",VARIABLE,CHILD,0) \
 X(EXPORT_CONST,"export_const",CONSTANT,CHILD,0) X(FIELD,"field",FIELD,TOKEN,0) \
 X(GET,"get",PROPERTY,CHILD,1) X(LET,"let",PROPERTY,CHILD,1) \
 X(CREATE,"create",SPECIAL,TOKEN,1) X(NAME,"name",TYPE_PARAMETER,TOKEN,0)
#define XLS_DECLARATION_EXTERNAL_MEMBER_KINDS(X) \
 X(NATIVE_EXTERN_FUNC,"native_extern_func",FUNCTION,CHILD,1) \
 X(NATIVE_EXTERN_SPECIAL,"native_extern_special",SPECIAL,CHILD,1)
#define XLS_DECLARATION_STRING_KINDS(X) XLS_DECLARATION_COMMON_KINDS(X) \
 XLS_DECLARATION_EXTERNAL_MEMBER_KINDS(X) \
 X(C_RECORD,"C record",RECORD,TOKEN,0) X(VARIANT,"variant",ENUM_MEMBER,TOKEN,0)

static inline xls_declaration_shape xlsDeclarationShapeFromText(const char* kind)
{
 if(!kind)return (xls_declaration_shape){0};
#define XLS_DECLARATION_TEXT_CASE(native,text,category,selection,scope) \
 if(!strcmp(kind,text))return (xls_declaration_shape){XLS_DECL_##category,XLS_DECL_NAME_##selection,scope};
 XLS_DECLARATION_STRING_KINDS(XLS_DECLARATION_TEXT_CASE)
#undef XLS_DECLARATION_TEXT_CASE
 return (xls_declaration_shape){0};
}

typedef struct xls_declaration_fact {
 xls_index_fact fact; /* Original fact, unmodified, including original selection. */
 xls_declaration_shape shape;
 unsigned declarationSource,declarationNode;
 xls_index_range declarationRange; /* May be the shared multi-binding statement. */
 unsigned nameSource,nameNode; /* Source and one-based original token node. */
 xls_index_range nameRange; /* Borrowed compiler token facts; still validate text. */
} xls_declaration_fact;
/* One immutable read; no source scans, allocations, path lookup or new semantic
 * identities. Retain/validate the owning check just as for the original index.
 * Nameless/generated declarations have an unknown name range. */
int xlsSemanticIndexDeclarationFact(const xls_semantic_index* index,size_t at,xls_declaration_fact* out);
#endif
