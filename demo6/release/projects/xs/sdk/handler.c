#include "../include/xs_handler.h"
#include "xrt.h"

void* xs2_handler_frame_wrap(void* frame)
{
    return frame;
}

void xs2_handler_frame_release(void* frame)
{
    (void)frame;
}

int xs2_handler_frame_kind(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) ? (int)value->kind : XS_HANDLER_KIND_NONE;
}

long long xs2_handler_frame_request_id(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) ? (long long)value->request_id : 0;
}

const char* xs2_handler_frame_service(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) && value->service_name != NULL ? value->service_name : "";
}

const char* xs2_handler_frame_route(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) && value->route_name != NULL ? value->route_name : "";
}

const char* xs2_handler_frame_remote_address(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) && value->remote_address != NULL ? value->remote_address : "";
}

int xs2_handler_frame_remote_port(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) ? (int)value->remote_port : 0;
}

void* xs2_handler_frame_request(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) ? value->request : NULL;
}

void* xs2_handler_frame_response(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) ? value->response : NULL;
}

void* xs2_handler_frame_data(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameHasData(value) ? (void*)value->data : NULL;
}

size_t xs2_handler_frame_data_size(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameHasData(value) ? value->data_size : 0u;
}

void* xs2_handler_frame_text_copy(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;

    if ( !xsHandlerFrameHasData(value) || value->data == NULL || value->data_size == 0u ) {
        return xrtCopyStr((str)(void*)"", 0u);
    }
    return xrtCopyStr((str)(void*)value->data, value->data_size);
}

int xs2_handler_frame_cancelled(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) && value->api != NULL && value->api->is_cancelled != NULL
        ? value->api->is_cancelled(value)
        : 1;
}

int xs2_handler_frame_clear(void* frame)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerApiHasBinaryOutput(value != NULL ? value->api : NULL) && value->api->clear_output != NULL
        ? value->api->clear_output(value)
        : 0;
}

int xs2_handler_frame_write_text(void* frame, const char* text)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerFrameIsValid(value) && value->api != NULL && value->api->set_output_text != NULL
        ? value->api->set_output_text(value, text != NULL ? text : "")
        : 0;
}

int xs2_handler_frame_append_text(void* frame, const char* text)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    size_t size = text != NULL ? xrtStrByteLen(text, 0u) : 0u;
    return xsHandlerApiHasBinaryOutput(value != NULL ? value->api : NULL)
        ? value->api->append_output(value, text, size)
        : 0;
}

int xs2_handler_frame_append(void* frame, const void* data, size_t size)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    return xsHandlerApiHasBinaryOutput(value != NULL ? value->api : NULL)
        ? value->api->append_output(value, data, size)
        : 0;
}

void xs2_handler_frame_log(void* frame, int level, const char* message)
{
    xs_handler_frame* value = (xs_handler_frame*)frame;
    if ( xsHandlerFrameIsValid(value) && value->api != NULL && value->api->log != NULL ) {
        value->api->log(value, level, message != NULL ? message : "");
    }
}
