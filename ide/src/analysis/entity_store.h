#ifndef XLANG_ANALYSIS_ENTITY_STORE_H
#define XLANG_ANALYSIS_ENTITY_STORE_H
#include "analysis/context.h"
typedef struct xls_entity_store xls_entity_store;
typedef struct xls_entity_domain xls_entity_domain;
typedef struct xls_entity_limits {size_t domains,entities,keyBytes,bytes;} xls_entity_limits;
typedef struct xls_entity_stats {size_t domains,entities,bytes,limit;uint64_t hits,admitted,declined;} xls_entity_stats;
/* Store-scoped external declaration identities,not compiler SymbolId/TypeId,
 * current-source proofs or semantic cache versions. Caller constructs a complete
 * declaration key from VERIFIED syntax/bindings;this API does not resolve names.
 * XRT-owned,thread-safe,monotonic IDs,never reused while this store lives.
 * Defaults1024domains/262144entities/64KiB perkey/64MiB known owned data;
 * allocator,transient/caller-owned data and whole-model RSS are separate.
 * All allocations and complete context equality happen outside metadata locks.
 * Resource/limit failure returnsNULL/0 without affecting analysis diagnostics. */
xls_entity_store* xlsEntityStoreCreate(const xls_entity_limits* limits);
xls_entity_store* xlsEntityStoreRetain(xls_entity_store* store);
void xlsEntityStoreRelease(xls_entity_store* store);
/* Borrowed immutable domain;retain store throughout use. Complete context
 * equality is required,hash collisions never establish identity. At64 distinct
 * candidates in one hash bucket,unknown new contexts conservatively decline. */
const xls_entity_domain* xlsEntityStoreDomain(xls_entity_store* store,xls_context* context);
uint64_t xlsEntityDomainIdentity(const xls_entity_domain* domain);
const xls_context* xlsEntityDomainContext(const xls_entity_domain* domain);
/* module belongs to the owning shared module store's namespace. Binary keys
 * are compared completely,may containNUL,are copied,and must be nonempty. */
uint64_t xlsEntityStoreIntern(xls_entity_store* store,const xls_entity_domain* domain,
 uint64_t module,const void* key,size_t size);
void xlsEntityStoreStats(xls_entity_store* store,xls_entity_stats* stats);
#endif
