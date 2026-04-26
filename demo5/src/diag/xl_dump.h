#ifndef XL_DUMP_H
#define XL_DUMP_H

#include "../common/xl_app_path.h"
#include "../common/xl_common.h"
#include "../common/xl_path.h"

typedef struct XL_DumpContext {
	char sRoot[XL_PATH_MAX_LEN];
} XL_DumpContext;

#if XLANG_DEBUGMODE_ENABLED
void xlDumpInit(XL_DumpContext* pDump, const char* sRoot);
int xlDumpEnsureLayout(XL_DumpContext* pDump);
int xlDumpWriteText(XL_DumpContext* pDump, const char* sRelativePath, const char* sText);
#else
static inline void xlDumpInit(XL_DumpContext* pDump, const char* sRoot)
{
	XL_UNUSED(sRoot);
	if ( pDump != NULL ) {
		if ( xlAppPathGetDirectory(pDump->sRoot, sizeof(pDump->sRoot)) != XL_RC_OK ) {
			snprintf(pDump->sRoot, sizeof(pDump->sRoot), ".");
		}
	}
}

static inline int xlDumpEnsureLayout(XL_DumpContext* pDump)
{
	XL_UNUSED(pDump);
	return XL_RC_OK;
}

static inline int xlDumpWriteText(XL_DumpContext* pDump, const char* sRelativePath, const char* sText)
{
	XL_UNUSED(pDump);
	XL_UNUSED(sRelativePath);
	XL_UNUSED(sText);
	return XL_RC_OK;
}
#endif

#endif
