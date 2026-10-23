#ifndef XLS_DIAGNOSTIC_PATHS_H
#define XLS_DIAGNOSTIC_PATHS_H
#include <xrt.h>
/* Hash the exact ordered UTF-8 path list. Paths are normalized/sorted and
   deduplicated by the host's existing lexical path policy before this call.
   Binary version1: tag+NUL, BE32 count, then BE32 byte length+bytes per path.
   No filesystem access. Output is cleared on failure. */
int xlsDiagnosticPathsDigest(const xvalue* paths,char digest[65]);
#endif
