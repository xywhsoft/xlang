#ifndef XIDE_SEMANTIC_DEFINITION_H
#define XIDE_SEMANTIC_DEFINITION_H
#include "services/toolchain.h"
#include "protocol/lsp_wire.h"
typedef struct xide_semantic_definitions xide_semantic_definitions;
/* Verified original UTF-8 after BOM, before newline normalization. Borrowed
   only during decoding; utf16Prefix accounts for an overlay BOM on line 0.
   Returned table owns paths and converted positions. */
typedef struct xide_semantic_source { const char *path,*text; int kind,utf16Prefix; } xide_semantic_source;
/* Batch conversion validates exact byte endpoints and compiler scalar positions
   in one sorted pass. Borrowed ranges; output positions use UTF-16/LF lines. */
typedef struct xide_semantic_range_query {
 int sourceId; const xvalue* range; xide_lsp_position start,end;
} xide_semantic_range_query;
int xideSemanticRangesToLsp(const xide_semantic_source* sources,size_t count,xide_semantic_range_query* ranges,size_t rangeCount);
xide_semantic_definitions* xideSemanticDefinitionsCreate(const xvalue* index,const xide_semantic_source* sources,size_t count,xide_model_error* error);
#ifdef XLS_COMPILER_XRT
#include "analysis/index.h"
#include "analysis/projection.h"
/* Borrows the checked native facts only during construction. The returned
 * navigation table owns all paths, text and UTF-16 ranges independently. */
xide_semantic_definitions* xideSemanticDefinitionsCreateTyped(const xls_semantic_index* index,const xide_semantic_source* sources,size_t count,xide_model_error* error);
typedef struct xide_navigation_work {int built,hit;size_t bytes;} xide_navigation_work;
/* Optional worker-local sharing of one independently owned immutable table.
 * Exact check owner + complete path/kind/BOM key and original source bytes are
 * required. This does not validate current disk/environment/publication.
 * Continue the original input and publication gates. With a non-NULL lease,
 * release via ReleaseTyped;otherwise the table is ordinarily owned. Optional
 * key/cache allocation/quota failure uses the original constructor. */
xide_semantic_definitions* xideSemanticDefinitionsAcquireTyped(xls_projection_store* cache,
    xls_semantic_check* check,const xide_semantic_source* sources,size_t count,
    xls_projection** lease,xide_navigation_work* work,xide_model_error* error);
void xideSemanticDefinitionsReleaseTyped(xide_semantic_definitions* definitions,xls_projection* lease);
/* Exact checked-source capture for demand construction. Owns paths and a
 * retained check (including original source bytes);never borrows the caller's
 * source array. NULL with no error means optional storage admission failed:
 * use the original eager constructor. No table is built during capture.
 * Resolve uses the original typed constructor/cache and returns its ordinary
 * table/lease ownership. Capture and cache do not replace freshness guards. */
typedef struct xide_navigation_snapshot xide_navigation_snapshot;
xide_navigation_snapshot* xideSemanticNavigationCapture(xls_semantic_check* check,
    const xide_semantic_source* sources,size_t count,xide_model_error* error);
void xideSemanticNavigationDestroy(xide_navigation_snapshot* snapshot);
/* Bind once, before concurrent resolution/publication. Retires an obsolete
 * resident and captures this bank's publication revision. The borrowed bank
 * must outlive Resolve;Destroy does not use it. A different bank is uncached.
 * Later checks invalidate old work's permission to repopulate the bank. */
void xideSemanticNavigationBind(xide_navigation_snapshot* snapshot,xls_projection_store* cache);
xide_semantic_definitions* xideSemanticNavigationResolve(const xide_navigation_snapshot* snapshot,
    xls_projection_store* cache,xls_projection** lease,xide_navigation_work* work,xide_model_error* error);
size_t xideSemanticNavigationBytes(const xide_navigation_snapshot* snapshot);
#endif
void xideSemanticDefinitionsDestroy(xide_semantic_definitions* definitions);
/* Returns a Location or JSON null. Explicit type facts can resolve a unique
   same-compilation declaration, including bases, constraints and generic
   parameters without a runtime ABI; value references keep SymbolId.
   Never searches by spelling or reads disk. */
xvalue* xideSemanticDefinition(xide_semantic_definitions* definitions,const char* path,int line,int character);
xvalue* xideSemanticHover(xide_semantic_definitions* definitions,const char* path,int line,int character);
xvalue* xideSemanticDocumentSymbols(xide_semantic_definitions* definitions,const char* path);
/* Owned standard DocumentHighlight[] for one verified source; deduplicated in
 * source order, using checked declaration identity rather than spelling.
 * Text kind is used until read/write facts are available. No disk reads. */
xvalue* xideSemanticDocumentHighlights(xide_semantic_definitions* definitions,const char* path,int line,int character);
/* Borrowed, immutable same-checked-compilation references, including imported
 * sources already in that compilation. This is not a project-wide index.
 * Keep definitions alive until all views/items are no longer used. Index lookup
 * and At allocate nothing; query retains the existing lexical path matching
 * (which may allocate for alternate paths). No filesystem/source scan or
 * spelling-based symbol search. Byte offsets use compiler UTF-8 after BOM;
 * UTF-16 positions include the captured overlay's BOM prefix. */
typedef struct xide_semantic_reference_view {
 const xide_semantic_definitions* definitions;
 size_t first,count,excluded;
} xide_semantic_reference_view;
typedef struct xide_semantic_reference {
 const char* path;
 xide_lsp_position start,end;
 int declaration;
} xide_semantic_reference;
int xideSemanticReferences(xide_semantic_definitions* definitions,const char* path,
 int line,int character,int includeDeclaration,xide_semantic_reference_view* view);
int xideSemanticReferenceAt(const xide_semantic_reference_view* view,size_t index,
 xide_semantic_reference* reference);
#endif
