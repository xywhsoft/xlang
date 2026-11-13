#ifndef XL6_XGE3D_DECODE_BUDGET_H
#define XL6_XGE3D_DECODE_BUDGET_H
#include <xge3_render_resources.h>

/* The actual native quota has no language edges. It is independently retained
 * by active XRT allocation scopes/blocks and by each ShareOwned result.
 * NativeFree consumes exactly the one owner transferred to this view.
 * Without a native CallBegin/CallEnd pair the resident root already defers
 * active close until its generic C scopes return. No scope-only flag is set. */
XL6_XGE_NATIVE_POLICY(xl6Xge3dDecodeBudgetPolicy,"xge3d.decodeBudget",xge3d_decode_budget,xge3dDecodeBudgetFree)
#define xl6Xge3dDecodeBudgetPolicy (*xl6Xge3dDecodeBudgetPolicyGet())
#endif
