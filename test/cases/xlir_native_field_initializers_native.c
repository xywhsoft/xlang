#include "xrt.h"
#include <stdlib.h>

typedef struct native_fields_owner {
    int Id;
} native_fields_owner;

static int NativeFieldsLive;
static int NativeFieldsDropped;
static int NativeFieldsNextId;

void* native_fields_create(void)
{
    native_fields_owner* owner = xrtMalloc(sizeof(*owner));
    if (owner == NULL) { return NULL; }
    owner->Id = ++NativeFieldsNextId;
    ++NativeFieldsLive;
    return owner;
}

void native_fields_destroy(void* handle)
{
    native_fields_owner* owner = handle;
    if (owner == NULL) { return; }
    if (NativeFieldsLive <= 0 || owner->Id <= 0) { abort(); }
    owner->Id = 0;
    --NativeFieldsLive;
    ++NativeFieldsDropped;
    xrtFree(owner);
}

int native_fields_id(void* handle)
{
    return handle != NULL ? ((native_fields_owner*)handle)->Id : 0;
}

int native_fields_live(void) { return NativeFieldsLive; }
int native_fields_dropped(void) { return NativeFieldsDropped; }
