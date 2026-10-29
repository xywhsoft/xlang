#include "src/runtime/xl6_container_projection.h"
#include "src/runtime/xl6_tree.h"
#include "src/api/xl_foreign_error.h"
typedef struct Schema {
    XL6_TreeLayoutV2 layout;
    XL6_SequenceAdmissionV3 key,value;
} Schema;
static bool capture(void* raw,const xvalue* witness)
{ Schema* schema=raw; return xl6TreeSchemaCopyV1(witness,&schema->layout,&schema->key,&schema->value); }
static bool match(const void* raw,const xvalue* source)
{ const Schema* schema=raw; return xl6TreeAdmitsSchemaV1(source,&schema->layout,&schema->key,&schema->value); }
static void drop(void* raw)
{
    Schema* schema=raw;
    xl6SequenceAdmissionDropV3(&schema->value);
    /* Callback-propagated NULL failure is already latched by the cleanup
     * scope. Capture a diagnostic before the second policy cleanup as well. */
    xerror* first=xrtTakeError();
    if(first)xl6ErrorScopePropagateFailure((xl6_error_result){false,first});
    xl6SequenceAdmissionDropV3(&schema->key); *schema=(Schema){0};
}
static const XL6_ContainerProjectionOpsV1 ops={capture,match,drop};
bool xl6TreeProjectionWitnessTakeV1(xvalue* witness,const xvalue* source,bool nullable,xvalue** output)
{ Schema schema={0}; return xl6ContainerProjectionWitnessTakeV1(witness,source,nullable,&schema,&ops,output); }
