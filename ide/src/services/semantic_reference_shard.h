#ifndef XIDE_SEMANTIC_REFERENCE_SHARD_H
#define XIDE_SEMANTIC_REFERENCE_SHARD_H
#include "services/semantic_definition.h"
#ifdef XLS_COMPILER_XRT
typedef struct xide_reference_shard xide_reference_shard;
typedef struct xide_reference_key xide_reference_key;
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
enum xide_reference_match { XIDE_REFERENCE_INVALID=0,XIDE_REFERENCE_MATCH=1,XIDE_REFERENCE_SOURCE_CHANGED=2 };
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
#endif
#endif
