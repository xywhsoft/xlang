#ifndef XIDE_ANALYSIS_CATALOG_H
#define XIDE_ANALYSIS_CATALOG_H
#include "protocol/analysis_context_wire.h"
typedef struct xide_analysis_catalog xide_analysis_catalog;
enum xide_analysis_binding { XIDE_ANALYSIS_UNOWNED, XIDE_ANALYSIS_PROJECT, XIDE_ANALYSIS_AMBIGUOUS=-1, XIDE_ANALYSIS_ERROR=-2 };
/* Prepared schema1 client configuration: up to64 projects,65536 entry/member
   bindings and16MiB of immutable contexts. No project manifest parsing or
   file/alias I/O. Owns contexts and a sorted native lexical-path index. */
xide_analysis_catalog* xideAnalysisCatalogCreate(const xvalue* value,char* error,size_t capacity);
xide_analysis_catalog* xideAnalysisCatalogRetain(xide_analysis_catalog* catalog);
void xideAnalysisCatalogDestroy(xide_analysis_catalog* catalog);
/* Borrowed context. Ambiguous membership needs an explicit selected project;
   an unrelated selection does not resolve ambiguity. Safe concurrent reads. */
enum xide_analysis_binding xideAnalysisCatalogFind(const xide_analysis_catalog* catalog,const char* path,const xls_context** context);
enum xide_analysis_binding xideAnalysisCatalogFindNative(const xide_analysis_catalog* catalog,const wchar_t* key,const xls_context** context);
int xideAnalysisCatalogPrefers(const xide_analysis_catalog* catalog,const xls_context* context);
const char* xideAnalysisCatalogPackagePath(const xide_analysis_catalog* catalog);
size_t xideAnalysisCatalogCount(const xide_analysis_catalog* catalog);
/* No matching project: capture a standalone .xl context from the file's own
   directory and this catalog's explicit package input, without alias I/O. */
xls_context* xideAnalysisCatalogFile(const xide_analysis_catalog* catalog,const char* path);
#endif
