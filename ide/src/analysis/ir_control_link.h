#ifndef XLS_IR_CONTROL_LINK_H
#define XLS_IR_CONTROL_LINK_H
#include "analysis/ir_record_link.h"
typedef xls_ir_record_link xls_ir_control_link;
/* Initialization,cleanup actions,ordered exit steps and exit plans share
 * the record engine. Scope depths,exit kinds and source evaluation ordinals
 * are exact values;node/type/symbol/region and slice IDs are relocated. */
xls_ir_control_link* xlsIrControlLink(const XL6_IrModule*,const xls_ir_record_binding*,
 size_t count,size_t bytes,int(*cancelled)(void*),void*,
 xls_ir_graph_status*,xls_ir_record_error*);
int xlsIrControlLinkApply(const xls_ir_control_link*,XL6_IrModule*);
void xlsIrControlLinkDestroy(xls_ir_control_link*);
#endif
