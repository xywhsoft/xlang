#ifndef XL6_TEST_FOREIGN_NATIVE_ARGUMENTS_H
#define XL6_TEST_FOREIGN_NATIVE_ARGUMENTS_H
#include "test/cases/foreign_managed_view.h"
typedef struct NativeArgument {int value;} NativeArgument;
const NativeArgument* native_argument(void);
int native_argument_read(const NativeArgument*);
int native_root_argument(ManagedRoot*,const NativeArgument*);
void native_argument_fail_after(int);
#endif
