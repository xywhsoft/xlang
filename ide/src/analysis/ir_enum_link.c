#include "analysis/ir_enum_link.h"
xls_ir_enum_link* xlsIrEnumLink(const XL6_IrModule* current,const xls_ir_record_binding* b,
 size_t count,size_t bytes,int(*cancelled)(void*),void* user,xls_ir_graph_status* status,xls_ir_record_error* error)
{return xlsIrRecordLink(current,b,count,XLS_RECORD_ENUMS,bytes,cancelled,user,status,error);}
int xlsIrEnumLinkApply(const xls_ir_enum_link* link,XL6_IrModule* view)
{return xlsIrRecordLinkFamilies(link)==XLS_RECORD_ENUMS && xlsIrRecordLinkApply(link,view);}
void xlsIrEnumLinkDestroy(xls_ir_enum_link* link){xlsIrRecordLinkDestroy(link);}
