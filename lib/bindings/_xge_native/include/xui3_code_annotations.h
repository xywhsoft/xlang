#ifndef XL6_XUI3_CODE_ANNOTATIONS_H
#define XL6_XUI3_CODE_ANNOTATIONS_H
#include <xui3_code_cleanup.h>
/* Independent store owns only copied native strings and numeric records.
 * No Document, UI resource or language callback is retained. */
static inline bool xl6Xui3CodeAnnotationStoreDestroy(void* raw)
{
    int status = xuiCodeAnnotationStoreDestroyChecked((xui_code_annotation_store)raw);
    return xl6Xui3CodeCleanupResult(status, "xuiCodeAnnotationStoreDestroyChecked");
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xui3CodeAnnotationStorePolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.codeAnnotationStore",
  0,1,NULL,NULL,xl6Xui3CodeAnnotationStoreDestroy,0,1,NULL,NULL,NULL,NULL,XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE,NULL,NULL};
 return &value;
}
#define xl6Xui3CodeAnnotationStorePolicy (*xl6Xui3CodeAnnotationStorePolicyGet())
static inline bool xl6Xui3CodeAnnotationSnapshotDestroy(void* raw)
{xuiCodeAnnotationSnapshotDestroy((xui_code_annotation_snapshot)raw);return true;}
static inline const XL6_ForeignViewDescriptorV1* xl6Xui3CodeAnnotationSnapshotPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.codeAnnotationSnapshot",
  0,1,NULL,NULL,xl6Xui3CodeAnnotationSnapshotDestroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
 return &value;
}
#define xl6Xui3CodeAnnotationSnapshotPolicy (*xl6Xui3CodeAnnotationSnapshotPolicyGet())
#endif
