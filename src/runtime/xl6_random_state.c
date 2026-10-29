#include "xl6_text_value.h"
#include "xl6_random_state.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
XL6_TEXT_API void* xl6_xrt_rng_create(uint64_t seed,uint64_t stream)
{
    if(stream>>63u){xrtSetErrorKind(XERR_RANGE);return NULL;}
    xlir_random_state* state=xrtMalloc(sizeof(*state));if(!state)return NULL;
    state->owner=xrtThreadCurrentId();xrtRngSeed(&state->rng,seed,stream);return state;
}
XL6_TEXT_API void* xl6_xrt_rng_create_default(uint64_t seed)
{ return xl6_xrt_rng_create(seed,1u); }
XL6_TEXT_API void xl6_xrt_rng_destroy(void* handle)
{ if(handle){xrtSecureZero(handle,sizeof(xlir_random_state));xrtFree(handle);} }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6RandomStateHandlePolicyV1Get,xl6_xrt_rng_destroy)
