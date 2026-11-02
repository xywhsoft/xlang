/* 与 recovery.xl 同一生命周期；C 显式检查返回值和首次诊断。 */
#include "xrt.h"
#include "../include/singleinstance_native.h"
#ifdef __TINYC__
#include "../singleinstance_native.c"
#endif
#include <stdio.h>
int main(void)
{
    int result=1;
    if(xsi_acquire("xlang_recovery_example")!=1)return 1;
    if(!xsi_is_acquired())goto done;
    puts("acquired: true");
    if(xsi_acquire("xlang_recovery_example")!=-1 || xrtErrorKind(xrtGetError())!=XERR_STATE || !xsi_is_acquired())goto done;
    puts("rejected reacquire keeps owner: true");
    if(!xsi_release() || xsi_is_acquired())goto done;
    puts("released: true");
    if(xsi_acquire("xlang_recovery_example")!=1 || !xsi_release() || !xsi_release())goto done;
    puts("retry and repeated release: true");result=0;
done:
    if(!xsi_release())result=1;
    return result;
}
