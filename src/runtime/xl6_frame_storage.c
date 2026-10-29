#define XRT_MODULE_MEMORY
#include "src/common/xl6_xrt.h"
#include "xl6_frame_storage.h"
#include "xl6_ownership_scope.h"

static void* frame_allocate(size_t size)
{
    XL6_OWNERSHIP_RETURN(void*, xrtMalloc(size));
}
static void frame_free(void* data)
{
    XL6_OWNERSHIP_RETURN_VOID(xrtFree(data));
}
const XL6_FrameStorageRuntimeV1* xl6FrameStorageRuntimeV1Get(void)
{
    static const XL6_FrameStorageRuntimeV1 api = {frame_allocate, frame_free};
    return &api;
}
