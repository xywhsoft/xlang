#include "xlir_class_copy_storage.generated.h"
#include "xlir_class_copy_storage.generated.c"
static void (*capture_hook)(struct xvalue*);
void class_copy_storage_bind(void (*capture)(struct xvalue*)) { capture_hook = capture; }
void class_copy_storage_capture(struct xvalue* value) { capture_hook(value); }
