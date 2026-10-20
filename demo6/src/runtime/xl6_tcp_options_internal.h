#ifndef XL6_TCP_OPTIONS_INTERNAL_H
#define XL6_TCP_OPTIONS_INTERNAL_H
#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
/* These immutable owners contain only by-value native configuration. They do
 * not retain a caller address, language object, callback, Engine or service. */
static inline void xl6TcpOptionsFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static inline bool xl6TcpOptionsSizeV1(uint64_t value)
{if(value<=SIZE_MAX)return true;xrtSetErrorKind(XERR_RANGE);xl6TcpOptionsFailedV1();return false;}
static inline bool xl6TcpOptionsRangeV1(bool valid)
{if(valid)return true;xrtSetErrorKind(XERR_RANGE);xl6TcpOptionsFailedV1();return false;}
/* The pinned TCP ABI uses uint64 microseconds/zero infinite. A finite zero
 * deadline is not representable: reject it rather than turn it into infinity.
 * Fallback delay zero is a real zero and uses the separate finite helper. */
static inline bool xl6TcpOptionsFiniteMillisV1(int64_t millis,uint64_t* micros)
{
    if(!xl6TcpOptionsRangeV1(millis>=0&&(uint64_t)millis<=UINT64_MAX/1000))return false;
    *micros=(uint64_t)millis*1000;return true;
}
static inline bool xl6TcpOptionsTimeoutV1(int64_t millis,uint64_t* micros)
{
    if(millis==-1){*micros=0;return true;}
    if(!xl6TcpOptionsRangeV1(millis>0))return false;
    return xl6TcpOptionsFiniteMillisV1(millis,micros);
}
static inline int64_t xl6TcpOptionsMillisV1(uint64_t micros)
{return (int64_t)(micros/1000+(micros%1000!=0));}
/* Separate provider, so merely using Dial/Listen need not include projections
 * which allocate a NetAddress. Copy returns an independent configuration. */
void* xl6TcpStreamOptionsCopyV1(const xnetstreamconfig*);
#endif
