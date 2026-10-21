#ifndef XIDE_C_ANALYSIS_INPUTS_H
#define XIDE_C_ANALYSIS_INPUTS_H
#include "app/xide_app.h"
#include "services/c_completion_job.h"
#define XIDE_C_INPUT_CAPACITY 64u
typedef struct xide_c_input { xide_document* document; uint64_t id; uint32_t version; int external; char *path,*text,*language; xvalue* context; } xide_c_input;
typedef struct xide_c_inputs {
 xide_app* app; xide_c_input items[XIDE_C_INPUT_CAPACITY],xlang[XIDE_C_INPUT_CAPACITY]; size_t count,xlangCount;
 char *server,*program,*directory,*compiler; xvalue *model,*commands,*bridgeProjects;
 xide_c_bridge_spec bridge;
 const char* extras[128]; size_t extraCount;
} xide_c_inputs;
int xideCInputNamed(xide_document* document);
xide_c_inputs* xideCInputsCapture(xide_app* app,xide_document* root,xide_model_error* error);
int xideCInputsCurrent(xide_c_inputs* inputs);
xide_c_input* xideCInputsPath(xide_c_inputs* inputs,const char* path);
xide_c_input* xideCInputsXlangPath(xide_c_inputs* inputs,const char* path);
xide_c_job_spec xideCInputsSpec(xide_c_inputs* inputs,xide_document* root,int offset,void (*wake)(void*));
void xideCInputsDestroy(xide_c_inputs* inputs);
int xideCInputsPrepareNavigation(xide_c_inputs* inputs,xide_document* source,const char* target,xvalue** context);
#endif
