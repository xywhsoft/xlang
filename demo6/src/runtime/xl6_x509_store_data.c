#include "xl6_x509_store_internal.h"
#include "xl6_buffer_internal.h"

int64_t xl6X509StoreAddEncodedV1(void* raw,xstrview text,int64_t size)
{
    return xl6TextSelectRaw(&text,size)?
        xl6X509StoreAddDataSpanV1(raw,(xbytesview){(const uint8*)text.Data,text.Size}):0;
}
int64_t xl6X509StoreAddEncodedBytesV1(void* raw,xbytesview input,int64_t size)
{
    return xl6BufferSelect(&input,size)?xl6X509StoreAddDataSpanV1(raw,input):0;
}
int64_t xl6X509StoreAddEncodedBufferV1(void* raw,void* buffer,int64_t size)
{
    xbytesview input={0};
    return xl6BufferInput(buffer,size,&input)?xl6X509StoreAddDataSpanV1(raw,input):0;
}
