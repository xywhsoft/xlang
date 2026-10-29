#include "xlir_class_storage.generated.h"
#include "xlir_class_storage.generated.c"
typedef struct ClassStorageLanguageHooks {
    void (*capture)(xvalue*);
    void (*destroyed)(int64_t);
} ClassStorageLanguageHooks;
static ClassStorageLanguageHooks hooks;
void class_storage_language_bind(const ClassStorageLanguageHooks* input) { hooks = *input; }
void class_storage_capture(xvalue* value) { hooks.capture(value); }
void class_storage_destroyed(int64_t value) { hooks.destroyed(value); }
