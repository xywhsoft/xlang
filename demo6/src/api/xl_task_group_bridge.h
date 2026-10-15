#ifndef XL_TASK_GROUP_BRIDGE_H
#define XL_TASK_GROUP_BRIDGE_H

#include <stdbool.h>

#include "include/xl.h"
#include "xl_future_bridge.h"



typedef struct xl6_task_group xl6_task_group;

struct xrtownershipref;
struct xrtownershipadapterv1;
struct xrtownershipfinishv1;
struct xcancelwatchownershipv1;
/* Quiescent frozen physical view. Every transitive owner is independently
 * admitted; these queries do not switch the global language collector. */
XL_API struct xrtownershipref xl6TaskGroupOwnershipV1(const xl6_task_group*);
XL_API const struct xrtownershipadapterv1* xl6TaskGroupOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershipfinishv1**);
XL_API const struct xcancelwatchownershipv1* xl6TaskGroupParentPolicyV1Get(void);



XL_API xl6_task_group* xl6TaskGroupCreate(void);
XL_API xl6_task_group* xl6TaskGroupChild(xl6_task_group* pParent);
XL_API void xl6TaskGroupDestroy(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupClose(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupCancel(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupBindParent(
	xl6_task_group* pGroup,
	xfuture* pParent
);
XL_API bool xl6TaskGroupAdd(
	xl6_task_group* pGroup,
	xfuture* pFuture
);
XL_API int xl6TaskGroupCount(xl6_task_group* pGroup);
XL_API int xl6TaskGroupReap(xl6_task_group* pGroup);
XL_API xfuture* xl6TaskGroupFuture(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupJoin(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupJoinFor(
	xl6_task_group* pGroup,
	long long iTimeoutMs
);
XL_API bool xl6TaskGroupJoinUntil(
	xl6_task_group* pGroup,
	long long iDeadlineMs
);

#endif
