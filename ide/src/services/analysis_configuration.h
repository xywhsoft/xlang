#ifndef XIDE_ANALYSIS_CONFIGURATION_H
#define XIDE_ANALYSIS_CONFIGURATION_H
#include "services/analysis_context.h"
#include "protocol/analysis_context_wire.h"
/* Capture existing decoded IDE models once when their configuration changes.
   The returned schema1 catalog/packet owns its DOM, excludes execution-only
   settings, and never reloads manifests or changes process state. */
xvalue* xideAnalysisConfigurationPacket(const xide_workspace* workspace,const char* selectedProject,
 const char* configurationOverride,const char* packagePath,int64 epoch,xide_model_error* error);
#endif
