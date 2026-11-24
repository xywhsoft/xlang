#include "foreign_callback_writable_span.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int xl_test_writable_span(writable_span_proc callback,void* user,int mode)
{
    char data[6]={'<','a','b','c','d','>'};
    int status;
    switch(mode){
        case 1:return callback(NULL,0,user);
        case 2:return callback(data+1,-1,user);
        case 3:return callback(NULL,4,user);
        default:status=callback(data+1,4,user);break;
    }
    assert(data[0]=='<' && data[5]=='>');
    if(!status)assert(memcmp(data+1,"wA\0d",4)==0);
    return status;
}

int xl_test_writable_span_size(writable_span_size_proc callback,void* user)
{
    char data[]={'<','x','>'};
    int status=callback(data+1,1,user);
    assert(data[0]=='<' && data[2]=='>');
    if(!status)assert(data[1]=='Z');
    return status;
}
