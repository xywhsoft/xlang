#include "foreign_native_fields_native.c"
#include "foreign_status.h"
int status_zero(int value){return value;}
int status_seventeen(int value){return value;}
int status_diagnostic(void)
{xrtSetErrorTake(xrtErrorCreate(XERR_RANGE,"native.status",91,"original native diagnostic"));return -1;}
int status_no_diagnostic(void){(void)xrtMemDebugFailAfter(0);return -1;}
int status_root(ManagedRoot* root,int value){assert(root && !root->closing);return value;}
