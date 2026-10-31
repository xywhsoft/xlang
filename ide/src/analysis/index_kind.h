#ifndef XLANG_ANALYSIS_INDEX_KIND_H
#define XLANG_ANALYSIS_INDEX_KIND_H
#include <string.h>
/* One classification table for the native AST adapter and compatibility
 * string facts. These are query categories,not exposed frontend enum values. */
#define XLS_INDEX_POINT_NATIVE_KINDS(X) \
 X(FUNC,"func") X(EXPORT_FUNC,"export_func") X(CLASS,"class") \
 X(PROTOCOL,"protocol") X(ENUM,"enum") X(TYPE_ALIAS,"type_alias") \
 X(EXTERN_TYPE_ALIAS,"extern_type_alias") X(UNION,"union") \
 X(VAR_DECL,"var_decl") X(CONST_DECL,"const_decl") X(CALL,"call") \
 X(TYPE,"type") X(NAME,"name") X(TYPE_PARAMS,"type_params") \
 X(GENERIC_CONSTRAINT,"generic_constraint") X(MEMBER,"member") X(SAFE_MEMBER,"safe_member")
/* Legacy string exports may use this name;the current native AST has no
 * VARIANT kind. Preserve compatibility without inventing a native enum. */
#define XLS_INDEX_POINT_KINDS(X) XLS_INDEX_POINT_NATIVE_KINDS(X) X(VARIANT,"variant")
enum xls_index_point_kind {
 XLS_INDEX_POINT_OTHER,
#define XLS_INDEX_KIND_ENUM(name,text) XLS_INDEX_POINT_##name,
 XLS_INDEX_POINT_KINDS(XLS_INDEX_KIND_ENUM)
#undef XLS_INDEX_KIND_ENUM
 XLS_INDEX_POINT_KIND_COUNT
};
static inline enum xls_index_point_kind xlsIndexPointKindParse(const char* value)
{
 if(!value)return XLS_INDEX_POINT_OTHER;
#define XLS_INDEX_KIND_PARSE(name,text) if(!strcmp(value,text))return XLS_INDEX_POINT_##name;
 XLS_INDEX_POINT_KINDS(XLS_INDEX_KIND_PARSE)
#undef XLS_INDEX_KIND_PARSE
 return XLS_INDEX_POINT_OTHER;
}
#endif
