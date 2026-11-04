#ifndef FOREIGN_CALLABLE_FIELD_H
#define FOREIGN_CALLABLE_FIELD_H
#include <xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct BindingFieldTable BindingFieldTable;
typedef struct BindingFieldScope { BindingFieldTable* table; } BindingFieldScope;
struct BindingFieldTable {
 int (*add)(BindingFieldTable*,int);
 int (*xrtPathCwd)(BindingFieldTable*,int); /* A field, never XRT's global symbol. */
 size_t (*count)(BindingFieldTable*,const uint8_t*,size_t);
 void (*pair)(BindingFieldTable*,int*,float*);
 int (*checked)(BindingFieldTable*,int);
 xstrview (*echo)(BindingFieldTable*,xstrview);
 int (*apply)(BindingFieldTable*,int,int(*)(int,void*),void*);
 int (*missing)(BindingFieldTable*);
 const xvalue* (*read)(const BindingFieldTable*);
 int (*create)(BindingFieldTable*,BindingFieldTable**);
 int (*missingFactory)(BindingFieldTable*,BindingFieldTable**);
 unsigned depth;
 bool closing;
 xvalue* stored;
};
extern const XL6_ForeignViewDescriptorV1 bindingFieldPolicy;
int binding_field_create(BindingFieldTable**);
void binding_field_destroy(BindingFieldTable*);
BindingFieldTable* binding_field_null(void);
const xvalue* binding_field_identity(const xvalue*);
#endif
