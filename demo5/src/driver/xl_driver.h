#ifndef XL_DRIVER_H
#define XL_DRIVER_H

#include "../common/xl_common.h"
#include "../ast/xl_ast_dump.h"
#include "../diag/xl_diag.h"
#include "../diag/xl_dump.h"
#include "../lexer/xl_lexer_api.h"
#include "../codegen/c/xl_cgen.h"
#include "../lower/xl_lower.h"
#include "../parser/xl_parser_api.h"
#include "../host/xl_host.h"
#include "../runtime/xl_exec_ctx.h"
#include "../sem/xl_sem_dump.h"
#include "xl_cli.h"
#include "xl_module_link.h"

int xlDriverRun(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);

#endif
