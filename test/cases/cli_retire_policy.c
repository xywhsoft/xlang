/* Observe only the shell's wait selection; actual CLI busy-owner execution is
 * qualified separately. Real runtime/compiler APIs supply uncalled CLI paths. */
#define main cli_unused_main
#define xlRuntimeDrainRetiringChecked cliTestDrain
#include "src/driver/xl6_xlc_shell.c"
#undef xlRuntimeDrainRetiringChecked
#undef main
#include <assert.h>
static long long observed;
static unsigned calls;
static XL_RuntimeDrainResult outcome;
XL_RuntimeDrainResult cliTestDrain(long long milliseconds)
{observed=milliseconds;++calls;return outcome;}
int main(void)
{
    outcome=(XL_RuntimeDrainResult){1,1};xlcShellRunFailed=0;
    assert(xlcShellRetireModules(7)==7 && observed==-1 && calls==1);
    outcome=(XL_RuntimeDrainResult){0,1};xlcShellRunFailed=1;
    assert(xlcShellRetireModules(1)==1 && observed==0 && calls==2);
    outcome=(XL_RuntimeDrainResult){1,0};
    assert(xlcShellRetireModules(1)==1 && observed==0 && calls==3);
    outcome=(XL_RuntimeDrainResult){1,1};
    assert(xlcShellRetireModules(1)==1 && observed==0 && calls==4);
    puts("CLI_RETIRE_POLICY_OK");return 0;
}
