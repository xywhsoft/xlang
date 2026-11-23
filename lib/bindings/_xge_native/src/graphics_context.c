/* Cold code ownership admission. Context methods and drawing remain direct. */
#include <xge3_runtime.h>

#if defined(_WIN32) || XGE_ENABLE_EGL
static int acquireContextCode(const XL6_CodeOwner* owner,XL6_CodeLease** lease,xge_callback_owner_t* code)
{
 XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquireScoped(owner,lease);
 if(admission!=XL6_CODE_LEASE_ACQUIRED)
  return admission==XL6_CODE_LEASE_RESOURCE_FAILURE ? XGE_ERROR_OUT_OF_MEMORY:XGE_ERROR_INVALID_STATE;
 *code=(xge_callback_owner_t){xl6CodeLeaseOwnership(*lease),xl6CodeLeaseHoldNativeV1,xl6CodeLeaseDropNativeV1};
 return XGE_OK;
}

static void releaseContextCode(XL6_CodeLease* lease)
{
 xerror* error=xrtTakeError();
 xl6CodeLeaseRelease(lease);
 xrtClearError();
 xrtSetErrorTake(error);
}
#endif

int xl6Xge3GraphicsContextCreateEglOwned(xge_graphics_context* out,int width,int height,bool surfaceless,
 const XL6_CodeOwner* owner)
{
 if(!out)return XGE_ERROR_INVALID_ARGUMENT;
 *out=NULL;
#if !XGE_ENABLE_EGL
 (void)owner;return xgeGraphicsContextCreateEglOwned(out,width,height,surfaceless,NULL);
#else
 XL6_CodeLease* lease=NULL;
 xge_callback_owner_t code={0};
 int status=acquireContextCode(owner,&lease,&code);
 if(status!=XGE_OK)return status;
 status=xgeGraphicsContextCreateEglOwned(out,width,height,surfaceless,lease ? &code:NULL);
 releaseContextCode(lease);
 return status;
#endif
}

int xl6Xge3GraphicsContextCreateOffscreenOwned(xge_graphics_context* out,int width,int height,
 const XL6_CodeOwner* owner)
{
 if(!out)return XGE_ERROR_INVALID_ARGUMENT;
 *out=NULL;
#if !defined(_WIN32) && !XGE_ENABLE_EGL
 (void)owner;
 return xgeGraphicsContextCreateOffscreenOwned(out,width,height,NULL);
#else
 XL6_CodeLease* lease=NULL;
 xge_callback_owner_t code={0};
 int status=acquireContextCode(owner,&lease,&code);
 if(status!=XGE_OK)return status;
 status=xgeGraphicsContextCreateOffscreenOwned(out,width,height,lease ? &code:NULL);
 releaseContextCode(lease);
 return status;
#endif
}
