#ifndef XLANG_IDE_BUILD_INPUTS_H
#define XLANG_IDE_BUILD_INPUTS_H
#include "model/workspace.h"
typedef struct xide_build_inputs xide_build_inputs;
typedef int (*xide_build_cancel)(void* user);
/* Environment-only snapshot, no disk I/O. NULL means absent, "" means empty.
   Returned text uses malloc/free. Build environment is independent of run env. */
int xideBuildInputsPackagePath(char** out,xide_model_error* error);
/* Worker-only receipt/streaming disk verification. The actual process must have
   exited successfully and both paths must belong to this task's unique stage. */
xide_build_inputs* xideBuildInputsLoad(const char* receipt,const char* artifact,
    const char* packagePath,xide_build_cancel cancel,void* user,xide_model_error* error);
void xideBuildInputsDestroy(xide_build_inputs* inputs);
int xideBuildInputsValidate(xide_build_inputs* inputs,xide_model_error* error);
int xideBuildInputsHasFile(const xide_build_inputs* inputs,const char* path);
/* Worker borrows immutable graph paths: actual files then resolution candidates. */
size_t xideBuildInputsPathCount(const xide_build_inputs* inputs);
const char* xideBuildInputsPath(const xide_build_inputs* inputs,size_t index);
/* Publication target must not overwrite any actual input/candidate, the receipt
   or original artifact, including physical aliases of existing inputs. */
int xideBuildInputsOutputValid(xide_build_inputs* inputs,const char* path,xide_model_error* error);
/* A copied/published artifact has a new identity; verify its actual stable bytes
   against the bound artifact digest. Original stage uses full identity too. */
int xideBuildInputsArtifactValid(xide_build_inputs* inputs,const char* path,xide_model_error* error);
#endif
