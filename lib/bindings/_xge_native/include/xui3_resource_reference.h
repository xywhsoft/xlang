#ifndef XL6_XUI3_RESOURCE_REFERENCE_H
#define XL6_XUI3_RESOURCE_REFERENCE_H
#include <xge3_runtime.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>

static inline bool xl6Xui3ResourceReferenceDestroy(void* raw)
{
    bool consumed = false;
    int status = xuiResourceReferenceReleaseOwnerChecked((xui_resource_reference_t*)raw, &consumed);
    if (status != XUI_OK) {
        if (!xrtGetError()) xl6ForeignStatusFailureV1(status, "xuiResourceReferenceReleaseOwnerChecked");
        xl6_error_result failure = {false, xrtTakeError()};
        if (!xl6ErrorScopeTryPropagateFailureV1(&failure)) xl6CallbackFailureRecordV1(failure);
    }
    return consumed;
}

static inline const XL6_ForeignViewDescriptorV1* xl6Xui3ResourceReferencePolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 policy = {sizeof(policy), XL6_FOREIGN_VIEW_V1_TAG,
        "xui.resourceReference", 0, 1, NULL, NULL, xl6Xui3ResourceReferenceDestroy, 0, 1,
        NULL, NULL, NULL, NULL, XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE | XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,
        NULL, NULL};
    return &policy;
}
#define xl6Xui3ResourceReferencePolicy (*xl6Xui3ResourceReferencePolicyGet())
#endif
