#include "test/cases/foreign_xge3_binding_rollback.h"
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>
#include <assert.h>
struct BindingRollbackRoot { int mode; };
static int live,calls;
static void diagnostic(int code,const char* text)
{xerrordesc d={0};d.Kind=XERR_STATE;d.Code=code;d.Domain="binding.rollback";d.Message=text;xrtSetErrorTake(xrtErrorBuild(&d));}
int binding_rollback_create(int mode,BindingRollbackRoot** output)
{
 if(!output)return -1;
 *output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;
 (*output)->mode=mode;++live;
 if(mode==1){diagnostic(91,"primary producer error");return -7;}
 if(mode==2)return -7;
 if(mode==3)assert(xrtMemDebugFailAfter(0));
 return 0;
}
int binding_rollback_release(BindingRollbackRoot* value)
{
 assert(value);int mode=value->mode;++calls;--live;xrtFree(value);
 if(mode==1){diagnostic(92,"secondary rollback error");return -9;}
 if(mode==2)return -9;
 if(mode==3){diagnostic(92,"unallocated rollback error");return -9;}
 return 0;
}
int binding_rollback_release_42(BindingRollbackRoot* value)
{int status=binding_rollback_release(value);return status ? status:42;}
void binding_rollback_reset(void){xrtMemDebugFailClear();}
int binding_rollback_live(void){return live;}
int binding_rollback_calls(void){return calls;}
static bool destroy(void* raw)
{
 int status=binding_rollback_release(raw);
 if(status){
  if(!xrtGetError())xl6ForeignStatusFailureV1(status,"binding_rollback_release");
  xl6_error_result failure={false,xrtTakeError()};
  if(!xl6ErrorScopeTryPropagateFailureV1(&failure))xl6CallbackFailureRecordV1(failure);
 }
 return true;
}
const XL6_ForeignViewDescriptorV1 bindingRollbackPolicy={
 sizeof(bindingRollbackPolicy),XL6_FOREIGN_VIEW_V1_TAG,"binding.rollback.root",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
