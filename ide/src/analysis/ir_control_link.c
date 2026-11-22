#include "analysis/ir_control_link.h"
xls_ir_control_link* xlsIrControlLink(const XL6_IrModule* current,const xls_ir_record_binding* b,
 size_t count,size_t bytes,int(*cancelled)(void*),void* user,xls_ir_graph_status* status,xls_ir_record_error* error)
{return xlsIrRecordLink(current,b,count,XLS_RECORD_CONTROL,bytes,cancelled,user,status,error);}
int xlsIrControlLinkApply(const xls_ir_control_link* link,XL6_IrModule* view)
{return xlsIrRecordLinkFamilies(link)==XLS_RECORD_CONTROL && xlsIrRecordLinkApply(link,view);}
void xlsIrControlLinkDestroy(xls_ir_control_link* link){xlsIrRecordLinkDestroy(link);}
