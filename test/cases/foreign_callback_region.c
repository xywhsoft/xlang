#include "foreign_callback_region.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

int xl_test_callback_region_read(xl_test_region_read visit,void* user,int mode)
{
    const char pixels[]="a\0cd???wxyz";
    switch(mode){
    case 1:return visit(1,2,NULL,0,user);
    case 2:return visit(1,2,pixels,3,user);
    case 3:return visit(INT_MAX,2,pixels,0,user);
    case 4:return visit(1,0,pixels,0,user);
    case 5:return visit(1,2,pixels,-1,user);
    default:return visit(1,2,pixels,7,user);
    }
}

int xl_test_callback_region_write(xl_test_region_write visit,void* user)
{
    char pixels[]="<....???....>";
    const xl_test_callback_rect rect={1,2};
    int status=visit(rect,pixels+1,7,user);
    if(status!=0)return status;
    return memcmp(pixels,"<a\0cd???wxyz>",13)==0 ? 0:-2;
}
