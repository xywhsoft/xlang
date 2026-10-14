#include "xl6_slot_map.h"
#include "xl6_resource_reference_admission.h"
static bool slotMapAdmits(const xvalue* value,uint64_t identity,
    const XL6_SequenceLayoutV1* layout,const XL6_SequenceAdmissionV3* policy)
{ const XL6_SlotMapLayoutV1 expected={identity,*layout}; return xl6SlotMapAdmitsSchemaV1(value,&expected,policy); }
#define XL6_QA_ADMITS slotMapAdmits
#define XL6_QA_PREFIX xl6SlotMap
#define XL6_QA_FACTORY xl6SequenceSlotMapCreateTakeV1
#include "src/runtime/xl6_queue_admission_impl.h"
