#ifndef XLS_EXIT_API_H
#define XLS_EXIT_API_H
#include "analysis/ir_profile.h"
#if XLS_EXPERIMENTAL_IR_EXIT_REUSE
#include "analysis/frontend_ir_memo.h"
typedef struct xls_exit_result_work {uint64_t queried,reused,captured,declined,originals,keyBytes,knownBytes,limitBytes;}xls_exit_result_work;
struct xls_semantic_check;
xls_exit_result_work xlsExitResultWork(const xls_ir_result*);
xls_exit_result_work xlsExitCheckWork(const struct xls_semantic_check*);
xls_ir_result* xlsFrontendIrResultBuildTypedCheckedExits(struct xls_typed_module*,XL6_DiagList*,const xls_ir_result*,size_t,const struct xls_entity_view*,size_t,struct xls_sem_memo*,size_t);
int xlsFrontendIrExitAvailable(void);
#endif
#endif
