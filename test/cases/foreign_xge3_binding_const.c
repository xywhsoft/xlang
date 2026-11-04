#include "foreign_xge3_binding_const.h"
int qualification_child_read(const ManagedChild* value){return value ? value->value : -1;}
int qualification_raw_read(const ManagedChild* value){return value ? value->value : -1;}
ManagedChild* qualification_raw_open(void){static ManagedChild value={.value=19};return &value;}
