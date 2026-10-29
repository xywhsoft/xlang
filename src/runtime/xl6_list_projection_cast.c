#include "src/runtime/xl6_container_projection.h"
#include "src/runtime/xl6_list.h"
typedef struct Schema { XL6_ListLayoutV1 layout; XL6_SequenceAdmissionV3 policy; } Schema;
static bool capture(void* raw,const xvalue* witness)
{ Schema* schema=raw; return xl6ListSchemaCopyV1(witness,&schema->layout,&schema->policy); }
static bool match(const void* raw,const xvalue* source)
{ const Schema* schema=raw; return xl6ListAdmitsSchemaV1(source,&schema->layout,&schema->policy); }
static void drop(void* raw)
{ Schema* schema=raw; xl6SequenceAdmissionDropV3(&schema->policy); *schema=(Schema){0}; }
static const XL6_ContainerProjectionOpsV1 ops={capture,match,drop};
bool xl6ListProjectionWitnessTakeV1(xvalue* witness,const xvalue* source,bool nullable,xvalue** output)
{ Schema schema={0}; return xl6ContainerProjectionWitnessTakeV1(witness,source,nullable,&schema,&ops,output); }
