#ifndef XS_HANDLER_GENERATION_H
#define XS_HANDLER_GENERATION_H

#include <stddef.h>
#include <stdint.h>

typedef struct xs2_handler_generation xs2_handler_generation;

xs2_handler_generation* xs2GenerationCreate(
    const char* source_path,
    const char* work_dir,
    const char* include_dir,
    int pool_size,
    int queue_capacity,
    int queue_timeout_ms,
    int execution_timeout_ms,
    char** error_text);

void xs2GenerationDestroy(xs2_handler_generation* generation);

int xs2GenerationCall(
    xs2_handler_generation* generation,
    uint64_t request_id,
    int kind,
    const char* service_name,
    const char* route_name,
    void* request_handle,
    void* response_handle,
    const void* data,
    size_t data_size,
    const char* remote_address,
    unsigned int remote_port,
    void* response_buffer,
    char** error_text);

int xs2GenerationPoolSize(const xs2_handler_generation* generation);

#endif
