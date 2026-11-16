#ifndef XL6_CLI_BUSY_ERROR_H
#define XL6_CLI_BUSY_ERROR_H
#include <__xl_runtime/xl6_foreign_view.h>
typedef struct CliBusy CliBusy;
int cliBusyCreate(CliBusy**);
void cliBusyRollback(CliBusy*);
static inline bool cliBusyDestroy(void* raw){(void)raw;return false;}
static inline bool cliBusyActive(const void* raw){(void)raw;return true;}
static inline const XL6_ForeignViewDescriptorV1* cliBusyPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 d={sizeof(d),XL6_FOREIGN_VIEW_V1_TAG,
        "test.cli.busy",0,1,NULL,NULL,cliBusyDestroy,0,1,NULL,NULL,NULL,cliBusyActive,
        XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,NULL};
    return &d;
}
#define cliBusyPolicy (*cliBusyPolicyGet())
#endif
