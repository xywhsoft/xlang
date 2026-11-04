#ifndef FOREIGN_XGE3_BINDING_CONST_H
#define FOREIGN_XGE3_BINDING_CONST_H
#include "foreign_managed_view.h"
int qualification_child_read(const ManagedChild*);
int qualification_raw_read(const ManagedChild*);
ManagedChild* qualification_raw_open(void);
#endif
