#include "src/runtime/xl6_array_call_frame.h"
#include "src/runtime/xl6_dict_call_frame.h"
#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_admission.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static void prefixes(const xvalue* source, uint64_t identity, uint64_t element, bool keywords)
{
    size_t point=0;
    for (;point<1000;++point) {
        xrtcallframe fixed={0}, output={.ArgumentCount=123,.Context=(void*)(uintptr_t)1}; void* owner=NULL;
        xl6ProductMemoryBegin(point);
        bool success=keywords ? xl6DictCallFrameExpandV1(identity,element,source,&fixed,&output,&owner) :
            xl6ArrayCallFrameExpandV1(identity,element,source,&fixed,&output,&owner);
        bool hit=xl6ProductMemoryEnd();
        if (hit ? success || owner || xrtErrorKind(xrtGetError())!=XERR_MEMORY : !success || !owner || xrtGetError())
            fprintf(stderr,"native keywords=%d point=%zu hit=%d success=%d kind=%d\n",keywords,point,hit,success,(int)xrtErrorKind(xrtGetError()));
        assert(hit ? !success && !owner && xrtErrorKind(xrtGetError())==XERR_MEMORY : success && owner && !xrtGetError());
        if (success) {
            assert(keywords ? output.KeywordCount==10 && output.ArgumentCount==0 : output.ArgumentCount==10 && output.KeywordCount==0);
            assert(xl6CallFrameExpansionFinishV1(&owner,false));
        } else assert(output.ArgumentCount==123 && output.Context==(void*)(uintptr_t)1);
        assert(!owner); xrtClearError(); xl6ProductMemoryVerify();
        if (!hit) break;
    }
    assert(point<1000);
    printf("native frame keywords=%d prefixes=%zu first-no-hit=%zu balanced\n",keywords,point+1,point);
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    xrtcallframe fixed={0}, output={0}; void* owner=NULL;
    xvalue* object=xrtValueObject(); assert(object);
    assert(xrtValueObjectSetNew(object,(xstrview){"a\0b",3},xrtValueInt(7)));
    assert(xl6DictCallFrameExpandV1(1,1,object,&fixed,&output,&owner) && owner);
    assert(output.ArgumentCount==0 && output.KeywordCount==1 && output.KeywordNames[0].Size==3);
    assert(!memcmp(output.KeywordNames[0].Data,"a\0b",3));
    /* Source mutation cannot invalidate the prepared frame's key/value. */
    assert(xrtValueObjectRemove(object,(xstrview){"a\0b",3}));
    int64_t value=0; assert(xrtValueGetInt(output.KeywordValues[0],&value) && value==7);
    assert(xl6CallFrameExpansionFinishV1(&owner,false) && !owner);
    assert(xl6CallFrameExpansionFinishV1(&owner,false));
    assert(!xl6CallFrameExpansionFinishV1(&owner,true) && !xrtGetError());
    output.ArgumentCount=123; output.Context=(void*)(uintptr_t)1;
    assert(!xl6DictCallFrameExpandV1(0,1,object,&fixed,&output,&owner) && !owner);
    assert(output.ArgumentCount==123 && output.Context==(void*)(uintptr_t)1); xrtClearError();
    xvalue* wireArray=xrtValueArray(); assert(wireArray);
    assert(!xl6ArrayCallFrameExpandV1(1,1,wireArray,&fixed,&output,&owner) && !owner);
    assert(output.ArgumentCount==123); xrtClearError(); xrtValueRelease(wireArray);
    xerror* original=xrtErrorCreate(XERR_VALUE,"test",7,"original"); assert(original);
    xrtSetErrorTake(original); assert(!xl6CallFrameExpansionFinishV1(&owner,true));
    assert(xrtGetError()==original); xrtClearError(); xrtValueRelease(object);
    XL6_DictLayoutV1 layout={1,{2,3,3,sizeof(xvalue*),xrtTypeValue()}};
    xvalue* witness=xl6SequenceReferenceCreateV1(1,2,3,XL6_VALUE_ADMIT_ANY,false,3); assert(witness);
    xvalue* dict=xl6DictCreateTakeV1(&layout,witness); assert(dict);
    dict=xl6DictProjectionTakeV1(dict,&layout,NULL); assert(dict);
    xvalue* array=xl6SequenceReferenceCreateV1(1,4,3,XL6_VALUE_ADMIT_ANY,false,3); assert(array);
    array=xl6ArrayProjectionTakeV1(array,4,3,3,sizeof(xvalue*),xrtTypeValue(),NULL); assert(array);
    for (int i=0;i<10;++i) {
        char key[4]; int size=snprintf(key,sizeof(key),"k%d",i); assert(size>0);
        xvalue* item=xrtValueInt(i); assert(item);
        assert(xl6DictSetV1(dict,&layout,(xstrview){key,(size_t)size},&item));
        assert(xl6ArrayValueAppendTakeV1(array,item));
    }
    prefixes(dict,1,3,true); prefixes(array,4,3,false);
    xrtValueRelease(array); xrtValueRelease(dict); assert(!xrtGetError());
    return 0;
}
