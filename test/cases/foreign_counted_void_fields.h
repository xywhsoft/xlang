#ifndef FOREIGN_COUNTED_VOID_FIELDS_H
#define FOREIGN_COUNTED_VOID_FIELDS_H
#include <stddef.h>
#include <stdint.h>
typedef struct counted_void_fields {
#if defined(COUNTED_BAD_POINTER)
 int* data;
#else
 void* data;
#endif
 const void* constant;
#if defined(COUNTED_BAD_LENGTH)
 size_t length;
#else
 int length;
#endif
} counted_void_fields;
static inline counted_void_fields* counted_void_get(int mode)
{
 static uint8_t data[]={0,255,128,0};static counted_void_fields value;
 #if defined(COUNTED_BAD_POINTER)
 value.data=(int*)data;
#else
 value.data=data;
#endif
 value.constant=data;value.length=4;
 if(mode==1){value.data=NULL;value.constant=NULL;value.length=0;}
 if(mode==2)value.length=-1;
 if(mode==3){value.data=NULL;value.constant=NULL;value.length=1;}
 if(mode==4){data[0]=42;}
 if(mode==5){data[0]=0;}
 return &value;
}
#endif
