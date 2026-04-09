#ifndef XL_DUMP_H
#define XL_DUMP_H

#include "../common/xl_common.h"
#include "../common/xl_path.h"

typedef struct XL_DumpContext {
	char sRoot[XL_PATH_MAX_LEN];
} XL_DumpContext;

void xlDumpInit(XL_DumpContext* pDump, const char* sRoot);
int xlDumpEnsureLayout(XL_DumpContext* pDump);
int xlDumpWriteText(XL_DumpContext* pDump, const char* sRelativePath, const char* sText);

#endif
