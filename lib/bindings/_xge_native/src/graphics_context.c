/* Cold code ownership admission. Context methods and drawing remain direct. */
#include <xge3_runtime.h>
int xl6Xge3GraphicsContextCreateEglOwned(xge_graphics_context* out,int width,int height,bool surfaceless,
 const XL6_CodeOwner* owner)
{
 if(!out)return XGE_ERROR_INVALID_ARGUMENT;
 *out=NULL;
#if !XGE_ENABLE_EGL
 (void)owner;return xgeGraphicsContextCreateEglOwned(out,width,height,surfaceless,NULL);
#else
 XL6_CodeLease* lease=NULL;
 XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquireScoped(owner,&lease);
 if(admission!=XL6_CODE_LEASE_ACQUIRED)
  return admission==XL6_CODE_LEASE_RESOURCE_FAILURE ? XGE_ERROR_OUT_OF_MEMORY:XGE_ERROR_INVALID_STATE;
 xge_callback_owner_t code={xl6CodeLeaseOwnership(lease),xl6CodeLeaseHoldNativeV1,xl6CodeLeaseDropNativeV1};
 int status=xgeGraphicsContextCreateEglOwned(out,width,height,surfaceless,lease ? &code:NULL);
 xerror* error=xrtTakeError();xl6CodeLeaseRelease(lease);xrtClearError();xrtSetErrorTake(error);
 return status;
#endif
}
