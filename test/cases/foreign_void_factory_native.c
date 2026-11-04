#include "foreign_void_factory.h"
#include "foreign_inplace_factory_native.c"
/* Fixture-only void producers report partial initialization through the XRT
 * error channel. Actual XGE MaterialInit is exercised separately. */
void inplace_void_plain_init(PlainRecord* output,int mode)
{
    int result=inplace_plain_init(output,mode);
    if(result && !xrtGetError())xrtSetErrorKind(XERR_STATE);
}
void inplace_void_init(InplaceRecord* output,int mode)
{
    int result=inplace_init(output,mode,(xstrview){"test",4});
    if(result && !xrtGetError())xrtSetErrorKind(XERR_STATE);
}
void inplace_void_plain_from(PlainRecord* source,PlainRecord* output)
{ assert(source && source->value==42);inplace_void_plain_init(output,0); }
