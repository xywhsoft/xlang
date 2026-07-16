#include "xs_handler.h"
#include "xrt.h"

#include <string.h>

int xs_handler(xs_handler_frame* frame)
{
    if ( !xsHandlerFrameIsValid(frame) || !xsHandlerApiIsValid(frame->api) ) {
        return XS_HANDLER_BAD_REQUEST;
    }
    if ( frame->kind >= XS_HANDLER_KIND_INIT && frame->kind <= XS_HANDLER_KIND_UNIT ) {
        return XS_HANDLER_OK;
    }
    if ( frame->kind != XS_HANDLER_KIND_HTTP || !xsHandlerFrameHasData(frame) ) {
        return XS_HANDLER_BAD_REQUEST;
    }
    if ( frame->data_size == 4u && memcmp(frame->data, "slow", 4u) == 0 ) {
        xrtSleep(120u);
        if ( frame->api->is_cancelled(frame) ) {
            return XS_HANDLER_TIMEOUT;
        }
    }
    if ( frame->data_size == 7u && memcmp(frame->data, "invalid", 7u) == 0 ) {
        return 99;
    }
    if ( frame->api->set_output_text(frame, "ready") == 0 ) {
        return XS_HANDLER_ERROR;
    }
    return XS_HANDLER_OK;
}
