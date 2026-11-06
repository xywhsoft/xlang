#ifndef XIDE_PROGRAM_VERSION_H
#define XIDE_PROGRAM_VERSION_H
#include <stdint.h>
/* The display version belongs to the running program; only the installed
   receipt's monotonic iteration identifies an installed update. */
const char* xideProgramVersion(void);
int64_t xideInstalledIteration(void);
int64_t xideInstalledIterationAt(const char* root);
#endif
