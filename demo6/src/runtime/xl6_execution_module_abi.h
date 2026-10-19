#ifndef XL6_EXECUTION_MODULE_ABI_H
#define XL6_EXECUTION_MODULE_ABI_H
#include "xl6_code_lease.h"
typedef struct XL6_ExecutionSourceV1 XL6_ExecutionSourceV1;
struct xrtownershipref;
/* Serialized module creator slots. Private native layout is never generated.
 * Initialization installs only a complete source; retirement preserves the
 * exact slot on BUSY/failure and consumes it only after physical completion. */
XL6_CODE_LEASE_API bool xl6ExecutionModuleInitializeV1(XL6_ExecutionSourceV1**);
XL6_CODE_LEASE_API XL6_ModulePrepareResult xl6ExecutionModulePrepareV1(XL6_ExecutionSourceV1*);
XL6_CODE_LEASE_API XL6_ModuleRetireResult xl6ExecutionModuleRetireV1(XL6_ExecutionSourceV1**,bool);
XL6_CODE_LEASE_API struct xrtownershipref xl6ExecutionSourceOwnershipV1(const XL6_ExecutionSourceV1*);
#endif
