#include "handler_generation.h"
#include "handler_runtime.h"

#include "../include/xs_handler.h"

#include "xrt.h"

/* ------------------------------------ Slot 数据结构 ------------------------------------ */

typedef struct xs2_generation_ref {
    xs2_handler_generation* generation;
    unsigned int active_calls;
    struct xs2_generation_ref* next;
} xs2_generation_ref;

typedef struct xs2_handler_slot {
    xmutex mutex;
    xcond changed;
    xs2_generation_ref* current;
    xs2_generation_ref* retired;

    char* source_path;
    char* work_dir;
    char* include_dir;
    char* last_error;

    int pool_size;
    int queue_capacity;
    int queue_timeout_ms;
    int execution_timeout_ms;
    int closing;
    unsigned int active_prepares;
    unsigned int prepared_count;
    uint64 generation_id;
    uint64 next_request_id;
} xs2_handler_slot;

typedef struct xs2_handler_prepared {
    xs2_handler_slot* owner;
    xs2_generation_ref* replacement;
} xs2_handler_prepared;

/* ------------------------------------ 字符串与错误 ------------------------------------ */

static char* xs2SlotCopyText(const char* text)
{
    if ( text == NULL ) {
        text = "";
    }
    return (char*)xrtCopyStr((str)(void*)text, 0u);
}

static void xs2SlotSetErrorUnlocked(xs2_handler_slot* slot, const char* text)
{
    if ( slot->last_error != NULL ) {
        xrtFree(slot->last_error);
    }
    slot->last_error = xs2SlotCopyText(text);
}

static xs2_generation_ref* xs2GenerationRefCreate(xs2_handler_generation* generation)
{
    xs2_generation_ref* ref;

    ref = (xs2_generation_ref*)xrtCalloc(1u, sizeof(*ref));
    if ( ref != NULL ) {
        ref->generation = generation;
    }
    return ref;
}

static void xs2GenerationRefDestroy(xs2_generation_ref* ref)
{
    if ( ref == NULL ) {
        return;
    }
    xs2GenerationDestroy(ref->generation);
    xrtFree(ref);
}

/* ------------------------------------ Retired generation 回收 ------------------------------------ */

static xs2_generation_ref* xs2SlotDetachReapableUnlocked(xs2_handler_slot* slot)
{
    xs2_generation_ref** link;
    xs2_generation_ref* ref;

    link = &slot->retired;
    while ( *link != NULL ) {
        ref = *link;
        if ( ref->active_calls == 0u ) {
            *link = ref->next;
            ref->next = NULL;
            return ref;
        }
        link = &ref->next;
    }
    return NULL;
}

static void xs2SlotReap(xs2_handler_slot* slot)
{
    xs2_generation_ref* ref;

    for ( ;; ) {
        xrtMutexLock(slot->mutex);
        ref = xs2SlotDetachReapableUnlocked(slot);
        xrtMutexUnlock(slot->mutex);
        if ( ref == NULL ) {
            break;
        }
        xs2GenerationRefDestroy(ref);
    }
}

/* ------------------------------------ 创建与销毁 ------------------------------------ */

void* xs2_handler_slot_create(
    const char* source_path,
    const char* work_dir,
    const char* include_dir,
    int pool_size,
    int queue_capacity,
    int queue_timeout_ms,
    int execution_timeout_ms)
{
    xs2_handler_slot* slot;
    xs2_handler_generation* generation;
    xs2_generation_ref* ref;
    char* error_text;

    slot = (xs2_handler_slot*)xrtCalloc(1u, sizeof(*slot));
    if ( slot == NULL ) {
        return NULL;
    }
    slot->mutex = xrtMutexCreate();
    slot->changed = xrtCondCreate();
    slot->source_path = xs2SlotCopyText(source_path);
    slot->work_dir = xs2SlotCopyText(work_dir);
    slot->include_dir = xs2SlotCopyText(include_dir);
    slot->pool_size = pool_size;
    slot->queue_capacity = queue_capacity;
    slot->queue_timeout_ms = queue_timeout_ms;
    slot->execution_timeout_ms = execution_timeout_ms;
    if ( slot->mutex == NULL || slot->changed == NULL || slot->source_path == NULL ||
         slot->work_dir == NULL || slot->include_dir == NULL ) {
        xs2_handler_slot_destroy(slot);
        return NULL;
    }

    error_text = NULL;
    generation = xs2GenerationCreate(
        slot->source_path,
        slot->work_dir,
        slot->include_dir,
        pool_size,
        queue_capacity,
        queue_timeout_ms,
        execution_timeout_ms,
        &error_text);
    if ( generation == NULL ) {
        xs2SlotSetErrorUnlocked(slot, error_text != NULL ? error_text : "handler generation creation failed");
        if ( error_text != NULL ) {
            xrtFree(error_text);
        }
        return slot;
    }
    ref = xs2GenerationRefCreate(generation);
    if ( ref == NULL ) {
        xs2GenerationDestroy(generation);
        xs2SlotSetErrorUnlocked(slot, "cannot allocate handler generation reference");
        return slot;
    }
    slot->current = ref;
    slot->generation_id = 1u;
    slot->next_request_id = 1u;
    return slot;
}

void xs2_handler_slot_destroy(void* handle)
{
    xs2_handler_slot* slot;
    xs2_generation_ref* list;
    xs2_generation_ref* next;
    int has_active;

    slot = (xs2_handler_slot*)handle;
    if ( slot == NULL ) {
        return;
    }
    if ( slot->mutex != NULL ) {
        xrtMutexLock(slot->mutex);
        slot->closing = 1;
        if ( slot->current != NULL ) {
            slot->current->next = slot->retired;
            slot->retired = slot->current;
            slot->current = NULL;
        }
        do {
            xs2_generation_ref* scan;
            has_active = slot->active_prepares != 0u || slot->prepared_count != 0u;
            for ( scan = slot->retired; scan != NULL; scan = scan->next ) {
                if ( scan->active_calls != 0u ) {
                    has_active = 1;
                    break;
                }
            }
            if ( has_active ) {
                xrtCondWait(slot->changed, slot->mutex);
            }
        } while ( has_active );
        list = slot->retired;
        slot->retired = NULL;
        xrtMutexUnlock(slot->mutex);
    } else {
        list = slot->current;
        slot->current = NULL;
    }

    while ( list != NULL ) {
        next = list->next;
        xs2GenerationRefDestroy(list);
        list = next;
    }
    if ( slot->changed != NULL ) {
        xrtCondDestroy(slot->changed);
    }
    if ( slot->mutex != NULL ) {
        xrtMutexDestroy(slot->mutex);
    }
    xrtFree(slot->source_path);
    xrtFree(slot->work_dir);
    xrtFree(slot->include_dir);
    xrtFree(slot->last_error);
    xrtFree(slot);
}

/* ------------------------------------ 状态与 reload ------------------------------------ */

int xs2_handler_slot_ready(void* handle)
{
    xs2_handler_slot* slot;
    int ready;

    slot = (xs2_handler_slot*)handle;
    if ( slot == NULL ) {
        return 0;
    }
    xrtMutexLock(slot->mutex);
    ready = !slot->closing && slot->current != NULL;
    xrtMutexUnlock(slot->mutex);
    return ready;
}

int xs2_handler_slot_reload(void* handle)
{
    void* prepared = xs2_handler_slot_prepare_reload(handle);
    if ( prepared == NULL ) {
        return 0;
    }
    if ( xs2_handler_slot_commit_reload(handle, prepared) ) {
        return 1;
    }
    xs2_handler_slot_discard_reload(handle, prepared);
    return 0;
}

void* xs2_handler_slot_prepare_reload(void* handle)
{
    xs2_handler_slot* slot = (xs2_handler_slot*)handle;
    xs2_handler_generation* generation;
    xs2_handler_prepared* prepared;
    xs2_generation_ref* replacement;
    char* error_text;

    if ( slot == NULL ) {
        return NULL;
    }
    xrtMutexLock(slot->mutex);
    if ( slot->closing ) {
        xs2SlotSetErrorUnlocked(slot, "handler slot is closing");
        xrtMutexUnlock(slot->mutex);
        return NULL;
    }
    slot->active_prepares += 1u;
    xrtMutexUnlock(slot->mutex);

    error_text = NULL;
    generation = xs2GenerationCreate(
        slot->source_path,
        slot->work_dir,
        slot->include_dir,
        slot->pool_size,
        slot->queue_capacity,
        slot->queue_timeout_ms,
        slot->execution_timeout_ms,
        &error_text);

    if ( generation == NULL ) {
        xrtMutexLock(slot->mutex);
        slot->active_prepares -= 1u;
        xs2SlotSetErrorUnlocked(slot, error_text != NULL ? error_text : "handler reload preparation failed");
        xrtCondBroadcast(slot->changed);
        xrtMutexUnlock(slot->mutex);
        xrtFree(error_text);
        return NULL;
    }
    xrtFree(error_text);

    replacement = xs2GenerationRefCreate(generation);
    prepared = (xs2_handler_prepared*)xrtCalloc(1u, sizeof(*prepared));
    if ( replacement == NULL || prepared == NULL ) {
        xs2GenerationRefDestroy(replacement);
        if ( replacement == NULL ) {
            xs2GenerationDestroy(generation);
        }
        xrtFree(prepared);
        xrtMutexLock(slot->mutex);
        slot->active_prepares -= 1u;
        xs2SlotSetErrorUnlocked(slot, "cannot allocate prepared handler generation");
        xrtCondBroadcast(slot->changed);
        xrtMutexUnlock(slot->mutex);
        return NULL;
    }
    prepared->owner = slot;
    prepared->replacement = replacement;
    xrtMutexLock(slot->mutex);
    slot->active_prepares -= 1u;
    if ( slot->closing ) {
        xrtCondBroadcast(slot->changed);
        xrtMutexUnlock(slot->mutex);
        xs2GenerationRefDestroy(replacement);
        xrtFree(prepared);
        return NULL;
    }
    slot->prepared_count += 1u;
    xrtMutexUnlock(slot->mutex);
    return prepared;
}

int xs2_handler_slot_commit_reload(void* handle, void* prepared_handle)
{
    xs2_handler_slot* slot = (xs2_handler_slot*)handle;
    xs2_handler_prepared* prepared = (xs2_handler_prepared*)prepared_handle;
    xs2_generation_ref* previous;

    if ( slot == NULL || prepared == NULL || prepared->owner != slot ||
         prepared->replacement == NULL ) {
        return 0;
    }
    xrtMutexLock(slot->mutex);
    if ( slot->closing ) {
        xrtMutexUnlock(slot->mutex);
        return 0;
    }
    previous = slot->current;
    slot->current = prepared->replacement;
    prepared->replacement = NULL;
    slot->generation_id += 1u;
    slot->prepared_count -= 1u;
    xs2SlotSetErrorUnlocked(slot, "");
    if ( previous != NULL ) {
        previous->next = slot->retired;
        slot->retired = previous;
    }
    xrtCondBroadcast(slot->changed);
    xrtMutexUnlock(slot->mutex);
    xrtFree(prepared);
    xs2SlotReap(slot);
    return 1;
}

void xs2_handler_slot_discard_reload(void* handle, void* prepared_handle)
{
    xs2_handler_prepared* prepared = (xs2_handler_prepared*)prepared_handle;
    xs2_handler_slot* slot = (xs2_handler_slot*)handle;

    if ( slot == NULL || prepared == NULL || prepared->owner != slot ) {
        return;
    }
    xrtMutexLock(slot->mutex);
    if ( slot->prepared_count != 0u ) {
        slot->prepared_count -= 1u;
    }
    xrtCondBroadcast(slot->changed);
    xrtMutexUnlock(slot->mutex);
    xs2GenerationRefDestroy(prepared->replacement);
    xrtFree(prepared);
}

void* xs2_handler_slot_last_error_copy(void* handle)
{
    xs2_handler_slot* slot;
    char* result;

    slot = (xs2_handler_slot*)handle;
    if ( slot == NULL ) {
        return xs2SlotCopyText("handler slot is null");
    }
    xrtMutexLock(slot->mutex);
    result = xs2SlotCopyText(slot->last_error);
    xrtMutexUnlock(slot->mutex);
    return result;
}

long long xs2_handler_slot_generation(void* handle)
{
    xs2_handler_slot* slot;
    uint64 value;

    slot = (xs2_handler_slot*)handle;
    if ( slot == NULL ) {
        return 0;
    }
    xrtMutexLock(slot->mutex);
    value = slot->generation_id;
    xrtMutexUnlock(slot->mutex);
    return (long long)value;
}

int xs2_handler_slot_pool_size(void* handle)
{
    xs2_handler_slot* slot = (xs2_handler_slot*)handle;
    return slot != NULL ? slot->pool_size : 0;
}

/* ------------------------------------ 请求调用 ------------------------------------ */

static int xs2HandlerSlotCall(
    xs2_handler_slot* slot,
    int kind,
    const char* service_name,
    const char* route_name,
    void* request_handle,
    void* protocol_response_handle,
    const void* data,
    size_t data_size,
    const char* remote_address,
    unsigned int remote_port,
    void* response_buffer)
{
    xs2_generation_ref* ref;
    uint64 request_id;
    char* error_text;
    int result;

    if ( slot == NULL ) {
        return XS_HANDLER_UNAVAILABLE;
    }
    xrtMutexLock(slot->mutex);
    ref = slot->current;
    if ( slot->closing || ref == NULL ) {
        xs2SlotSetErrorUnlocked(slot, "handler slot is unavailable");
        xrtMutexUnlock(slot->mutex);
        return XS_HANDLER_UNAVAILABLE;
    }
    ref->active_calls += 1u;
    request_id = slot->next_request_id++;
    xrtMutexUnlock(slot->mutex);

    error_text = NULL;
    result = xs2GenerationCall(
        ref->generation,
        request_id,
        kind,
        service_name,
        route_name,
        request_handle,
        protocol_response_handle,
        data,
        data_size,
        remote_address,
        remote_port,
        response_buffer,
        &error_text);

    xrtMutexLock(slot->mutex);
    ref->active_calls -= 1u;
    if ( error_text != NULL && error_text[0] != '\0' ) {
        xs2SlotSetErrorUnlocked(slot, error_text);
    } else if ( result == 0 ) {
        xs2SlotSetErrorUnlocked(slot, "");
    }
    xrtCondBroadcast(slot->changed);
    xrtMutexUnlock(slot->mutex);
    xrtFree(error_text);
    xs2SlotReap(slot);
    return result;
}

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
    void* response_buffer)
{
    return xs2HandlerSlotCall(
        (xs2_handler_slot*)handle,
        kind,
        service_name,
        route_name,
        request_handle,
        protocol_response_handle,
        data,
        data_size,
        remote_address,
        remote_port,
        response_buffer);
}

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
    void* response_buffer)
{
    return xs2HandlerSlotCall(
        (xs2_handler_slot*)handle,
        kind,
        service_name,
        route_name,
        request_handle,
        protocol_response_handle,
        data,
        data_size,
        remote_address,
        remote_port,
        response_buffer);
}
