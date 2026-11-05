#ifndef FOREIGN_MODULE_CODE_OWNER_H
#define FOREIGN_MODULE_CODE_OWNER_H
#include <__xl_runtime/xl6_code_lease.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OwnerReceiver OwnerReceiver;
struct OwnerReceiver { int value;int(*invoke)(OwnerReceiver*,const XL6_CodeOwner*,int); };
extern const XL6_ForeignViewDescriptorV1 moduleOwnerPolicy;
int module_owner_capture(const XL6_CodeOwner*,int);
int module_owner_after(int,const XL6_CodeOwner*,const void*);
int module_owner_dual(const XL6_CodeOwner*,int,const XL6_CodeOwner*);
int module_owner_only(const XL6_CodeOwner*,int*);
int module_owner_method(OwnerReceiver*,const XL6_CodeOwner*,int);
int module_owner_create(OwnerReceiver**);
void module_owner_drop(OwnerReceiver*);
void module_owner_expect(const XL6_CodeOwner*);
int module_owner_expected(void);
void module_owner_release(void);
#ifdef __cplusplus
}
#endif
#endif
