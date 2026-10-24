#ifndef XLANG_ANALYSIS_FRONTEND_IR_REGIONS_H
#define XLANG_ANALYSIS_FRONTEND_IR_REGIONS_H
#include "analysis/frontend_ir.h"
#include <xrt.h>
#define XLS_IR_REGION_BYTES (8u*1024u*1024u)
/* Private one-owner read-only view. Initialize to zero; clear before releasing
 * the module. Only the caller's rooted-tree checked,immutable node arena may
 * be used. Rows preserve every BLOCK candidate and original node order. */
typedef struct xls_ir_region_index {
    const XL6_IrModule* module;
    const XL6_IrNode* nodes;
    size_t count;
    xarray rows;
    int ready;
} xls_ir_region_index;
int xlsIrRegionBuild(xls_ir_region_index* index,const XL6_IrModule* module,size_t bytes);
void xlsIrRegionClear(xls_ir_region_index* index);
size_t xlsIrRegionFirst(const xls_ir_region_index* index,const XL6_IrModule* module,uint32 region);
size_t xlsIrRegionNext(const xls_ir_region_index* index,const XL6_IrModule* module,uint32 region,size_t current);
#endif
