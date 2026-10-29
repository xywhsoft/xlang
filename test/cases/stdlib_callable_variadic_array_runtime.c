#include "src/runtime/xl6_array_call_frame.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_value_format.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ARRAY_ID UINT64_C(0x12345678)
#define VAR_ID UINT64_C(0xabcdefff)

static void vectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* text=xrtValueString((xstrview){"a\0\xe4\xbd\xa0",5});
    const unsigned char binary[]={0,65,255,0};
    xvalue* data=xrtValueBytes((xbytesview){binary,sizeof(binary)});
    xvalue* number=xrtValueInt(42); xvalue* nil=xrtValueNull();
    assert(text && data && number && nil);
    xvalue* arguments[]={number,text,data,nil,text};
    xrtcallframe frame={0}; frame.ArgumentCount=5; frame.Arguments=arguments;
    xvalue* a=xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,&frame,1); assert(a);
    assert(xl6SequenceAdmitsRoleV1(a,XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,VAR_ID,3,sizeof(xvalue*),xrtTypeValue()));
    assert(!xl6SequenceAdmitsRoleV1(a,XL6_SEQUENCE_STACK_V1,ARRAY_ID,VAR_ID,3,sizeof(xvalue*),xrtTypeValue()));
    assert(xl6ArrayErasedSizeV1(a)==4);
    xvalue* first=xl6ArrayErasedReadV1(a,0); assert(first==text); xrtValueRelease(first);
    xvalue* second=xl6ArrayErasedReadV1(a,1); assert(second==data); xrtValueRelease(second);
    xvalue* b=xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,&frame,1); assert(b && a!=b);
    assert(xl6ArrayErasedSetV1(a,0,number));
    first=xl6ArrayErasedReadV1(b,0); assert(first==text); xrtValueRelease(first);
    assert(frame.ArgumentCount==5 && arguments[1]==text);
    xrtValueRelease(text); xrtValueRelease(data); xrtValueRelease(number); xrtValueRelease(nil);
    first=xl6ArrayErasedReadV1(b,0); xstrview view={0};
    assert(first && xrtValueGetString(first,&view) && view.Size==5 && !memcmp(view.Data,"a\0\xe4\xbd\xa0",5));
    xstrbuf display; xrtStrBufInit(&display);
    assert(xl6ValueTextAppendV1(&display,first));
    assert(display.Size==5 && !memcmp(display.Data,"a\0\xe4\xbd\xa0",5));
    xrtStrBufFree(&display); xrtValueRelease(first);
    /* Use the ordinary wire writer as an independent representation oracle,
     * not another native projection. Both nested and mixed carriers must
     * produce identical exact-byte XLON, including bytes and escaped NUL. */
    xvalue* wire=xrtValueArray(); assert(wire);
    for(int64_t i=0;i<4;++i){
        first=xl6ArrayErasedReadV1(b,i); assert(first && xrtValueArrayAppend(wire,first));
        xrtValueRelease(first);
    }
    size_t expectedSize=0; char* expected=xrtXlonStringify(wire,false,&expectedSize); assert(expected);
    xrtStrBufInit(&display); assert(xl6ValueTextAppendV1(&display,b));
    assert(display.Size==expectedSize && !memcmp(display.Data,expected,expectedSize));
    xrtStrBufFree(&display); xrtFree(expected);
    xvalue* children[]={b,wire}; frame.ArgumentCount=2; frame.Arguments=children;
    xvalue* nested=xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,&frame,0); assert(nested);
    xvalue* wireNested=xrtValueArray(); assert(wireNested);
    assert(xrtValueArrayAppend(wireNested,wire) && xrtValueArrayAppend(wireNested,wire));
    expected=xrtXlonStringify(wireNested,false,&expectedSize); assert(expected);
    xrtStrBufInit(&display); assert(xl6ValueTextAppendV1(&display,nested));
    assert(display.Size==expectedSize && !memcmp(display.Data,expected,expectedSize));
    xrtStrBufFree(&display); xrtFree(expected);
    xrtValueRelease(wireNested); xrtValueRelease(nested); xrtValueRelease(wire);
    xrtValueRelease(a); xrtValueRelease(b);
    frame.ArgumentCount=0; frame.Arguments=NULL;
    a=xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,&frame,SIZE_MAX); assert(a && xl6ArrayErasedSizeV1(a)==0); xrtValueRelease(a);
    assert(!xl6ArrayCallFramePackV1(0,VAR_ID,&frame,0) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    assert(!xl6ArrayCallFramePackV1(ARRAY_ID,0,&frame,0) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    assert(!xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,NULL,0) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    frame.ArgumentCount=1; assert(!xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,&frame,0)); xrtClearError();
    xvalue* missing[]={NULL}; frame.Arguments=missing;
    assert(!xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,&frame,0) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    frame.ArgumentCount=SIZE_MAX;
    assert(!xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,&frame,0) && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
int main(int argc,char** argv)
{
    assert(argc<=2);
    if(argc==1){vectors(); puts("native frame vectors balanced"); return 0;}
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* items[24];
    for(size_t i=0;i<24;++i){items[i]=xrtValueInt((int64_t)i); assert(items[i]);}
    xrtcallframe frame={0}; frame.ArgumentCount=24; frame.Arguments=items;
    assert(xrtMemDebugFailAfter((size_t)strtoull(argv[1],NULL,10)));
    xvalue* a=xl6ArrayCallFramePackV1(ARRAY_ID,VAR_ID,&frame,2);
    bool hit=xl6ProductMemoryEnd(); xerror* error=xrtTakeError();
    assert(hit ? !a && xrtErrorKind(error)==XERR_MEMORY : a && !error);
    if(a){assert(xl6ArrayErasedSizeV1(a)==22); xrtValueRelease(a);}
    xrtErrorFree(error); xrtClearError();
    for(size_t i=0;i<24;++i){int64_t value=-1; assert(xrtValueGetInt(items[i],&value) && value==(int64_t)i); xrtValueRelease(items[i]);}
    xl6ProductMemoryVerify();
    printf("native frame point=%s hit=%d balanced\n",argv[1],hit); return 0;
}
