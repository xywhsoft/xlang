#ifndef XL6_FRAME_STORAGE_H
#define XL6_FRAME_STORAGE_H
#include <stddef.h>
#ifndef XL6_FRAME_STORAGE_API
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_FRAME_STORAGE_API __declspec(dllexport)
#else
#define XL6_FRAME_STORAGE_API
#endif
#endif

/* Physical activation storage only: no second graph identity, synthetic
 * reference, callback, or inferred payload layout. Allocation and final free
 * enter the ownership mutation domain. The returned storage is still private;
 * verified producers initialize it before publishing its existing owner node
 * (an embedded continuation ticket for async machines). Slot transfers and
 * producer/executor handoff have their own explicit contracts.
 *
 * Free accepts NULL or the original allocation after every transport and
 * owning slot has detached. Payload cleanup must finish BEFORE Free and must
 * not run inside this storage admission. Borrowed inspection views require a
 * live owner; Freeze is not a lifetime extension for a dangling native pointer.
 * This API does not certify arbitrary task data, tracing adapters or children.
 * Return actual resident addresses, not unloadable PE import thunks. */
typedef struct XL6_FrameStorageRuntimeV1 {
    void* (*Allocate)(size_t);
    void (*Free)(void*);
} XL6_FrameStorageRuntimeV1;
XL6_FRAME_STORAGE_API const XL6_FrameStorageRuntimeV1* xl6FrameStorageRuntimeV1Get(void);
#endif
