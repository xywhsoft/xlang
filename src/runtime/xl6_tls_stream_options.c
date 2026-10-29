#include "xl6_tls_stream_options_internal.h"
#include "xl6_tls_stream_options_abi.h"

void* xl6TlsStreamOptionsCopyV1(const xtlsstreamconfig* value)
{
    if(xrtGetError())return NULL;
    if(!value){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS stream configuration");return NULL;}
    if(!value->AsyncBytesLimit||!value->AsyncCountLimit||!value->AsyncBatch){
        xl6TlsFailV1(XERR_RANGE,"TLS asynchronous limits must be positive");return NULL;}
    if(value->HandshakeTimeout>INT64_MAX||value->CloseTimeout>INT64_MAX){
        xl6TlsFailV1(XERR_RANGE,"TLS timeout exceeds signed milliseconds");return NULL;}
    xtlsstreamconfig* result=xrtMalloc(sizeof(*result));if(result)*result=*value;return result;
}
void xl6TlsStreamOptionsDefaultsV1(xtlsstreamconfig* output)
{
    /* Defaults are public XRT constants, not a second TLS implementation.
     * Calling the backend's trivial Init would select its whole TLS/TCP
     * module merely to construct an options record. Compare Init in tests. */
    const xtlsstreamconfig value={
        .HandshakeTimeout=XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT,
        .CloseTimeout=XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT,
        .AsyncBytesLimit=XTLS_STREAM_ASYNC_BYTES_DEFAULT,
        .AsyncCountLimit=XTLS_STREAM_ASYNC_COUNT_DEFAULT,
        .AsyncBatch=XTLS_STREAM_ASYNC_BATCH_DEFAULT
    };
    *output=value;
}
void* xl6TlsStreamOptionsCreateV1(void)
{xtlsstreamconfig value;xl6TlsStreamOptionsDefaultsV1(&value);return xl6TlsStreamOptionsCopyV1(&value);}
void xl6TlsStreamOptionsDestroyV1(void* raw){xrtFree(raw);}
static const xtlsstreamconfig* checked(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS stream options");
    return raw;
}
/* Native configuration uses milliseconds; zero disables. Match TCP's language
 * convention: -1 disables, finite zero is rejected rather than made infinite. */
static bool timeout(int64_t millis,uint64* result)
{
    if(millis==-1){*result=0;return true;}
    if(millis<=0)
        return xl6TlsFailV1(XERR_RANGE,"TLS timeout must be positive milliseconds or -1");
    *result=(uint64)millis;return true;
}
static int64_t milliseconds(uint64 value)
{return value?(int64_t)value:-1;}
#define XL6_TLS_STREAM_MILLIS(Field,Name) int64_t xl6TlsStreamOptions##Field##V1(void* raw){const xtlsstreamconfig* self=checked(raw);return self?milliseconds(self->Field):0;}
#define XL6_TLS_STREAM_BYTES(Field,Name) uint64_t xl6TlsStreamOptions##Field##V1(void* raw){const xtlsstreamconfig* self=checked(raw);return self?(uint64_t)self->Field:0;}
#define XL6_TLS_STREAM_COUNT(Field,Name) int64_t xl6TlsStreamOptions##Field##V1(void* raw){const xtlsstreamconfig* self=checked(raw);return self?(int64_t)self->Field:0;}
#include "xl6_tls_stream_options_fields.inc"
#undef XL6_TLS_STREAM_MILLIS
#undef XL6_TLS_STREAM_BYTES
#undef XL6_TLS_STREAM_COUNT
void* xl6TlsStreamOptionsWithTimeoutsV1(void* raw,int64_t handshake,int64_t close)
{
    const xtlsstreamconfig* self=checked(raw);if(!self)return NULL;
    xtlsstreamconfig value=*self;
    if(!timeout(handshake,&value.HandshakeTimeout)||!timeout(close,&value.CloseTimeout))return NULL;
    return xl6TlsStreamOptionsCopyV1(&value);
}
void* xl6TlsStreamOptionsWithAsyncLimitsV1(void* raw,uint64_t bytes,int64_t count,int64_t batch)
{
    const xtlsstreamconfig* self=checked(raw);if(!self)return NULL;
    if(!bytes||bytes>SIZE_MAX||count<=0||(uint64_t)count>UINT32_MAX||batch<=0||(uint64_t)batch>UINT32_MAX){
        xl6TlsFailV1(XERR_RANGE,"TLS asynchronous limits exceed their positive native ranges");return NULL;}
    xtlsstreamconfig value=*self;value.AsyncBytesLimit=(size_t)bytes;
    value.AsyncCountLimit=(uint32)count;value.AsyncBatch=(uint32)batch;
    return xl6TlsStreamOptionsCopyV1(&value);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsStreamOptionsHandlePolicyV1Get,xl6TlsStreamOptionsDestroyV1)
