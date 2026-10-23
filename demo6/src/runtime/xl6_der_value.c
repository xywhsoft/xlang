#include "xl6_der_internal.h"

void* xl6DerParseV1(const xvalue* data,int64_t size,int64_t limit)
{xbytesview input;return xl6BufferBytesInput(data,size,&input) && xl6DerBound(input,limit)?xl6DerCopyValue(input):NULL;}
void xl6DerValueDestroyV1(void* raw)
{
    XL6_DerValue* self=raw;if (!self) return;
    xl6CryptoFreeSecret(self->data,self->value.Raw.Size);xl6CryptoFreeSecret(self,sizeof(*self));
}
int64_t xl6DerTagClassV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);return self?self->value.Tag.Class:0;}
int64_t xl6DerTagNumberV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);return self?self->value.Tag.Number:0;}
bool xl6DerConstructedV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);return self && self->value.Tag.Constructed;}
xvalue* xl6DerRawV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);return self?xrtValueBytes(self->value.Raw):NULL;}
xvalue* xl6DerContentV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);return self?xrtValueBytes(self->value.Value):NULL;}
bool xl6DerBooleanV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);bool result=false;if (self) xrtDerBoolean(&self->value,&result);return result;}
int64_t xl6DerInt64V1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);int64 result=0;if (self) xrtDerInt64(&self->value,&result);return result;}
uint64_t xl6DerUInt64V1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);uint64 result=0;if (self) xrtDerUInt64(&self->value,&result);return result;}
xvalue* xl6DerUnsignedV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);xbytesview result;return self && xrtDerUnsigned(&self->value,&result)?xrtValueBytes(result):NULL;}
xvalue* xl6DerOctetsV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);xbytesview result;return self && xrtDerOctets(&self->value,&result)?xrtValueBytes(result):NULL;}
xvalue* xl6DerBitsV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);xbytesview result;uint8 unused;return self && xrtDerBitString(&self->value,&result,&unused)?xrtValueBytes(result):NULL;}
int64_t xl6DerUnusedBitsV1(void* raw)
{XL6_DerValue* self=xl6DerChecked(raw);xbytesview result;uint8 unused=0;if (self) xrtDerBitString(&self->value,&result,&unused);return unused;}
xstrview xl6DerOidTextV1(void* raw)
{
    XL6_DerValue* self=xl6DerChecked(raw);xbytesview oid;xbuffer result={0};
    if (!self || !xrtDerOid(&self->value,&oid) || !xrtDerOidDecode(oid,&result)) {
        xl6CryptoFreeSecret(result.Data,result.Size);return xl6TextFailure;
    }
    /* Transfer the decoder's storage rather than copying it through STRING.
     * Buffer has no implicit terminator; the language string ABI does. */
    if (result.Size>=INT64_MAX) {
        xrtSetErrorKind(XERR_RANGE);xl6CryptoFreeSecret(result.Data,result.Size);return xl6TextFailure;
    }
    size_t size=result.Size;
    if (!xrtBufferAppend(&result,(xbytesview){(const uint8*)"",1})) {
        xl6CryptoFreeSecret(result.Data,result.Size);return xl6TextFailure;
    }
    bytes data=xrtBufferTake(&result,NULL,NULL);
    return (xstrview){(const char*)data,size};
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6DerValueHandlePolicyV1Get,xl6DerValueDestroyV1)
