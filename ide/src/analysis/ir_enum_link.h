#ifndef XLS_IR_ENUM_LINK_H
#define XLS_IR_ENUM_LINK_H
#include "analysis/ir_record_link.h"

typedef xls_ir_record_link xls_ir_enum_link;
/* Link enum layouts, ordered variants and payload slices through the common
 * record owner. Rows and slices are zero-based; type/node/symbol references
 * use their explicit domains. Variant tags and payload ABIs are exact values,
 * never class-runtime tags. Current metadata is a comparison borrow, not a
 * production eligibility certificate. The result pins only selected units;
 * borrowed names outlive the source graph/module. See record-link limits. */
xls_ir_enum_link* xlsIrEnumLink(const XL6_IrModule*,const xls_ir_record_binding*,
 size_t count,size_t bytes,int(*cancelled)(void*),void*,
 xls_ir_graph_status*,xls_ir_record_error*);
/* Replace only the three enum arrays in a readonly caller-owned module view;
 * other arrays need independent owners. Never native-ModuleFree this view. */
int xlsIrEnumLinkApply(const xls_ir_enum_link*,XL6_IrModule*);
void xlsIrEnumLinkDestroy(xls_ir_enum_link*);
#endif
