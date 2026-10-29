#ifndef XS_HANDLER_RUNTIME_H
#define XS_HANDLER_RUNTIME_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* xs2_handler_slot_create(
    const char* source_path,
    const char* work_dir,
    const char* include_dir,
    int pool_size,
    int queue_capacity,
    int queue_timeout_ms,
    int execution_timeout_ms);

void xs2_handler_slot_destroy(void* handle);
int xs2_handler_slot_ready(void* handle);
int xs2_handler_slot_reload(void* handle);
void* xs2_handler_slot_prepare_reload(void* handle);
int xs2_handler_slot_commit_reload(void* handle, void* prepared_handle);
void xs2_handler_slot_discard_reload(void* handle, void* prepared_handle);
void* xs2_handler_slot_last_error_copy(void* handle);
long long xs2_handler_slot_generation(void* handle);
int xs2_handler_slot_pool_size(void* handle);

int xs2_handler_slot_call_text(
    void* handle,
    int kind,
    const char* service_name,
    const char* route_name,
    void* request_handle,
    void* protocol_response_handle,
    const char* data,
    size_t data_size,
    const char* remote_address,
    unsigned int remote_port,
    void* response_buffer);

int xs2_handler_slot_call_buffer(
    void* handle,
    int kind,
    const char* service_name,
    const char* route_name,
    void* request_handle,
    void* protocol_response_handle,
    const void* data,
    size_t data_size,
    const char* remote_address,
    unsigned int remote_port,
    void* response_buffer);

#ifdef __cplusplus
}
#endif

#endif
