#ifndef XL6_TEST_FOREIGN_MANAGED_POINTEE_H
#define XL6_TEST_FOREIGN_MANAGED_POINTEE_H
#include "test/cases/foreign_managed_view.h"
typedef int (*ManagedPointeeCallback)(ManagedChild**,void*);
int managed_pointee_fire(ManagedRoot*,ManagedPointeeCallback,void*,const ManagedCallbackOwner*,ManagedChild*);
#endif
