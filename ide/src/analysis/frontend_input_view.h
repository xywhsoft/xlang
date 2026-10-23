#ifndef XLANG_ANALYSIS_FRONTEND_INPUT_VIEW_H
#define XLANG_ANALYSIS_FRONTEND_INPUT_VIEW_H
#include "analysis/input_view.h"
#include "src/compile/xl6_source_report.h"
/* Owners gate invalidation before passing a report. NULL means no available
 * owner. Read immutable captured rows, not mutable complete/error fields. */
int xlsInputReportInputs(const XL6_SourceReport* report,xls_inputs* inputs);
int xlsInputReportSource(const XL6_SourceReport* report,size_t index,xls_input_source* source);
int xlsInputReportProbe(const XL6_SourceReport* report,size_t index,xls_input_probe* probe);
#endif
