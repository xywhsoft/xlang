#include "xl_dump.h"
#include "../common/xl_app_path.h"

void xlDumpInit(XL_DumpContext* pDump, const char* sRoot)
{
	if ( pDump == NULL ) {
		return;
	}

	if ( sRoot == NULL ) {
		if ( xlAppPathGetDirectory(pDump->sRoot, sizeof(pDump->sRoot)) == XL_RC_OK ) {
			return;
		}
		sRoot = ".";
	}

	snprintf(pDump->sRoot, sizeof(pDump->sRoot), "%s", sRoot);
}

int xlDumpEnsureLayout(XL_DumpContext* pDump)
{
	static const char* arrDirs[] = {
		"cgen",
		"dump",
		"dump\\lexer",
		"dump\\parser",
		"dump\\sem",
		"dump\\lower",
		"dump\\cgen",
		"dump\\runtime",
		"trace",
		"trace\\runtime",
		"tmp"
	};
	char sPath[XL_PATH_MAX_LEN];
	size_t i;

	if ( pDump == NULL ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < XL_COUNTOF(arrDirs); ++i ) {
		xlPathJoin(sPath, sizeof(sPath), pDump->sRoot, arrDirs[i]);
		if ( xlPathEnsureDir(sPath) != XL_RC_OK ) {
			return XL_RC_IO_ERROR;
		}
	}

	return XL_RC_OK;
}

int xlDumpWriteText(XL_DumpContext* pDump, const char* sRelativePath, const char* sText)
{
	char sPath[XL_PATH_MAX_LEN];
	FILE* pFile;

	if ( (pDump == NULL) || (sRelativePath == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlPathJoin(sPath, sizeof(sPath), pDump->sRoot, sRelativePath);
	if ( xlPathEnsureParentDir(sPath) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pFile = fopen(sPath, "wb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}

	if ( sText != NULL ) {
		fputs(sText, pFile);
	}

	fclose(pFile);
	return XL_RC_OK;
}
