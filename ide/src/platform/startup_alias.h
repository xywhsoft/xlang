#ifndef XIDE_STARTUP_ALIAS_H
#define XIDE_STARTUP_ALIAS_H
#include <wchar.h>
/* Return 1 after a verified alias/staged child exits, 0 for a safe native path
   or verified staged child, and -1 for failure before graphics are opened. */
int xideStartupAliasMaybe(int* exitCode,unsigned long* error);
/* Resource/settings/tool paths use the verified original module after staging. */
const wchar_t* xideStartupResourceModule(const wchar_t* actual);
#endif
