#include "src/common/xl6_xrt.h"
#include "xl6_native_capture_graph.h"
#include <stddef.h>

const XL6_NativeGraphCaptureProfileV1* xl6NativeGraphCaptureProfileV1Get(const XL6_NativeCaptureProfileV1* prefix)
{
    if (!prefix || prefix->size!=sizeof(XL6_NativeGraphCaptureProfileV1) ||
        !prefix->identity || prefix->Type || prefix->DetachCleanup) return NULL;
    const XL6_NativeGraphCaptureProfileV1* p=(const XL6_NativeGraphCaptureProfileV1*)prefix;
    if (p->abiTag!=XL6_NATIVE_GRAPH_CAPTURE_V1_TAG || !p->bytes || !p->alignment ||
        (p->alignment&(p->alignment-1)) || p->alignment>_Alignof(max_align_t) ||
        _Alignof(max_align_t)%p->alignment || p->bytes%p->alignment ||
        !p->CopyFresh || !p->Trace || !p->Detach) return NULL;
    return p;
}
