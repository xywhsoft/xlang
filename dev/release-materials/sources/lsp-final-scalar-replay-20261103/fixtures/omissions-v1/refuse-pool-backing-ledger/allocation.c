#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
extern _Thread_local int xlsNativeTestAllocation;
static unsigned denied;
bool __real_xrtMapSet(xmap*,xbytesview,const void*);
bool __wrap_xrtMapSet(xmap* map,xbytesview key,const void* value){if(xlsNativeTestAllocation){++denied;return false;}return __real_xrtMapSet(map,key,value);}
static void report(void){if(!denied){fputs("producer allocation not observed\n",stderr);_Exit(2);}printf("observed native allocation refusals:%u\n",denied);}
__attribute__((constructor)) static void setup(void){atexit(report);}
