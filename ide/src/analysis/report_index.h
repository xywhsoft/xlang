#ifndef XLS_REPORT_INDEX_H
#define XLS_REPORT_INDEX_H
#include <stddef.h>
struct XL6_SourceReport;
typedef struct xls_report_indexes xls_report_indexes;
typedef struct xls_report_index_work {
 size_t lookups,indexed,comparisons,preparedRows,declines,peakBytes;
} xls_report_index_work;
enum xls_report_index_kind {XLS_REPORT_SOURCE,XLS_REPORT_PROBE};
/* Optional per-call lookup acceleration, never a new resolution/cache policy.
 *2=use the original scan,0=complete miss,1=first native row ordinal in *index.
 * The original append-only report/first answer remains authoritative. Use the
 * existing prepared native lexical keys and XRT AVL, at most8MiB known owned
 * storage across all reports in one frontend call. Any optional failure
 * disables this accelerator for that call, without manufacturing absence.
 * Rows are synchronized once as reports append. No AST/source bodies, disk
 * reads or report ownership. Only the current frontend call may use a scope.
 * Report Init/Free must forget it in EVERY active containing call scope. */
int xlsReportIndexLookup(struct XL6_SourceReport*,const char* absolute,
 enum xls_report_index_kind,size_t* index);
void xlsReportIndexesForget(xls_report_indexes*,const struct XL6_SourceReport*);
void xlsReportIndexesDestroy(xls_report_indexes*);
void xlsReportIndexesWork(const xls_report_indexes*,xls_report_index_work*);
#endif
