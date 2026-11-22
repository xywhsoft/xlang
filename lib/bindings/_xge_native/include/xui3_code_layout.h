#ifndef XL6_XUI3_CODE_LAYOUT_H
#define XL6_XUI3_CODE_LAYOUT_H

#include <xge3_runtime.h>

/* Packed declarations with different input projections use separate compiler
 * names. The C compiler still calls the same three native functions. */
#define xl6Xui3CodeLayoutCaptureEditor xuiCodeLayoutCapture
#define xl6Xui3CodeLayoutCaptureFolded xuiCodeLayoutCapture
#define xl6Xui3CodeLayoutCaptureFoldedEditor xuiCodeLayoutCapture
#define xl6Xui3CodeLayoutHitTestEditor xuiCodeLayoutHitTest
#define xl6Xui3CodeLayoutHitTestFolded xuiCodeLayoutHitTest
#define xl6Xui3CodeLayoutHitTestFoldedEditor xuiCodeLayoutHitTest
#define xl6Xui3CodeLayoutCaretRectEditor xuiCodeLayoutGetCaretRect
#define xl6Xui3CodeLayoutCaretRectFolded xuiCodeLayoutGetCaretRect
#define xl6Xui3CodeLayoutCaretRectFoldedEditor xuiCodeLayoutGetCaretRect

static inline bool xl6Xui3CodeLayoutSnapshotDestroy(void* raw)
{
    xuiCodeLayoutSnapshotRelease((xui_code_layout_snapshot)raw);
    return true;
}

static inline const XL6_ForeignViewDescriptorV1* xl6Xui3CodeLayoutSnapshotPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value = {
        sizeof(value), XL6_FOREIGN_VIEW_V1_TAG, "xui.codeLayoutSnapshot",
        0, 1, NULL, NULL, xl6Xui3CodeLayoutSnapshotDestroy, 0, 1,
        NULL, NULL, NULL, NULL, 0, NULL, NULL
    };
    return &value;
}

#define xl6Xui3CodeLayoutSnapshotPolicy (*xl6Xui3CodeLayoutSnapshotPolicyGet())
#endif
