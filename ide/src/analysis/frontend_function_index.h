#ifndef XLS_FRONTEND_FUNCTION_INDEX_H
#define XLS_FRONTEND_FUNCTION_INDEX_H
#include "analysis/index.h"
#include "analysis/typed_module.h"

typedef struct xls_function_index_asset xls_function_index_asset;
typedef struct xls_function_index_info {size_t first,facts,recordBytes,canonicalBytes,sharedBytes;} xls_function_index_info;
typedef struct xls_function_index_source {
    const char* path;size_t size,rawSize;int kind;
    unsigned char textSha256[32],rawSha256[32];
} xls_function_index_source;
/* Optional normal collector storage. Zero metadata allowance keeps the original
 * flat typed collector. Both paths retain the original total index budget. */
typedef struct xls_index_storage_control {struct xls_index_storage_control* previous;size_t bytes;} xls_index_storage_control;
void xlsFrontendIndexStorageBegin(xls_index_storage_control*,size_t bytes);
void xlsFrontendIndexStorageEnd(xls_index_storage_control*);
/* Bind segments only to the SAME finalized native producer. No tree/fact or
 * canonical-string copy;failure preserves the ordinary index. */
int xlsFrontendIndexBindFunctions(xls_semantic_index*,const xls_typed_module*);
xls_function_index_asset* xlsFrontendIndexFunctionAsset(const xls_semantic_index*,const xls_typed_function*);
xls_function_index_asset* xlsFunctionIndexAssetRetain(xls_function_index_asset*);
void xlsFunctionIndexAssetRelease(xls_function_index_asset*);
xls_function_index_info xlsFunctionIndexAssetInfo(const xls_function_index_asset*);
/* Original collector IDs/parent/source domains. These are NOT automatically
 * valid in a later compilation. Returned values borrow the retained asset.
 * Unrelated fact buffers and the old index/check/report/text are not retained. */
int xlsFunctionIndexAssetFact(const xls_function_index_asset*,size_t,xls_index_fact*);
int xlsFunctionIndexAssetPointFact(const xls_function_index_asset*,size_t,xls_index_point_fact*);
size_t xlsFunctionIndexAssetSourceCount(const xls_function_index_asset*);
int xlsFunctionIndexAssetSource(const xls_function_index_asset*,size_t,xls_function_index_source*);
#endif
