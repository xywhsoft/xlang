#include "test/cases/foreign_xge3_binding_scalar.h"
#include <assert.h>
int binding_scalar_apply(BindingScalarCallback callback,void* user)
{
    int integer=7;float real=1.25f;bool flag=false;
    assert(callback);
    callback(&integer,&real,&flag,user);
    assert(integer==23 && real==2.5f && flag);
    return 0;
}
