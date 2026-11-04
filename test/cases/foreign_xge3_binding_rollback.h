#ifndef XL6_TEST_BINDING_ROLLBACK_H
#define XL6_TEST_BINDING_ROLLBACK_H
#include <xrt_decl.h>
#include "src/runtime/xl6_foreign_view.h"
typedef struct BindingRollbackRoot BindingRollbackRoot;
extern const XL6_ForeignViewDescriptorV1 bindingRollbackPolicy;
int binding_rollback_create(int,BindingRollbackRoot**);
int binding_rollback_release(BindingRollbackRoot*);
int binding_rollback_release_42(BindingRollbackRoot*);
void binding_rollback_reset(void);
int binding_rollback_live(void);
int binding_rollback_calls(void);
#endif
