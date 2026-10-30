#define XRT_MODULE_HASH_KEYED
#define XRT_MODULE_MEMORY
#include "src/common/xl6_xrt.h"
#include "xl6_siphash_state.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
/* The state contains scalar lanes and a copied key, with no retained input,
 * callback, language value or independently reference-counted native child. */
void xl6_xrt_siphasher64_destroy(void* state)
{
    if (state) {
        xrtSecureZero(state, sizeof(xlir_siphasher64));
        xrtFree(state);
    }
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6SipHasher64HandlePolicyV1Get,xl6_xrt_siphasher64_destroy)
