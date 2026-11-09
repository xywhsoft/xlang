#ifndef XL6_XGE3_CHECKED_GRAPHICS_H
#define XL6_XGE3_CHECKED_GRAPHICS_H
#include <xge3_runtime.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>
/* Native owner consumption and checked failure are independent. */
static inline bool xl6Xge3GraphicsConsumed(int status,bool consumed,const char* operation)
{
 if(status!=XGE_OK){
  if(!xrtGetError())xl6ForeignStatusFailureV1(status,operation);
  xl6_error_result failure={false,xrtTakeError()};
  if(!xl6ErrorScopeTryPropagateFailureV1(&failure))xl6CallbackFailureRecordV1(failure);
 }
 return consumed;
}
#endif
