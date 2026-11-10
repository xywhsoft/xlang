#ifndef XIDE_SEMANTIC_REFERENCE_SHARD_H
#define XIDE_SEMANTIC_REFERENCE_SHARD_H
#include "services/semantic_definition.h"
#ifdef XLS_COMPILER_XRT
typedef struct xide_reference_shard xide_reference_shard;
typedef struct xide_reference_key xide_reference_key;
typedef struct xide_reference_family xide_reference_family;
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
/* Demand-only observed CLASS/PROTOCOL dispatch component of an exact source
 * declaration key. Follow original verified slot/witness IDs transitively;
 * also join IDs which the existing navigation maps to the same exact source
 * declaration span. Never join unrelated declarations by spelling/signature.
 * Missing/generated/non-IDENT or differently spelled connected declarations
 * reject the WHOLE capture. Missing seed source/declaration is ABSENT.
 * This is NOT complete overload/property/generic-origin/workspace closure,
 * writable/foreign policy, freshness, conflicts or permission to edit.
 * Copies each contributing source once; retains no check/AST/cache/shard/key.
 * Default32MiB/max64MiB admits concurrent scratch plus completed payload.
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
#endif
#endif
