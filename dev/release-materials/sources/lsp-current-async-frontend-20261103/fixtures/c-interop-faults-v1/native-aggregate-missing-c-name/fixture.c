
#include "analysis/frontend_internal.h"
#include <stdio.h>
#include "xl6_ir.c"
static int different(const xls_ir_derived_context* base,const XL6_IrModule* module)
{
 xls_ir_derived_context* other=xlsIrDerivedContextBuild(module,1024u*1024u);
 int result=other && !xlsIrMemoEqual(&base->key,&other->key);xlsIrDerivedContextRelease(other);return result;
}
int main(void)
{
 XL6_IrType type={.kind=XL6_IR_TYPE_C_RECORD,.name="Record",.foreignHeader="record.h"};
 XL6_IrAggregateLayout layout={.type=1,.declaration=1,.firstField=0,.fieldCount=1};
 XL6_IrAggregateField field={.ownerType=1,.type=1,.declaration=2,.name="value",.cName="value"};
 XL6_IrModule module={.types={.items=&type,.count=1,.capacity=1},
  .aggregateLayouts=&layout,.aggregateLayoutCount=1,.aggregateLayoutCapacity=1,
  .aggregateFields=&field,.aggregateFieldCount=1,.aggregateFieldCapacity=1};
 xls_ir_derived_context* base=xlsIrDerivedContextBuild(&module,1024u*1024u);if(!base)return 1;
 for(unsigned change=0;change<5;++change){XL6_IrAggregateField saved=field;
  switch(change){case 0:++field.ownerType;break;case 1:++field.type;break;case 2:++field.declaration;break;
   case 3:field.name="changed";break;default:field.cName[0]='V';break;}
  if(!different(base,&module)){fprintf(stderr,"foreign aggregate key missing field %u\n",change);return 1;}field=saved;
 }
 xlsIrDerivedContextRelease(base);puts("foreign aggregate key passed: five exact native field facts");return 0;
}
