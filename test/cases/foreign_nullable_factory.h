#ifndef FOREIGN_NULLABLE_FACTORY_H
#define FOREIGN_NULLABLE_FACTORY_H
#include <__xl_runtime/xl6_foreign_view.h>
#include <xrt_decl.h>
typedef struct NullableFactoryObject {int value;} NullableFactoryObject;
static int nullable_factory_live,nullable_factory_calls;
static inline void nullableFactoryDrop(NullableFactoryObject* raw)
{if(raw){--nullable_factory_live;xrtFree(raw);}}
static inline bool nullableFactoryDestroy(void* raw)
{nullableFactoryDrop((NullableFactoryObject*)raw);return true;}
static const XL6_ForeignViewDescriptorV1 nullableFactoryPolicy={
 sizeof(nullableFactoryPolicy),XL6_FOREIGN_VIEW_V1_TAG,"fixture.nullableFactory",
 0,1,NULL,NULL,nullableFactoryDestroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static inline int nullableFactoryCreate(NullableFactoryObject** output,int mode)
{
 ++nullable_factory_calls;*output=NULL;
 if(mode==1)return 0;
 if(mode==2)return -31;
 if(mode==5){xrtSetErrorKind(XERR_ARGUMENT);return 0;}
 NullableFactoryObject* raw=(NullableFactoryObject*)xrtMalloc(sizeof(*raw));
 if(!raw)return -2;
 raw->value=73;*output=raw;++nullable_factory_live;
 if(mode==3 || mode==4)xrtSetErrorKind(XERR_ARGUMENT);
 return mode==3 || mode==6 ? -32:0;
}
static inline int nullableFactoryMethod(NullableFactoryObject* self,NullableFactoryObject** output,int mode)
{if(!self)return -33;return nullableFactoryCreate(output,mode);}
static inline int nullableFactoryLive(void){return nullable_factory_live;}
static inline int nullableFactoryCalls(void){return nullable_factory_calls;}
static inline void nullableFactoryFailAfter(int value){(void)xrtMemDebugFailAfter(value);}
static inline void nullableFactoryFailClear(void){xrtMemDebugFailClear();xrtClearError();}
#endif
