#include "test/cases/foreign_native_arguments.h"
#include <assert.h>
static const NativeArgument argument={73};
const NativeArgument* native_argument(void){return &argument;}
int native_argument_read(const NativeArgument* value)
{assert(!value || value==&argument);return value ? value->value : -1;}
int native_root_argument(ManagedRoot* root,const NativeArgument* value)
{assert(root && managed_root_read(root)==42);return native_argument_read(value);}
void native_argument_fail_after(int count){(void)xrtMemDebugFailAfter(count);}
