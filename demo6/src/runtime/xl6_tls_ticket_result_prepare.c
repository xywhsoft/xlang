#include "xl6_tls_ticket_result.h"
xvalue* xl6TlsTicketResultPrepareV1(XL6_NativeClassResultContextV1 context,XL6_CodeLease* lease,
    bool derived,XL6_TlsTicketResultV1** output)
{
    if(!output||*output||context.field.policy!=xl6TlsTicketResultHandlePolicyV1Get()||
        !xl6NativeClassResultContextValidV1(context)){
        if(!xrtGetError())xl6TlsFailV1(XERR_ARGUMENT,"invalid TLS ticket result recipe");
        return NULL;
    }
    if(xrtGetError())return NULL;
    XL6_TlsTicketResultV1* self=xrtCalloc(1,sizeof(*self));if(!self)return NULL;
    self->status=XTLS_AGAIN; /* A valid empty result, never exposed unfinished. */
    xvalue* prepared=derived?xl6NativeClassResultPrepareFromLeaseV1(context,lease):xl6NativeClassResultPrepareV1(context);
    if(!prepared){xl6TlsTicketResultDestroyV1(self);return NULL;}
    void* owned=self;xvalue* result=xl6NativeClassResultCommitTakeV1(context,&prepared,&owned);
    if(result)*output=self;
    return result;
}
bool xl6TlsTicketResultSetV1(XL6_TlsTicketResultV1* self,xtlsresult status,xtlsresume** owned)
{
    if(!self||!owned||self->resume||self->status!=XTLS_AGAIN||
        (status!=XTLS_OK&&status!=XTLS_AGAIN&&status!=XTLS_CLOSED)||
        ((status==XTLS_OK)!=(*owned!=NULL))){
        return xl6TlsFailV1(XERR_STATE,"inconsistent TLS ticket producer result");
    }
    self->status=status;self->resume=*owned;*owned=NULL;return true;
}
