#include "foreign_value_projection.h"
#include "foreign_callback_span_native.c"
int inplace_plain_init(PlainRecord* record,int mode)
{
    assert(!mode && record && !record->value && !record->payload);
    record->payload=xrtMalloc(1);if(!record->payload)return -1;
    record->value=42;return 0;
}
void inplace_plain_free(PlainRecord* record)
{ xrtFree(record->payload);*record=(PlainRecord){0}; }
static bool projection_destroy_plain(void* raw)
{ inplace_plain_free(raw);xrtFree(raw);return true; }
const XL6_ForeignViewDescriptorV1 inplacePlainPolicy={sizeof(inplacePlainPolicy),
    XL6_FOREIGN_VIEW_V1_TAG,"projection.plain",0,1,NULL,NULL,projection_destroy_plain,0,1,NULL,NULL,NULL,NULL,0,projection_destroy_plain,NULL};
/* Independent native consumers: the language emitter must pass these exact
 * records by value, rather than changing the provider's ABI. */
int value_projection_size(xbytesview input)
{
    assert(input.Size==0 || (input.Size==3 && input.Data));
    if(input.Size)assert(!memcmp(input.Data,"a\0b",3));
    return (int)input.Size;
}
int value_projection_pair(xbytesview a,xbytesview b)
{ return value_projection_size(a)+value_projection_size(b); }
int value_projection_record(PlainRecord record)
{ assert(record.payload);return record.value; }
