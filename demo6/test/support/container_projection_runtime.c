#include "src/runtime/xl6_container_projection.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
typedef bool (*Project)(xvalue*,const xvalue*,bool,xvalue**);
static const Project project[]={xl6ListProjectionWitnessTakeV1,xl6SetProjectionWitnessTakeV1,
    xl6DictProjectionWitnessTakeV1,xl6TreeProjectionWitnessTakeV1};
static const XL6_SequenceLayoutV1 keyLayout={7200,7201,1,sizeof(int64_t),NULL};
static xvalue* seed(uint64_t id,uint64_t element,unsigned rule)
{ return xl6SequencePrimitiveCreateV1(1,id,element,1,sizeof(int64_t),xrtTypeInt64(),rule,false,element); }
static xvalue* factory(unsigned family,unsigned keyRule,unsigned valueRule)
{
    XL6_SequenceLayoutV1 value={7202,7203,1,sizeof(int64_t),xrtTypeInt64()};
    xvalue* witness=seed(value.identity,value.elementIdentity,valueRule); if(!witness)return NULL;
    switch(family) {
    case 0:{ XL6_ListLayoutV1 layout={7001,value}; return xl6ListCreateTakeV1(&layout,witness); }
    case 1:{ XL6_SetLayoutV1 layout={7001,value}; return xl6SetCreateTakeV1(&layout,witness); }
    case 2:{ XL6_DictLayoutV1 layout={7001,value}; return xl6DictCreateTakeV1(&layout,witness); }
    default:{ XL6_TreeLayoutV1 layout={7001,2,keyLayout,value}; layout.key.type=xrtTypeInt64();
        xvalue* keys=seed(layout.key.identity,layout.key.elementIdentity,keyRule);
        if(!keys){ xrtValueRelease(witness); return NULL; }
        return xl6TreeCreateTakeV1(&layout,keys,witness); }
    }
}
static void fakeDrop(ptr value,ptr context) { (void)value; (void)context; }
static const xvaluehandleops fakeOps={NULL,fakeDrop,NULL,NULL};
static unsigned fault;
static bool capture(void* schema,const xvalue* witness)
{
    xvalue** owned=schema; *owned=xrtValueRetain(witness); assert(*owned);
    if(fault==1)return false;
    if(fault==4){xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});return false;}
    return true;
}
static bool match(const void* schema,const xvalue* source) { (void)schema; (void)source; return true; }
static void drop(void* schema)
{
    xvalue** owned=schema; xrtValueRelease(*owned); *owned=NULL;
    if(fault==2)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if(fault==3 || fault==4)xrtSetErrorKind(XERR_STATE);
}
static const XL6_ContainerProjectionOpsV1 faultOps={capture,match,drop};
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    for(unsigned family=0;family<4;++family) {
        xvalue* source=factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8); assert(source);
        xvalue* output=NULL;
        assert(project[family](factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8),source,false,&output) && output==source);
        xrtValueRelease(output); output=NULL;
        xvalue* wrong=factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I64); assert(wrong);
        assert(!project[family](factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8),wrong,false,&output) && !output);
        assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
        assert(project[family](factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8),wrong,true,&output));
        assert(xrtValueType(output)==XVALUE_NULL); xrtValueRelease(output); output=NULL; xrtValueRelease(wrong);
        ptr pointer=(ptr)(uintptr_t)1; wrong=xrtValueHandleTake(&pointer,&fakeOps,NULL); assert(wrong && !pointer);
        assert(xrtValueTypeIdBind(wrong,7001));
        assert(project[family](factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8),wrong,true,&output));
        assert(xrtValueType(output)==XVALUE_NULL); xrtValueRelease(output); output=NULL; xrtValueRelease(wrong);
        assert(!project[family](factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8),NULL,true,&output) && !output);
        xrtClearError();
        wrong=factory((family+1)%4,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8); assert(wrong);
        assert(project[family](factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8),wrong,true,&output));
        assert(xrtValueType(output)==XVALUE_NULL); xrtValueRelease(output); output=NULL; xrtValueRelease(wrong);
        if(family==3) {
            wrong=factory(family,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_I8); assert(wrong);
            assert(project[family](factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8),wrong,true,&output));
            assert(xrtValueType(output)==XVALUE_NULL); xrtValueRelease(output); xrtValueRelease(wrong);
            XL6_TreeLayoutV1 layout={0}; XL6_SequenceAdmissionV3 key={0},value={0};
            assert(xl6TreeSchemaCopyV1(source,&layout,&key,&value));
            xrtValueRelease(source); source=NULL; /* Independent dual metadata. */
            xvalue* second=factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8); assert(second);
            assert(xrtMemDebugFailAfter(0));
            assert(xl6TreeAdmitsSchemaV1(second,&layout,&key,&value) && !xrtMemDebugFailTriggered());
            xrtMemDebugFailClear(); xl6SequenceAdmissionDropV3(&value); xl6SequenceAdmissionDropV3(&key); xrtValueRelease(second);
        }
        xrtValueRelease(source);
    }
    for(fault=1;fault<=4;++fault) {
        xvalue* source=xrtValueInt(7); assert(source); xvalue* witness=xrtValueInt(8); assert(witness);
        xvalue* schema=NULL,*output=NULL;
        assert(!xl6ContainerProjectionWitnessTakeV1(witness,source,false,&schema,&faultOps,&output) && !output && !schema);
        assert(fault==3 ? xrtErrorKind(xrtGetError())==XERR_STATE : !xrtGetError());
        xrtClearError(); xrtValueRelease(source);
    }
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    for(unsigned family=0;family<4;++family) {
        size_t point;
        for(point=0;point<1000;++point) {
            xl6ProductMemoryBegin(point); xvalue* output=NULL;
            xvalue* source=factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8);
            xvalue* witness=source ? factory(family,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I8) : NULL;
            bool ok=witness && project[family](witness,source,false,&output);
            bool hit=xl6ProductMemoryEnd(); xerror* failure=xrtTakeError();
            assert(hit ? !ok && !output && xrtErrorKind(failure)==XERR_MEMORY : ok && output==source && !failure);
            xrtValueRelease(output); xrtValueRelease(source); xrtErrorFree(failure); xrtClearError(); xl6ProductMemoryVerify();
            if(!hit)break;
        }
        assert(point<1000); printf("family=%u native prefixes=%zu balanced\n",family,point+1);
    }
    puts("complete container schema/key/value/identity/null-diagnostic/cleanup balanced"); return 0;
}
