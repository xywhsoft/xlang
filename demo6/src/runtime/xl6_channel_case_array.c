#include "xl6_text_value.h"
#include "xl6_channel_case_array.h"
#include "xl6_channel_case.h"
#include "xl6_channel_internal.h"
#include "xl6_channel_wait_common.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>

typedef struct Cases {
    size_t count;
    xvalue** owners;
    XL6_ChannelCaseOwnerV1** values;
    xvalue* inlineOwners[8];
    XL6_ChannelCaseOwnerV1* inlineValues[8];
} Cases;
static bool capture(Cases* frame,XL6_ChannelCaseArrayContextV1 context,const xvalue* input)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    XL6_Sequence* owner=NULL;XL6_SequenceAdmissionV3 admission={0};bool ok=false;
    if(!context.fieldName || !context.fieldSize || context.policy!=xl6ChannelCaseHandlePolicyV1Get() ||
        context.layout.transfer!=3 || context.layout.size!=sizeof(xvalue*)) {
        xrtSetErrorKind(XERR_ARGUMENT);goto done;
    }
    XL6_SequenceLayoutV1 layout=context.layout;
    owner=xl6SequenceBeginRoleV1(input,XL6_SEQUENCE_ARRAY_V1,layout.identity,layout.elementIdentity,
        layout.transfer,layout.size,layout.type);
    if(!owner || !xl6SequenceAdmissionCopyV3(owner,&admission))goto done;
    if(!admission.ops || !admission.data){xrtSetErrorKind(XERR_TYPE);goto done;}
    xtypedarray* storage=xl6SequenceStorageV1(owner);size_t count=xrtTypedArrayCount(storage);
    if(!count){xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    if(count>=UINT32_MAX || count>INT64_MAX || count>SIZE_MAX/sizeof(*frame->owners) ||
        count>SIZE_MAX/sizeof(*frame->values)){xrtSetErrorKind(XERR_RANGE);goto done;}
    if(count<=8){frame->owners=frame->inlineOwners;frame->values=frame->inlineValues;}
    else {
        frame->owners=xrtCalloc(count,sizeof(*frame->owners));
        frame->values=xrtCalloc(count,sizeof(*frame->values));
        if(!frame->owners || !frame->values)goto done;
    }
    for(size_t i=0;i<count;++i) {
        xvalue* const* slot=xrtTypedArrayConstGet(storage,i);
        if(!slot || !xl6SequenceAdmitSlotV1(owner,slot))goto done;
        frame->owners[i]=xrtValueRetain(*slot);if(!frame->owners[i])goto done;
        frame->count=i+1;
        xvalue* field=xrtValueObjectGet(frame->owners[i],(xstrview){context.fieldName,context.fieldSize});
        void* raw=NULL;const xvaluehandleops* ops=NULL;void* user=NULL;
        if(!field || !xrtValueGetHandle(field,&raw,&ops,&user) || !raw ||
            ops!=context.policy->Ops || user){xrtSetErrorKind(XERR_TYPE);goto done;}
        frame->values[i]=raw;
    }
    ok=true;
done:
    if(!ok || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&admission);
    if(owner)xl6SequenceEndV1(owner);
    if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return false;}
    xrtErrorFree(result.error);return true;
}
static void discard(Cases* frame)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,false))abort();
    for(size_t i=frame->count;i>0;--i) {
        xrtValueRelease(frame->owners[i-1]);
        if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    }
    if(frame->owners && frame->owners!=frame->inlineOwners)xrtFree(frame->owners);
    if(frame->values && frame->values!=frame->inlineValues)xrtFree(frame->values);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);else xrtErrorFree(result.error);
}
void* xl6ChannelCasesLanguageInvokeV1(XL6_ChannelCaseArrayContextV1 context,const xvalue* input,
    XL6_ChannelWaitOptionsV2 options,XL6_ChannelCasesInvokeV1 invoke,xcancel* token)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    Cases frame={0};XL6_ChannelSelectionOwnerV1* result=NULL;uint64_t deadline;
    /* All metadata capture/copy time belongs to a finite wait's budget. */
    bool ok=invoke && xl6ChannelWaitDeadline(options,&deadline);
    if(ok && options.mode==XL6_CHANNEL_FOR_V1)
        options=(XL6_ChannelWaitOptionsV2){sizeof(options),XL6_CHANNEL_UNTIL_V2,0,deadline};
    if(ok)ok=capture(&frame,context,input);
    if(ok){result=invoke(frame.values,frame.count,options,token);ok=result!=NULL;}
    if(!ok || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    discard(&frame);
    if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ChannelSelectionDestroyV1(result);xl6ErrorScopePropagateFailure(outcome);return NULL;}
    xrtErrorFree(outcome.error);return result;
}
