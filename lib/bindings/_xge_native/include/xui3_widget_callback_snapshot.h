#ifndef XL6_XUI3_WIDGET_CALLBACK_SNAPSHOT_H
#define XL6_XUI3_WIDGET_CALLBACK_SNAPSHOT_H
#include <xge3_runtime.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>

static inline void xl6Xui3WidgetCallbackSnapshotReport(int status, const char* operation)
{
    if (!xrtGetError()) xl6ForeignStatusFailureV1(status, operation);
    xl6_error_result failure = {false, xrtTakeError()};
    if (!xl6ErrorScopeTryPropagateFailureV1(&failure)) xl6CallbackFailureRecordV1(failure);
}

static inline bool xl6Xui3WidgetCallbackSnapshotDestroy(void* raw)
{
    bool consumed = false;
    int status = xuiWidgetCallbackSnapshotReleaseOwnerChecked(raw, &consumed);
    if (status != XUI_OK) xl6Xui3WidgetCallbackSnapshotReport(status, "xuiWidgetCallbackSnapshotReleaseOwnerChecked");
    return consumed;
}

static inline bool xl6Xui3WidgetCallbackSnapshotTrace(const void* raw, XL6_ModuleOwnershipVisit visit, void* user)
{
    return xuiWidgetCallbackSnapshotTraceOwnership(raw, visit, user);
}

static inline bool xl6Xui3WidgetCallbackSnapshotBusy(const void* raw)
{
    return xuiWidgetCallbackSnapshotOwnershipBusy(raw);
}

static inline bool xl6Xui3WidgetCallbackSnapshotRequestClose(void* raw)
{
    if (!xl6Xui3WidgetCallbackSnapshotBusy(raw)) return true;
    xl6Xui3WidgetCallbackSnapshotReport(XUI_ERROR_BUSY, "xui.WidgetCallbackSnapshot.close");
    return false;
}

static inline const XL6_ForeignViewDescriptorV1* xl6Xui3WidgetCallbackSnapshotPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 policy = {sizeof(policy), XL6_FOREIGN_VIEW_V1_TAG,
        "xui.widgetCallbackSnapshot", 0, 1, NULL, NULL, xl6Xui3WidgetCallbackSnapshotDestroy, 0, 1,
        NULL, NULL, xl6Xui3WidgetCallbackSnapshotTrace, xl6Xui3WidgetCallbackSnapshotBusy,
        XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE | XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,
        NULL, xl6Xui3WidgetCallbackSnapshotRequestClose};
    return &policy;
}
#define xl6Xui3WidgetCallbackSnapshotPolicy (*xl6Xui3WidgetCallbackSnapshotPolicyGet())
#endif
