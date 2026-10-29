#include "src/runtime/xl6_value_domain_projection.h"
#include "src/runtime/xl6_container_projection.h"
#include "src/runtime/xl6_sequence.h"
typedef struct Schema { XL6_SequenceAdmissionV3 policy; } Schema;
static bool capture(void* raw,const xvalue* witness)
{
    Schema* schema=raw; XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 layout={0};
    if(!xl6SequenceSchemaCopyV1(witness,&role,&layout,&schema->policy))return false;
    if(role!=XL6_SEQUENCE_ARRAY_V1 || layout.transfer!=3 || layout.size!=sizeof(xvalue*) ||
        layout.type!=xrtTypeValue() || !schema->policy.ops || !schema->policy.ops->Value) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(witness,role,layout.identity,layout.elementIdentity,
        layout.transfer,layout.size,layout.type);
    if(!pin)return false;
    bool empty=xrtTypedArrayCount(xl6SequenceStorageV1(pin))==0; xl6SequenceEndV1(pin);
    if(!empty)xrtSetErrorKind(XERR_ARGUMENT);
    return empty;
}
static bool match(const void* raw,const xvalue* source)
{ const Schema* schema=raw; return schema->policy.ops->Value(schema->policy.data,source); }
static void drop(void* raw)
{ Schema* schema=raw; xl6SequenceAdmissionDropV3(&schema->policy); *schema=(Schema){0}; }
static const XL6_ContainerProjectionOpsV1 ops={capture,match,drop};
bool xl6ValueDomainProjectionWitnessTakeV1(xvalue* witness,const xvalue* source,bool nullable,xvalue** output)
{ Schema schema={0}; return xl6ContainerProjectionWitnessTakeV1(witness,source,nullable,&schema,&ops,output); }
