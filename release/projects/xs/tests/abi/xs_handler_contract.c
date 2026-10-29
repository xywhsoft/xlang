#include <stddef.h>
#include <stdint.h>

#include "../../include/xs_handler.h"

_Static_assert(XS_HANDLER_ABI_VERSION == 1u, "unexpected handler ABI version");
_Static_assert(offsetof(xs_handler_frame, struct_size) == 0u, "struct_size must be first");
_Static_assert(offsetof(xs_handler_frame, abi_version) == sizeof(uint32_t), "abi_version layout mismatch");
_Static_assert(offsetof(xs_handler_frame, host) > offsetof(xs_handler_frame, api), "fields must only append");
_Static_assert(offsetof(xs_handler_frame, data) > offsetof(xs_handler_frame, host), "extended fields must append");
_Static_assert(offsetof(xs_handler_host_api, append_output) > offsetof(xs_handler_host_api, log), "API fields must append");

int xs_handler(xs_handler_frame* frame)
{
    if ( !xsHandlerFrameIsValid(frame) || !xsHandlerApiIsValid(frame->api) ) {
        return XS_HANDLER_ERROR;
    }
    return XS_HANDLER_OK;
}

int main(void)
{
    xs_handler_host_api api = {0};
    xs_handler_frame frame = {0};

    api.struct_size = (uint32_t)sizeof(api);
    api.abi_version = XS_HANDLER_ABI_VERSION;

    frame.struct_size = (uint32_t)sizeof(frame);
    frame.abi_version = XS_HANDLER_ABI_VERSION;
    frame.kind = XS_HANDLER_KIND_INIT;
    frame.api = &api;
    frame.data = "abc";
    frame.data_size = 3u;

    if ( xs_handler(&frame) != XS_HANDLER_OK ) {
        return 1;
    }
    if ( XS_HANDLER_KIND_HTTP != 1 || XS_HANDLER_UNAVAILABLE != 8 ) {
        return 2;
    }
    if ( !xsHandlerFrameHasData(&frame) || xsHandlerApiHasBinaryOutput(&api) ) {
        return 3;
    }
    return 0;
}
