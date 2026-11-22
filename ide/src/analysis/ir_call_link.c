#include "analysis/ir_call_link.h"
xls_ir_call_link* xlsIrCallLink(const XL6_IrModule* m,const xls_ir_call_binding* b,
 size_t count,size_t bytes,int(*cancel)(void*),void* user,xls_ir_graph_status* status,xls_ir_call_error* error)
{return xlsIrRecordLink(m,b,count,XLS_RECORD_CALLS,bytes,cancel,user,status,error);}
int xlsIrCallLinkApply(const xls_ir_call_link* p,XL6_IrModule* m)
{return xlsIrRecordLinkFamilies(p)==XLS_RECORD_CALLS && xlsIrRecordLinkApply(p,m);}
size_t xlsIrCallLinkBytes(const xls_ir_call_link* p){return xlsIrRecordLinkBytes(p);}
size_t xlsIrCallLinkPeakBytes(const xls_ir_call_link* p){return xlsIrRecordLinkPeakBytes(p);}
void xlsIrCallLinkDestroy(xls_ir_call_link* p){xlsIrRecordLinkDestroy(p);}
