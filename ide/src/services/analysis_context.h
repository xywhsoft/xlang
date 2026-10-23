#ifndef XLANG_IDE_ANALYSIS_CONTEXT_H
#define XLANG_IDE_ANALYSIS_CONTEXT_H
#include "analysis/context.h"
#include "model/workspace.h"
/* Capture an already decoded, available project. NULL configuration selects
 * its active configuration; "" selects the implicit configuration only when
 * no explicit configurations exist. Caller keeps the model stable during
 * capture; the returned context owns everything independently afterwards.
 * Reuses the IDE model; no manifest parsing/loading or environment mutation.
 * Runtime arguments/environment/CWD/output and display metadata are excluded.
 * analysisProfile and the observed package environment are explicit inputs. */
xls_context* xideAnalysisProjectContext(const xide_project* project,const char* configuration,
    const char* analysisProfile,const char* packagePath,xide_model_error* error);
/* Normalize a standalone .xl/.c path against an explicit base, using the
 * file's directory as compile CWD. Path normalization may inspect aliases;
 * do it off the interactive owner when preparing background work. */
xls_context* xideAnalysisFileContext(const char* path,const char* base,
    const char* analysisProfile,const char* packagePath,xide_model_error* error);
#endif
