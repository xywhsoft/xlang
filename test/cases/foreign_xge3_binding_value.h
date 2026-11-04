#ifndef XL6_TEST_FOREIGN_BINDING_VALUE_H
#define XL6_TEST_FOREIGN_BINDING_VALUE_H
#include <xrt_decl.h>
#include "src/runtime/xl6_foreign_view.h"
typedef struct BindingValueBox BindingValueBox;
typedef struct BindingValueScope { BindingValueBox* box; } BindingValueScope;
extern const XL6_ForeignViewDescriptorV1 bindingValueBoxPolicy;
int binding_value_create(BindingValueBox**);
void binding_value_destroy(BindingValueBox*);
void binding_value_set(BindingValueBox*,const xvalue*);
const xvalue* binding_value_read(BindingValueBox*);
const xvalue* binding_value_null(BindingValueBox*);
const xvalue* binding_value_identity(const xvalue*);
int binding_value_empty(BindingValueBox*);
int binding_value_checks(void);
#endif
