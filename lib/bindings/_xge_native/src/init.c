/* Cold engine code admission. Window/GPU operations remain direct native calls. */
#include <xge3_runtime.h>
int xl6Xge3InitWithCode(const xge_desc_t* desc,const XL6_CodeOwner* owner)
{
 /* Native copies the title. Raw descriptors carrying a user pointer or
  * native window keep the original, conservative language-state lease. */
 bool imageOnly=!desc || (!desc->pUser && !desc->pNativeWindow);
 XL6_CodeLease* lease=NULL;
 XL6_CodeLeaseAdmission admission=imageOnly ? xl6CodeLeaseAcquireImageV1(owner,&lease) :
  xl6CodeLeaseAcquireScoped(owner,&lease);
 if(admission!=XL6_CODE_LEASE_ACQUIRED)
  return admission==XL6_CODE_LEASE_RESOURCE_FAILURE ? XGE_ERROR_OUT_OF_MEMORY:XGE_ERROR_INVALID_STATE;
 xge_callback_owner_t code={xl6CodeLeaseOwnership(lease),xl6CodeLeaseHoldNativeV1,xl6CodeLeaseDropNativeV1};
 int status=xgeInitWithCode(desc,lease ? &code:NULL);
 xerror* error=xrtTakeError();xl6CodeLeaseRelease(lease);xrtClearError();xrtSetErrorTake(error);
 return status;
}
