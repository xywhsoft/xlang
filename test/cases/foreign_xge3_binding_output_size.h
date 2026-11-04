#ifndef XL6_BINDING_OUTPUT_SIZE_H
#define XL6_BINDING_OUTPUT_SIZE_H
#include "test/cases/foreign_managed_view.h"
#include <xge3_runtime.h>
typedef struct BindingSizedInfo { unsigned size; int value; } BindingSizedInfo;
int binding_sized_free(BindingSizedInfo*,int);
void binding_sized_void(BindingSizedInfo*);
int binding_sized_method(ManagedRoot*,BindingSizedInfo*,int);
int binding_sized_multi(unsigned*,BindingSizedInfo*,const uint8_t*,size_t,BindingSizedInfo*);
#endif
