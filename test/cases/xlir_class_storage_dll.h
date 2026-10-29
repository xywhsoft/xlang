#ifndef XLIR_CLASS_STORAGE_DLL_H
#define XLIR_CLASS_STORAGE_DLL_H
struct xvalue;
typedef struct ClassStorageHooks {
    void* data;
    void (*finalize)(struct xvalue*, void*);
    unsigned returned;
} ClassStorageHooks;
#endif
