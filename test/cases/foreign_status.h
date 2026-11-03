#ifndef XL6_TEST_FOREIGN_STATUS_H
#define XL6_TEST_FOREIGN_STATUS_H
#include "foreign_native_fields.h"
int status_zero(int);
int status_seventeen(int);
int status_diagnostic(void);
int status_no_diagnostic(void);
int status_root(ManagedRoot*,int);
#endif
