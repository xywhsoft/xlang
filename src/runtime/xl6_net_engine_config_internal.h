#ifndef XL6_NET_ENGINE_CONFIG_INTERNAL_H
#define XL6_NET_ENGINE_CONFIG_INTERNAL_H
#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
typedef struct XL6_NetEngineConfigV1 {
    xnetengineconfig value;
    xnetbufpoolconfig buffers;
} XL6_NetEngineConfigV1;
static inline void xl6NetEngineFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
/* Copies of this object MUST rebind BufferPool to their own embedded storage.
 * No string, language callback, pool object or graph edge is retained. */
static inline void xl6NetEngineConfigRebindV1(XL6_NetEngineConfigV1* config)
{config->value.BufferPool=&config->buffers;}
#endif
