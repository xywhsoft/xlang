#ifndef XIDE_SEMANTIC_REFERENCE_SHARD_H
#define XIDE_SEMANTIC_REFERENCE_SHARD_H
#include "services/semantic_definition.h"
#ifdef XLS_COMPILER_XRT
typedef struct xide_reference_shard xide_reference_shard;
typedef struct xide_reference_key xide_reference_key;
typedef struct xide_reference_family xide_reference_family;
typedef struct xide_reference_graph xide_reference_graph;
#define XIDE_REFERENCE_BYTES (64u*1024u*1024u)
/* Independently owned immutable projections of a successful checked owner.
 * Constructors verify the complete supplied source vector against that owner,
 * then use the original navigation binding/overload/type rules. The optional
 * navigation cache avoids rebuilding its table. Neither value retains an AST,
 * check, cache, worker or borrowed text. Quota charges all owned payload bytes;
 * allocator metadata and temporary navigation construction are additional.
 * No filesystem, parsing, checking or compatibility JSON is performed here. */
xide_reference_shard* xideSemanticReferenceShardCreate(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,
 size_t bytes,xide_model_error* error);
/* NULL with an unchanged error means no navigable declaration at this cursor.
 * Identity is the lexical path, complete exact post-BOM source and declaration
 * byte interval, never a compilation-local numeric ID or symbol spelling. */
xide_reference_key* xideSemanticReferenceKeyCreate(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,
 const char* path,int line,int character,size_t bytes,xide_model_error* error);
/* Same original binding decision, additionally capture its cursor occurrence.
 * The occurrence path borrows the supplied sources (not the navigation table);
 * byte offsets address post-BOM input, UTF16 positions keep the BOM prefix.
 * Clear selected on missing/failed lookup. No extra navigation construction. */
xide_reference_key* xideSemanticReferenceKeyCreateAt(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,
 const char* path,int line,int character,size_t bytes,
 xide_semantic_reference* selected,xide_model_error* error);
/* Borrow the independently owned exact declaration source and byte interval.
 * Storage/identity only: not writable-scope, freshness or rename-safety proof. */
int xideSemanticReferenceKeyTarget(const xide_reference_key*,xide_semantic_source*,size_t* start,size_t* end);
/* Native IDENT and UTF16 length, captured once with the immutable key. No
 * allocation/scan on read. Lexical validity alone never authorizes rename. */
int xideSemanticReferenceKeyIdentifier(const xide_reference_key*,xstrview* name,int* utf16Units);
xide_reference_shard* xideSemanticReferenceShardRetain(xide_reference_shard* shard);
void xideSemanticReferenceShardRelease(xide_reference_shard* shard);
xide_reference_key* xideSemanticReferenceKeyRetain(xide_reference_key* key);
void xideSemanticReferenceKeyRelease(xide_reference_key* key);
size_t xideSemanticReferenceShardBytes(const xide_reference_shard* shard);
size_t xideSemanticReferenceKeyBytes(const xide_reference_key* key);
size_t xideSemanticReferenceShardSources(const xide_reference_shard* shard);
/* Borrowed exact captured source bytes, paths and BOM conversion convention. */
int xideSemanticReferenceShardSource(const xide_reference_shard* shard,size_t index,xide_semantic_source* source);
typedef struct xide_reference_shard_view {
 const xide_reference_shard* shard;
 size_t first,count,excluded;
} xide_reference_shard_view;
enum xide_reference_match { XIDE_REFERENCE_INVALID=0,XIDE_REFERENCE_MATCH=1,XIDE_REFERENCE_SOURCE_CHANGED=2,XIDE_REFERENCE_IDENTIFIER_UNSUPPORTED=3 };
/* Cross-owner lookup compares exact declaration source bytes before using
 * sorted occurrence rows. Missing sources/targets yield an empty MATCH; a
 * same-path source mismatch is explicitly SOURCE_CHANGED, never a name search.
 * This is captured same-compilation data, NOT a project completeness/freshness
 * proof. The project scheduler must select compatible configuration contexts,
 * cover every configured member, validate all sources/resolution/overlays and
 * publication epochs, and deduplicate locations across shards before replying.
 * Keep the shard alive while using a view. Exact-path lookup and At allocate
 * nothing; alternate lexical path comparison can allocate, as in navigation.
 * Readers may run concurrently; construction failure changes no diagnostics. */
enum xide_reference_match xideSemanticReferenceShardQuery(const xide_reference_shard* shard,
 const xide_reference_key* key,int includeDeclaration,xide_reference_shard_view* view);
int xideSemanticReferenceShardAt(const xide_reference_shard_view* view,size_t index,xide_semantic_reference* reference);
/* Exact identifiers within the SAME already-bound native occurrence rows.
 * Preserve the original full-source identity and includeDeclaration rules.
 * Qualified native name/type tokens end in the declaration's IDENT: verify
 * the suffix bytes and both original lexer word boundaries, then subtract
 * its cached UTF16 width from the verified native endpoint. Query retains the
 * original source comparison and alternate-path allocation; At adds no parse,
 * AST walk, prefix scan, allocation or semantic binding decision.
 * A non-spelled/generated or incompatible range fails At rather than being
 * silently omitted. The complete rename query must reject such a failure.
 * The view borrows only the shard (not key/check/source vector); keep that
 * shard alive and use this view unchanged after successful construction. */
typedef struct xide_reference_identifier_view {
 xide_reference_shard_view references;xstrview name;int utf16Units;
 uint32_t targetSource,targetStart,targetEnd;
} xide_reference_identifier_view;
enum xide_reference_match xideSemanticReferenceShardIdentifiers(const xide_reference_shard*,
 const xide_reference_key*,int includeDeclaration,xide_reference_identifier_view*);
int xideSemanticReferenceShardIdentifierAt(const xide_reference_identifier_view*,size_t,xide_semantic_reference*);
enum xide_reference_family_error {
 XIDE_REFERENCE_FAMILY_OK,XIDE_REFERENCE_FAMILY_ABSENT,XIDE_REFERENCE_FAMILY_INVALID,
 XIDE_REFERENCE_FAMILY_LIMIT,XIDE_REFERENCE_FAMILY_MEMORY,XIDE_REFERENCE_FAMILY_CANCELLED,
 XIDE_REFERENCE_FAMILY_UNSUPPORTED
};
typedef struct xide_reference_family_info {size_t members,sources,bytes,relations;unsigned kinds;} xide_reference_family_info;
/* Demand-only observed class/protocol/lexical-overload/property component of
 * an exact source declaration key. Follow original verified slot/witness IDs
 * and same-owner native callable declaration associations transitively;
 * also join IDs which the existing navigation maps to the same exact source
 * declaration span. Never join unrelated declarations by spelling/signature.
 * Missing/generated/non-IDENT or differently spelled connected declarations
 * reject the WHOLE capture. Missing seed source/declaration is ABSENT.
 * This is NOT complete generic-origin/workspace closure,
 * writable/foreign policy, freshness, conflicts or permission to edit.
 * Copies each contributing source once; retains no check/AST/cache/shard/key.
 * Default32MiB/max64MiB admits concurrent callable capture/graph scratch plus
 * completed payload. Callable capture itself has a32MiB maximum.
 * Borrowed navigation/check/cache and allocator/sort internals are additional.
 * Cancellation polls bounded graph work and64KiB payload copies; original
 * navigation acquisition and the quota-bounded library sorts are not
 * preemptible. No graph is constructed for ordinary navigation/references. */
xide_reference_family* xideSemanticReferenceFamilyCreate(xls_projection_store*,xls_semantic_check*,
 const xide_semantic_source*,size_t,const xide_reference_key*,size_t,
 int (*cancelled)(void*),void*,enum xide_reference_family_error*);
xide_reference_family* xideSemanticReferenceFamilyRetain(xide_reference_family*);
void xideSemanticReferenceFamilyRelease(xide_reference_family*);
int xideSemanticReferenceFamilyInfo(const xide_reference_family*,xide_reference_family_info*);
/* New independently owned key lease, borrowing the retained family payload.
 * Outlives the family handle and original check. KeyBytes conservatively
 * charges its header plus the ENTIRE retained family, not merely this source.
 * No text copy/scan on this read; NULL on invalid index/allocation refusal. */
xide_reference_key* xideSemanticReferenceFamilyKey(xide_reference_family*,size_t);
/* Borrow one exact member without allocating a key. Keep the family alive. */
int xideSemanticReferenceFamilyMember(const xide_reference_family*,size_t,
 xide_semantic_source*,size_t* start,size_t* end);
typedef struct xide_reference_graph_info {
 size_t declarations,components,relations,sources,ownedBytes,bytes;
} xide_reference_graph_info;
/* Demand-only all-declaration components of ONE successful checked owner.
 * Build the ordinary reference shard/source bank once and share it with this
 * graph. Reuse original dispatch/callable relations and exact declaration
 * source origins; ordinary reference capture never constructs this graph.
 * Include all current source declaration spans (including singleton locals
 * and types). Connected missing/generated/non-IDENT/differently spelled
 * declarations mark that entire component unsupported, not unrelated ones.
 * No check/index/navigation/AST/cache is retained. Default/max64MiB charges
 * the retained shard, callable-capture peak, graph scratch and owned result;
 * borrowed navigation/check and allocator/sort internals are additional.
 * Cancellation polls graph work; original navigation, shard capture and
 * bounded library sorting are not preemptible. No I/O/parsing/checking/JSON.
 * This is not complete workspace/generic coverage or edit permission. */
xide_reference_graph* xideSemanticReferenceGraphCreate(xls_projection_store*,xls_semantic_check*,
 const xide_semantic_source*,size_t,size_t,int (*cancelled)(void*),void*,enum xide_reference_family_error*);
xide_reference_graph* xideSemanticReferenceGraphRetain(xide_reference_graph*);
void xideSemanticReferenceGraphRelease(xide_reference_graph*);
int xideSemanticReferenceGraphInfo(const xide_reference_graph*,xide_reference_graph_info*);
typedef struct xide_reference_graph_declaration {uint32_t source,start,end,component;int utf16Units;} xide_reference_graph_declaration;
typedef struct xide_reference_graph_component {size_t members,relations;unsigned kinds;int unsupported;} xide_reference_graph_component;
/* Allocation-free whole-graph streaming for cross-owner composition. Entries
 * are sorted by one-based source ordinal and exact declaration byte interval.
 * Source ordinals address the shared shard; component ordinals are LOCAL to
 * this graph, never semantic identities across different checked owners.
 * Include unsupported entries/components so callers cannot silently drop a
 * connected missing/generated/foreign spelling. Clear outputs on failure. */
int xideSemanticReferenceGraphDeclarationAt(const xide_reference_graph*,size_t,xide_reference_graph_declaration*);
int xideSemanticReferenceGraphComponentAt(const xide_reference_graph*,size_t,xide_reference_graph_component*);
/* Same immutable source/reference bank, independently retained. A cache that
 * already owns this exact shard charges graph.ownedBytes in addition to its
 * existing shard charge; graph.bytes conservatively includes both. */
xide_reference_shard* xideSemanticReferenceGraphRetainShard(xide_reference_graph*);
/* Exact source-key query after the original check has gone. Shared-bank keys
 * skip the whole-source comparison; independent keys prove complete source
 * bytes before a logarithmic declaration lookup. No source text is copied.
 * Only the newly allocated family header/member rows use this default32MiB,
 * max64MiB construction budget. FamilyInfo.bytes/KeyBytes conservatively
 * include the ENTIRE retained graph and shard as well. Identifier validity,
 * widths and component spelling were checked once during graph capture. */
xide_reference_family* xideSemanticReferenceGraphFamily(xide_reference_graph*,const xide_reference_key*,
 size_t,int (*cancelled)(void*),void*,enum xide_reference_family_error*);
/* Batch composition across independently checked owners. Sort source paths
 * using the existing native lexical identity, prove complete source/kind/BOM
 * agreement, join identical declaration spans and each provider's components.
 * No repeated analysis or per-discovered-member graph queries. Output copies
 * contributing sources once and retains no input graph. Relation counts are
 * provider observations, not deduplicated edges. Default/max128MiB charges
 * the complete borrowed graph payloads, scratch, retained native path keys
 * and output together; path conversion/sort/allocator internals are extra.
 * Caller must prove ALL configured scopes, including zero-target scopes,
 * and revalidate their guards/epochs before publishing. No edit permission. */
xide_reference_family* xideSemanticReferenceGraphsFamily(xide_reference_graph* const*,size_t,
 const xide_reference_key*,size_t,int (*cancelled)(void*),void*,enum xide_reference_family_error*);
#endif
#endif
