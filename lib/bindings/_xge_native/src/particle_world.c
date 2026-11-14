/* Cold code-ownership admission; simulation and callback operations are direct. */
#include <xge3_runtime.h>
int xl6Xge3ParticleWorldCreateOwned(xge_particle_world* output,const xge_particle_world_desc_t* desc,
 const XL6_CodeOwner* owner)
{
 if(!output)return XGE_ERROR_INVALID_ARGUMENT;
 *output=NULL;XL6_CodeLease* lease=NULL;
 XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquireScoped(owner,&lease);
 if(admission!=XL6_CODE_LEASE_ACQUIRED)
  return admission==XL6_CODE_LEASE_RESOURCE_FAILURE ? XGE_ERROR_OUT_OF_MEMORY:XGE_ERROR_INVALID_STATE;
 xge_callback_owner_t code={xl6CodeLeaseOwnership(lease),xl6CodeLeaseHoldNativeV1,xl6CodeLeaseDropNativeV1};
 int status=xgeParticleWorldCreateOwnedWithCode(output,desc,lease ? &code:NULL);
 xerror* error=xrtTakeError();xl6CodeLeaseRelease(lease);xrtClearError();xrtSetErrorTake(error);return status;
}
