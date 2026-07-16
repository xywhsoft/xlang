#include "xrt.h"

#include <string.h>

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
long long xs2_handler_slot_generation(void* handle);
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

typedef struct worker_state {
    void* slot;
    int iterations;
    const char* expected;
    size_t expected_size;
} worker_state;

static uint32 handler_worker(ptr argument)
{
    worker_state* state = (worker_state*)argument;
    xbuffer response = xrtBufferCreate(64u);
    int i;

    if ( state == NULL || response == NULL ) {
        return 10u;
    }
    for ( i = 0; i < state->iterations; ++i ) {
        response->Length = 0u;
        if ( xs2_handler_slot_call_text(
                 state->slot, 1, "test", "/parallel", NULL, NULL, "work", 4u,
                 "127.0.0.1", 9000u, response) != 0 ) {
            xrtBufferDestroy(response);
            return 11u;
        }
        if ( response->Length != state->expected_size ||
             memcmp(response->Buffer, state->expected, state->expected_size) != 0 ) {
            xrtBufferDestroy(response);
            return 12u;
        }
    }
    xrtBufferDestroy(response);
    return 0u;
}

static int run_case(const char* source_path, const char* expected, size_t expected_size)
{
    worker_state state;
    xthread workers[4];
    void* slot;
    int i;
    int result;

    memset(&state, 0, sizeof(state));
    memset(workers, 0, sizeof(workers));
    slot = xs2_handler_slot_create(
        source_path,
        ".",
        "projects/xs/include",
        2,
        32,
        5000,
        5000);
    if ( slot == NULL || !xs2_handler_slot_ready(slot) ) {
        xs2_handler_slot_destroy(slot);
        return 1;
    }

    state.slot = slot;
    state.iterations = 500;
    state.expected = expected;
    state.expected_size = expected_size;
    for ( i = 0; i < 4; ++i ) {
        workers[i] = xrtThreadCreate((ptr)handler_worker, &state, 0u);
        if ( workers[i] == NULL ) {
            xs2_handler_slot_destroy(slot);
            return 2;
        }
    }
    if ( !xs2_handler_slot_reload(slot) || xs2_handler_slot_generation(slot) != 2 ) {
        xs2_handler_slot_destroy(slot);
        return 3;
    }

    result = 0;
    for ( i = 0; i < 4; ++i ) {
        xrtThreadWait(workers[i]);
        if ( xrtThreadGetExitCode(workers[i]) != 0u ) {
            result = 4;
        }
        xrtThreadDestroy(workers[i]);
    }
    xs2_handler_slot_destroy(slot);
    return result;
}

int main(void)
{
    int result;

    result = run_case("projects/xs/tests/handlers/abi_c.c", "c:work", 6u);
    if ( result != 0 ) {
        return result;
    }
    result = run_case("projects/xs/tests/handlers/abi_xlang.xl", "xl:work", 7u);
    return result != 0 ? result + 20 : 0;
}
