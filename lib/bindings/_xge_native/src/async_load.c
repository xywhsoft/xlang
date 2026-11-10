/* Cold code ownership admission. Ordinary request methods call XGE directly. */
#include <xge3_runtime.h>
#define XL6_XGE_ASYNC_FACTORY(Name,Native) \
int Name(xge_async_owned* output,const XL6_CodeOwner* owner) \
{ \
 if(!output)return XGE_ERROR_INVALID_ARGUMENT; \
 *output=NULL;XL6_CodeLease* lease=NULL; \
 XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquireScoped(owner,&lease); \
 if(admission!=XL6_CODE_LEASE_ACQUIRED) \
  return admission==XL6_CODE_LEASE_RESOURCE_FAILURE ? XGE_ERROR_OUT_OF_MEMORY:XGE_ERROR_INVALID_STATE; \
 xge_callback_owner_t code={xl6CodeLeaseOwnership(lease),xl6CodeLeaseHoldNativeV1,xl6CodeLeaseDropNativeV1}; \
 int status=Native(output,lease ? &code:NULL); \
 xerror* error=xrtTakeError();xl6CodeLeaseRelease(lease);xrtClearError();xrtSetErrorTake(error); \
 return status; \
}
XL6_XGE_ASYNC_FACTORY(xl6Xge3AsyncImageCreateOwned,xgeAsyncImageCreateOwned)
XL6_XGE_ASYNC_FACTORY(xl6Xge3AsyncTextureCreateOwned,xgeAsyncTextureCreateOwned)
#undef XL6_XGE_ASYNC_FACTORY
