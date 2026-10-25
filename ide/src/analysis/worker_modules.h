#ifndef XLANG_ANALYSIS_WORKER_MODULES_H
#define XLANG_ANALYSIS_WORKER_MODULES_H
#include "analysis/worker.h"
#include "analysis/module_store.h"
/* Optional shared immutable ordinary syntax;semantic sessions remain isolated.
 * Existing constructors preserve their private-cache behavior. Retains store.
 * No store lock is held while running the compiler or waiting on its actor. */
xls_worker* xlsWorkerCreateWithModules(const xls_session_limits* limits,size_t queueLimit,
    const xls_worker_cache_limits* cache,xls_module_store* store);
#endif
