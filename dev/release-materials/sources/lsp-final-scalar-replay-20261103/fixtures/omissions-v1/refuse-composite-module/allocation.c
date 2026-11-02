#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
extern _Thread_local int xlsNativeTestAllocation;
static unsigned denied;
void* __real_xrtCalloc(size_t,size_t);
void* __wrap_xrtCalloc(size_t count,size_t bytes){if(xlsNativeTestAllocation){++denied;return NULL;}return __real_xrtCalloc(count,bytes);}
static void report(void){if(!denied){fputs("producer allocation not observed\n",stderr);_Exit(2);}printf("observed native allocation refusals:%u\n",denied);}
__attribute__((constructor)) static void setup(void){atexit(report);}
