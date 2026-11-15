#ifndef XIDE_ANALYSIS_CATALOG_H
#define XIDE_ANALYSIS_CATALOG_H
#include "protocol/analysis_context_wire.h"
typedef struct xide_analysis_catalog xide_analysis_catalog;
/* Optional immutable external membership/configuration evidence. Installed
 * only while constructing a private catalog. Retain/release do not perform IO;
 * validate is a worker-only, thread-safe, cancellable operation. bytes charges
 * the complete known retained evidence payload, not allocator overhead. The
 * evidence must not retain its catalog or any owner that retains the catalog.
 * This does not replace source, overlay, request or publication epoch guards. */
typedef struct xide_analysis_catalog_guard {
 void* value;void* (*retain)(void*);void (*release)(void*);
 int (*validate)(void*,int (*cancelled)(void*),void*,char*,size_t);
 size_t bytes;
} xide_analysis_catalog_guard;
enum xide_analysis_binding { XIDE_ANALYSIS_UNOWNED, XIDE_ANALYSIS_PROJECT, XIDE_ANALYSIS_AMBIGUOUS=-1, XIDE_ANALYSIS_ERROR=-2 };
/* Prepared schema1/2 client configuration: up to64 project/standalone roles,65536 entry/member
   bindings and16MiB of immutable contexts. No project manifest parsing or
   file/alias I/O. Owns contexts and a sorted native lexical-path index. */
xide_analysis_catalog* xideAnalysisCatalogCreate(const xvalue* value,char* error,size_t capacity);
xide_analysis_catalog* xideAnalysisCatalogCreateGuarded(const xvalue*,const xide_analysis_catalog_guard*,char*,size_t);
/* Worker-only for guarded catalogs. NULL/ordinary client catalogs have no
 * external evidence and only check cancellation. Never call this on owner as
 * a substitute for a background validation/publication fence. */
int xideAnalysisCatalogValidate(const xide_analysis_catalog*,int (*cancelled)(void*),void*,char*,size_t);
xide_analysis_catalog* xideAnalysisCatalogRetain(xide_analysis_catalog* catalog);
void xideAnalysisCatalogDestroy(xide_analysis_catalog* catalog);
/* Borrowed context. Ambiguous membership needs an explicit selected project;
   an unrelated selection does not resolve ambiguity. Safe concurrent reads. */
enum xide_analysis_binding xideAnalysisCatalogFind(const xide_analysis_catalog* catalog,const char* path,const xls_context** context);
enum xide_analysis_binding xideAnalysisCatalogFindNative(const xide_analysis_catalog* catalog,const wchar_t* key,const xls_context** context);
/* All explicitly configured project owners, independent of selectedProject.
   Bits address At order (at most64). Native lookup allocates nothing and does
   not inspect disk, aliases, imports or directory containment. An unlisted
   dependency has no owners even when its path is under a project directory.
   This is membership, not OS writability or rename-safety/freshness proof. */
int xideAnalysisCatalogOwnersNative(const xide_analysis_catalog* catalog,const wchar_t* key,uint64_t* owners);
/* Complete known retained payload, including allocated binding capacity before
   duplicate compaction, contexts, keys, package and catalog. Excludes allocator
   metadata and temporary construction. No traversal or allocation. */
size_t xideAnalysisCatalogBytes(const xide_analysis_catalog* catalog);
int xideAnalysisCatalogPrefers(const xide_analysis_catalog* catalog,const xls_context* context);
const char* xideAnalysisCatalogPackagePath(const xide_analysis_catalog* catalog);
size_t xideAnalysisCatalogCount(const xide_analysis_catalog* catalog);
/* Borrowed complete project scope in prepared order;NULL out of range. */
const xls_context* xideAnalysisCatalogAt(const xide_analysis_catalog* catalog,size_t index);
/* Schema2 explicitly declares workspace standalone roots, including closed
   files. Project-owned and native-lexical duplicate roots are compacted. These
   do not change project membership/selection. Borrowed immutable contexts. */
size_t xideAnalysisCatalogFileCount(const xide_analysis_catalog* catalog);
const xls_context* xideAnalysisCatalogFileAt(const xide_analysis_catalog* catalog,size_t index);
/* False when a workspace project was unavailable. Ordinary per-file analysis
   may proceed, but a whole-workspace result must not claim complete coverage. */
int xideAnalysisCatalogComplete(const xide_analysis_catalog* catalog);
/* No matching project: capture a standalone .xl context from the file's own
   directory and this catalog's explicit package input, without alias I/O.
   An explicitly registered standalone root returns its retained exact context. */
xls_context* xideAnalysisCatalogFile(const xide_analysis_catalog* catalog,const char* path);
#endif
