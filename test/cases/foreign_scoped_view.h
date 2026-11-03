#ifndef XL6_TEST_FOREIGN_SCOPED_VIEW_H
#define XL6_TEST_FOREIGN_SCOPED_VIEW_H
#include "foreign_managed_view.h"
int scoped_fire(ManagedRoot*,int);
int scoped_read(ManagedChild*);
ManagedChild* scoped_identity(ManagedChild*);
ManagedChild* scoped_optional(ManagedChild*);
#endif
