#ifndef XLS_PRIVATE_RAW_BODY_PROJECTION_H
#define XLS_PRIVATE_RAW_BODY_PROJECTION_H
#include "analysis/frontend_internal.h"
typedef struct xls_raw_body_projection xls_raw_body_projection;
typedef struct syntax_unit xls_raw_unit;
typedef struct xls_raw_body_work {
    uint64_t deferred,materialized,declined,sizingPasses;
    size_t retainedBytes,limit;
    int complete,detached;
} xls_raw_body_work;
/* RAW syntax mechanism only. Shared declaration identity does not prove any
 * import,stdlib,semantic,ID or IR equivalence. A native semantic consumer must
 * establish those proofs before using a header root. Not a typed result API.
 * Current/previous syntax are pinned before construction; only independently
 * owned immutable declaration chunks survive in the returned object. */
xls_raw_body_projection* xlsRawBodyProjectionCreate(const xls_syntax* current,
    const xls_syntax* previous,size_t bytes);
/* Internal native producer policy. Inputs are pinned before any callback.
 * The policy must prove preprocessing/semantic admission separately. A
 * nonzero cachedBytes must be an already measured value of this exact unit. */
typedef int (*xls_raw_body_admit)(xls_raw_unit*,const char*,const XL6_AstRange*,size_t* cachedBytes,void*);
typedef void (*xls_raw_body_record)(xls_raw_unit*,const char*,XL6_AstNode*,int deferred,void*);
xls_raw_body_projection* xlsRawBodyProjectionCreatePolicy(const xls_syntax*,size_t,
    xls_raw_body_admit,xls_raw_body_record,void*);
xls_raw_unit* xlsRawUnitRetain(xls_raw_unit*);
void xlsRawUnitRelease(xls_raw_unit*);
const XL6_AstNode* xlsRawUnitNode(const xls_raw_unit*);
int xlsRawUnitBytes(const xls_raw_unit*,size_t limit,size_t* bytes);
/* Transfers construction-private headers for a native caller that will
 * prove/restore or reject ALL omitted bodies before publishing any result.
 * This handle no longer exposes an AST after transfer;its leases may close. */
int xlsRawBodyProjectionTakeHeaderParse(xls_raw_body_projection*,XL6_ParseResult*);
xls_raw_body_projection* xlsRawBodyProjectionRetain(xls_raw_body_projection*);
void xlsRawBodyProjectionRelease(xls_raw_body_projection*);
/* Construction-thread private,unpublished RAW headers. Never expose as a
 * complete AST or send to the ordinary checker without preprocessing proof. */
const XL6_AstNode* xlsRawBodyProjectionHeaders(const xls_raw_body_projection*);
/* Whole RAW tree only. Independent body copies publish atomically per job.
 * Failure returns NULL and can retry;no incomplete tree is exposed. */
const XL6_AstNode* xlsRawBodyProjectionAst(xls_raw_body_projection*);
xls_raw_body_work xlsRawBodyProjectionWork(xls_raw_body_projection*);
#ifdef XLS_RAW_BODY_VERIFY_OWNERS
uint64_t xlsRawBodyProjectionOwnedLeases(void);
#endif
#endif
