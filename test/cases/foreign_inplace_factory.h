#ifndef XL6_TEST_FOREIGN_INPLACE_FACTORY_H
#define XL6_TEST_FOREIGN_INPLACE_FACTORY_H
#include "src/runtime/xl6_foreign_view.h"
#include <xrt_decl.h>
typedef struct InplaceRecord {int refs,mode,value;bool adopted;void* payload;} InplaceRecord;
extern const XL6_ForeignViewDescriptorV1 inplacePolicy;
int inplace_init(InplaceRecord*,int,xstrview);
int inplace_adopt(InplaceRecord*);
void inplace_free(InplaceRecord*);
int inplace_duplicate(InplaceRecord*,InplaceRecord**);
int inplace_from(InplaceRecord*,InplaceRecord*);
int inplace_read(InplaceRecord*);
int inplace_live(void);
void inplace_fail_after(int);
void inplace_reset(void);
#endif
