#include "xs_handler.h"

int xs_handler(xs_handler_frame* frame)
{
    if ( !xsHandlerFrameIsValid(frame) || !xsHandlerApiHasBinaryOutput(frame->api) ) {
        return XS_HANDLER_ERROR;
    }
    if ( frame->kind >= XS_HANDLER_KIND_INIT && frame->kind <= XS_HANDLER_KIND_UNIT ) {
        return XS_HANDLER_OK;
    }
    if ( frame->kind == XS_HANDLER_KIND_TCP_OPEN || frame->kind == XS_HANDLER_KIND_TCP_CLOSE ) {
        return XS_HANDLER_OK;
    }
    if ( frame->kind != XS_HANDLER_KIND_HTTP && frame->kind != XS_HANDLER_KIND_TCP_DATA &&
         frame->kind != XS_HANDLER_KIND_UDP ) {
        return XS_HANDLER_BAD_REQUEST;
    }
    if ( !frame->api->clear_output(frame) ||
         !frame->api->append_output(frame, frame->data, frame->data_size) ) {
        return XS_HANDLER_ERROR;
    }
    return XS_HANDLER_OK;
}
