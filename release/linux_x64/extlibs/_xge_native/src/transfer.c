/* Cold exported-native-object boundaries only. Ordinary UI/draw calls stay
 * declarative. A borrowed caller capability is accepted during this call;
 * every successfully exported resident capsule retains its own real lease. */
#include <xge3_runtime.h>
#include <__xl_runtime/xl6_native_com.h>

static int transfer_begin(const XL6_CodeOwner* owner,const XL6_OwnershipDomainV1* domain,
    XL6_CodeLease** lease,xn_transfer_services_v1* services)
{
    *lease=NULL;
    XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquireScoped(owner,lease);
    if(admission!=XL6_CODE_LEASE_ACQUIRED)
        return admission==XL6_CODE_LEASE_RESOURCE_FAILURE ? XGE_ERROR_OUT_OF_MEMORY:XGE_ERROR_INVALID_STATE;
    bool ok=*lease ? xl6TransferServicesFromLeaseV1(*lease,domain,services) :
        xl6TransferServicesProcessResidentV1(domain,services);
    if(!ok){
        xerror* error=xrtTakeError();
        xl6CodeLeaseRelease(*lease);*lease=NULL;
        xrtClearError();xrtSetErrorTake(error);
        return XGE_ERROR_INVALID_STATE;
    }
    return XGE_OK;
}
static int transfer_end(XL6_CodeLease* lease,int status)
{
    xerror* error=xrtTakeError();
    xl6CodeLeaseRelease(lease);
    xrtClearError();xrtSetErrorTake(error);
    return status;
}
int xl6Xge3RunOwned(xge_scene_proc frame,void* user,const xge_callback_owner_t* owner,
    const XL6_CodeOwner* code)
{
    XL6_CodeLease* lease=NULL;xn_transfer_services_v1 services={0};
    int status=transfer_begin(code,xl6Xge3TransferDomainV1Get(),&lease,&services);
    if(status!=XGE_OK)return status;
    status=xgeRunOwnedWithTransfer(frame,user,owner,&services);
    return transfer_end(lease,status);
}
int xl6Xge3DragBegin(xge_data_object data,uint32_t allowed,uint32_t suggested,
    const XL6_CodeOwner* code)
{
    XL6_CodeLease* lease=NULL;xn_transfer_services_v1 services={0};
    int status=transfer_begin(code,xl6Xge3TransferDomainV1Get(),&lease,&services);
    if(status!=XGE_OK)return status;
    status=xgeDragBeginWithTransfer(data,allowed,suggested,&services);
    return transfer_end(lease,status);
}
int xl6Xge3CopyToClipboard(xge_data_object data,const XL6_CodeOwner* code)
{
    XL6_CodeLease* lease=NULL;xn_transfer_services_v1 services={0};
    int status=transfer_begin(code,xl6Xge3TransferDomainV1Get(),&lease,&services);
    if(status!=XGE_OK)return status;
    status=xgeDataObjectCopyToClipboardWithTransfer(data,&services);
    return transfer_end(lease,status);
}
int xl6Xui3CopyToClipboard(xui_context context,xui_data_object data,const XL6_CodeOwner* code)
{
    XL6_CodeLease* lease=NULL;xn_transfer_services_v1 services={0};
    int status=transfer_begin(code,xl6Xui3OwnershipDomainV1Get(),&lease,&services);
    if(status!=XGE_OK)return status==XGE_ERROR_OUT_OF_MEMORY ? XUI_ERROR_OUT_OF_MEMORY:XUI_ERROR_INVALID_STATE;
    status=xuiClipboardCopyDataWithTransfer(context,data,&services);
    return transfer_end(lease,status);
}
