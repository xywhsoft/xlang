#ifndef XL6_XGE3_BINDING_SCALAR_H
#define XL6_XGE3_BINDING_SCALAR_H
#include <stdbool.h>
typedef void (*BindingScalarCallback)(int*,float*,bool*,void*);
int binding_scalar_apply(BindingScalarCallback callback,void* user);
#endif
