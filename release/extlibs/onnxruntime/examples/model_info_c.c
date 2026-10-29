#include <stdio.h>
#include "../include/onnxruntime.h"
#ifdef __TINYC__
/* 单文件 TCC 范例复用包内公开包装层，桥接 DLL 仍按原路径动态装载。 */
#include "../src/onnxruntime_artifact.c"
#endif

int main(void)
{
    void* session = ort_session_create_native(
        "extlibs/rapidocr/models/ch_ppocr_mobile_v2.0_cls_infer.onnx", NULL);
    if (session == NULL) {
        fputs("cannot open classifier model\n", stderr);
        return 1;
    }
    puts(ort_session_inputs_native(session));
    puts(ort_session_outputs_native(session));
    ort_session_release_native(session);
    return 0;
}
