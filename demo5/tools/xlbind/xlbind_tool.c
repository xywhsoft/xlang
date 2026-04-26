#include "xlbind_tool.h"

#include "../../src/common/xl_app_path.h"
#include "../../src/common/xl_path.h"

static int __xlBindWriteTextFile(const char* sFilePath, const char* sText)
{
	FILE* pFile;

	if ( (sFilePath == NULL) || (sText == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( xlPathEnsureParentDir(sFilePath) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pFile = fopen(sFilePath, "wb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}

	fputs(sText, pFile);
	fclose(pFile);
	return XL_RC_OK;
}

static int __xlBindCopyFileBytes(const char* sSrcPath, const char* sDstPath)
{
	FILE* pSrc;
	FILE* pDst;
	char arrBuf[8192];
	size_t iRead;

	if ( (sSrcPath == NULL) || (sDstPath == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( xlPathEnsureParentDir(sDstPath) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}
	pSrc = fopen(sSrcPath, "rb");
	if ( pSrc == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pDst = fopen(sDstPath, "wb");
	if ( pDst == NULL ) {
		fclose(pSrc);
		return XL_RC_IO_ERROR;
	}
	while ( (iRead = fread(arrBuf, 1, sizeof(arrBuf), pSrc)) != 0 ) {
		if ( fwrite(arrBuf, 1, iRead, pDst) != iRead ) {
			fclose(pSrc);
			fclose(pDst);
			return XL_RC_IO_ERROR;
		}
	}
	if ( ferror(pSrc) ) {
		fclose(pSrc);
		fclose(pDst);
		return XL_RC_IO_ERROR;
	}
	fclose(pSrc);
	fclose(pDst);
	return XL_RC_OK;
}

static void __xlBindFileStem(const char* sPath, char* sOut, size_t iOutSize)
{
	const char* sName;
	const char* sScan;
	const char* sDot;
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	sName = (sPath != NULL) ? sPath : "binding";
	sScan = sName;
	while ( *sScan != '\0' ) {
		if ( xlPathIsSep((unsigned char)*sScan) ) {
			sName = sScan + 1;
		}
		++sScan;
	}
	sDot = strrchr(sName, '.');
	iLen = (sDot != NULL) ? (size_t)(sDot - sName) : strlen(sName);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1u;
	}
	memcpy(sOut, sName, iLen);
	sOut[iLen] = '\0';
}

static void __xlBindSanitizeModuleName(char* sText)
{
	size_t i;

	if ( sText == NULL ) {
		return;
	}
	for ( i = 0; sText[i] != '\0'; ++i ) {
		unsigned char ch = (unsigned char)sText[i];

		if ( ((ch >= 'a') && (ch <= 'z')) ||
			((ch >= 'A') && (ch <= 'Z')) ||
			((ch >= '0') && (ch <= '9')) ||
			(ch == '_') ) {
			continue;
		}
		sText[i] = '_';
	}
	if ( (sText[0] >= '0') && (sText[0] <= '9') ) {
		size_t iLen = strlen(sText);

		if ( (iLen + 2u) < XL_PATH_MAX_LEN ) {
			memmove(sText + 2, sText, iLen + 1u);
			sText[0] = 'b';
			sText[1] = '_';
		}
	}
	if ( sText[0] == '\0' ) {
		snprintf(sText, XL_PATH_MAX_LEN, "binding");
	}
}

void xlBindInit(XLBindOptions* pOpts)
{
	if ( pOpts == NULL ) {
		return;
	}
	memset(pOpts, 0, sizeof(*pOpts));
}

int xlBindParse(int argc, char** argv, XLBindOptions* pOpts)
{
	int i;

	if ( (argv == NULL) || (pOpts == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 1; i < argc; ++i ) {
		const char* sArg;

		sArg = argv[i];
		if ( sArg == NULL ) {
			continue;
		}
		if ( (strcmp(sArg, "-h") == 0) || (strcmp(sArg, "--help") == 0) ) {
			pOpts->bHelp = true;
			continue;
		}
		if ( strcmp(sArg, "--emit") == 0 ) {
			if ( (i + 1) >= argc ) {
				fprintf(stderr, "[error][xlbind] --emit requires a path\n");
				return XL_RC_BAD_ARG;
			}
			pOpts->sEmitPath = argv[++i];
			continue;
		}
		if ( sArg[0] == '-' ) {
			fprintf(stderr, "[error][xlbind] unknown option: %s\n", sArg);
			return XL_RC_BAD_ARG;
		}
		if ( pOpts->sInputFile == NULL ) {
			pOpts->sInputFile = sArg;
			continue;
		}

		fprintf(stderr, "[error][xlbind] unexpected argument: %s\n", sArg);
		return XL_RC_BAD_ARG;
	}

	if ( pOpts->bHelp ) {
		return XL_RC_OK;
	}
	if ( pOpts->sInputFile == NULL ) {
		fprintf(stderr, "[error][xlbind] missing input header file\n");
		return XL_RC_BAD_ARG;
	}
	return XL_RC_OK;
}

void xlBindPrintHelp(const char* sProgName)
{
	if ( sProgName == NULL ) {
		sProgName = "xlbind";
	}

	printf("xlang binding package generator\n\n");
	printf("Usage:\n");
	printf("  %s <header-file> [options]\n\n", sProgName);
	printf("Options:\n");
	printf("  --emit <path>     write package into a directory\n");
	printf("  -h, --help        print help\n");
}

int xlBindRun(const XLBindOptions* pOpts)
{
	char sStem[XL_PATH_MAX_LEN];
	char sOutRoot[XL_PATH_MAX_LEN];
	char sIncludeDir[XL_PATH_MAX_LEN];
	char sHeaderName[XL_PATH_MAX_LEN];
	char sHeaderDst[XL_PATH_MAX_LEN];
	char sSrcDir[XL_PATH_MAX_LEN];
	char sSrcPath[XL_PATH_MAX_LEN];
	char sProjectPath[XL_PATH_MAX_LEN];
	char sAppDir[XL_PATH_MAX_LEN];
	char sJson[XL_DIAG_TEXT_MAX];
	char sModule[XL_DIAG_TEXT_MAX];
	const char* sEmit;
	int iRc;

	if ( (pOpts == NULL) || (pOpts->sInputFile == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlBindFileStem(pOpts->sInputFile, sStem, sizeof(sStem));
	__xlBindSanitizeModuleName(sStem);
	sEmit = pOpts->sEmitPath;
	if ( sEmit != NULL ) {
		snprintf(sOutRoot, sizeof(sOutRoot), "%s", sEmit);
	} else {
		iRc = xlAppPathGetDirectory(sAppDir, sizeof(sAppDir));
		if ( iRc != XL_RC_OK ) {
			fprintf(stderr, "[error][xlbind] failed to resolve executable directory\n");
			return iRc;
		}
		xlPathJoin(sOutRoot, sizeof(sOutRoot), sAppDir, "binding");
		xlPathJoin(sOutRoot, sizeof(sOutRoot), sOutRoot, sStem);
	}

	xlPathJoin(sIncludeDir, sizeof(sIncludeDir), sOutRoot, "include");
	__xlBindFileStem(pOpts->sInputFile, sHeaderName, sizeof(sHeaderName));
	{
		const char* sDot;

		sDot = strrchr(pOpts->sInputFile, '.');
		strncat(sHeaderName, (sDot != NULL) ? sDot : ".h", sizeof(sHeaderName) - strlen(sHeaderName) - 1u);
	}
	xlPathJoin(sHeaderDst, sizeof(sHeaderDst), sIncludeDir, sHeaderName);
	xlPathJoin(sSrcDir, sizeof(sSrcDir), sOutRoot, "src");
	xlPathJoin(sSrcPath, sizeof(sSrcPath), sSrcDir, sStem);
	strncat(sSrcPath, ".xl", sizeof(sSrcPath) - strlen(sSrcPath) - 1u);
	xlPathJoin(sProjectPath, sizeof(sProjectPath), sOutRoot, "xlang.json");

	iRc = __xlBindCopyFileBytes(pOpts->sInputFile, sHeaderDst);
	if ( iRc != XL_RC_OK ) {
		fprintf(stderr, "[error][xlbind] failed to copy header: %s\n", pOpts->sInputFile);
		return iRc;
	}

	snprintf(
		sModule,
		sizeof(sModule),
		"#incpath \"../include\"\n#binding \"../include/%s\"\n\n",
		sHeaderName
	);
	iRc = __xlBindWriteTextFile(sSrcPath, sModule);
	if ( iRc != XL_RC_OK ) {
		fprintf(stderr, "[error][xlbind] failed to write binding module: %s\n", sSrcPath);
		return iRc;
	}

	snprintf(
		sJson,
		sizeof(sJson),
		"{\n\t\"name\": \"binding.%s\",\n\t\"version\": \"0.1.0\",\n\t\"type\": \"package\",\n\t\"source\": [\"src\"],\n\t\"exports\": [\"%s\"],\n\t\"incpath\": [\"include\"]\n}\n",
		sStem,
		sStem
	);
	iRc = __xlBindWriteTextFile(sProjectPath, sJson);
	if ( iRc != XL_RC_OK ) {
		fprintf(stderr, "[error][xlbind] failed to write package file: %s\n", sProjectPath);
		return iRc;
	}

	printf("BindingPackage path=%s module=%s\n", sOutRoot, sStem);
	return XL_RC_OK;
}
