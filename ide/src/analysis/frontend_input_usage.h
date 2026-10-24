#ifndef XLANG_ANALYSIS_FRONTEND_INPUT_USAGE_H
#define XLANG_ANALYSIS_FRONTEND_INPUT_USAGE_H
#include <stddef.h>
#include <stdint.h>
struct XL6_SourceOverlay;
/* Same-thread observation of the original frontend's overlay lookups. Bind
 * only after construction; no source text, path policy, allocation or I/O.
 * A result may use the mask only while the entire overlay membership,
 * identities and paths are unchanged. Disk/probe/environment validation is
 * still mandatory. Failed lookups caused by resources make usage unknown. */
typedef struct xls_input_usage_scope {
    struct xls_input_usage_scope* previous;
    const struct XL6_SourceOverlay* overlay;
    uint64_t used;
    size_t count;
    int bound,failed;
} xls_input_usage_scope;
void xlsFrontendInputUsageBegin(xls_input_usage_scope* scope);
void xlsFrontendInputUsageBind(const struct XL6_SourceOverlay* overlay,size_t count);
void xlsFrontendInputUsageRead(const struct XL6_SourceOverlay* overlay,size_t index);
/* Returns zero, preserving the original matcher's failure behavior. */
int xlsFrontendInputUsageFailure(const struct XL6_SourceOverlay* overlay);
int xlsFrontendInputUsageEnd(xls_input_usage_scope* scope,uint64_t* used);
#endif
