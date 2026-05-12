#include "xl_xle_load.h"

#include "../common/xl_path.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif

typedef struct XL_XleJsonCursor {
	const char* pCur;
	const char* pEnd;
} XL_XleJsonCursor;

static int __xlXleResolveModuleFromManifest(const char* sManifestPath, const char* sImportKey, XL_XleResolveInfo* pInfo);
static int __xlXleFindDependencyVersion(const char* sManifestPath, const char* sImportKey, char* sOutVersion, size_t iOutVersionSize);
static int __xlXleParseDependencies(const char* sJson, XL_XleDependencyInfo** ppDeps, size_t* piDepCount);
static int __xlXleParseFunctions(const char* sJson, const char* sModuleKey, XL_XleFunctionInfo** ppFuncs, size_t* piFuncCount);
static int __xlXleParseExports(const char* sJson, const char* sModuleKey, XL_XleExportInfo** ppExports, size_t* piExportCount);
static int __xlXleParseArtifactSection(const char* sJson, XL_XleResolveInfo* pInfo);
static int __xlXleParseArtifactLinkArrays(const char* sJson, XL_XleResolveInfo* pInfo);
static int __xlXleFinalizeArtifactInfo(XL_XleResolveInfo* pInfo, const char* sManifestPath, const char* sModuleFile, int bIsNative);
static int __xlXleTryResolveFromPackageDir(const char* sPackageDir, const char* sImportKey, const char* sVersion, XL_XleResolveInfo* pInfo);

void xlXleResolveInfoInit(XL_XleResolveInfo* pInfo)
{
	if ( pInfo != NULL ) {
		memset(pInfo, 0, sizeof(*pInfo));
	}
}

static int __xlXleFileExists(const char* sFilePath)
{
	FILE* pFile;

	if ( sFilePath == NULL ) {
		return 0;
	}

	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return 0;
	}
	fclose(pFile);
	return 1;
}

static int __xlXleReadFileText(const char* sFilePath, char** psText)
{
	FILE* pFile;
	long iSize;
	size_t iRead;
	char* sText;

	if ( (sFilePath == NULL) || (psText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*psText = NULL;
	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}
	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	iSize = ftell(pFile);
	if ( iSize < 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	sText = (char*)malloc((size_t)iSize + 1);
	if ( sText == NULL ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	iRead = fread(sText, 1, (size_t)iSize, pFile);
	fclose(pFile);
	if ( iRead != (size_t)iSize ) {
		free(sText);
		return XL_RC_IO_ERROR;
	}

	sText[iRead] = '\0';
	*psText = sText;
	return XL_RC_OK;
}

static void __xlXleSkipWs(XL_XleJsonCursor* pCur)
{
	while ( (pCur != NULL) && (pCur->pCur < pCur->pEnd) ) {
		switch ( *pCur->pCur ) {
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				pCur->pCur += 1;
				break;
			default:
				return;
		}
	}
}

static int __xlXleMatchLiteral(XL_XleJsonCursor* pCur, const char* sText)
{
	size_t iLen;

	if ( (pCur == NULL) || (sText == NULL) ) {
		return 0;
	}

	iLen = strlen(sText);
	if ( (size_t)(pCur->pEnd - pCur->pCur) < iLen ) {
		return 0;
	}
	if ( strncmp(pCur->pCur, sText, iLen) != 0 ) {
		return 0;
	}

	pCur->pCur += iLen;
	return 1;
}

static int __xlXleParseString(XL_XleJsonCursor* pCur, char** psText)
{
	char* sText;
	size_t iCap;
	size_t iLen;

	if ( (pCur == NULL) || (psText == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	*psText = NULL;

	__xlXleSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '"') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;

	iCap = 32;
	iLen = 0;
	sText = (char*)malloc(iCap);
	if ( sText == NULL ) {
		return XL_RC_IO_ERROR;
	}

	while ( pCur->pCur < pCur->pEnd ) {
		char ch;

		ch = *pCur->pCur++;
		if ( ch == '"' ) {
			sText[iLen] = '\0';
			*psText = sText;
			return XL_RC_OK;
		}
		if ( ch == '\\' ) {
			if ( pCur->pCur >= pCur->pEnd ) {
				break;
			}
			ch = *pCur->pCur++;
			switch ( ch ) {
				case '"':
				case '\\':
				case '/':
					break;
				case 'b':
					ch = '\b';
					break;
				case 'f':
					ch = '\f';
					break;
				case 'n':
					ch = '\n';
					break;
				case 'r':
					ch = '\r';
					break;
				case 't':
					ch = '\t';
					break;
				case 'u':
					if ( (pCur->pEnd - pCur->pCur) < 4 ) {
						free(sText);
						return XL_RC_FAIL;
					}
					ch = '?';
					pCur->pCur += 4;
					break;
				default:
					free(sText);
					return XL_RC_FAIL;
			}
		}

		if ( (iLen + 2) > iCap ) {
			char* sNewText;

			iCap *= 2;
			sNewText = (char*)realloc(sText, iCap);
			if ( sNewText == NULL ) {
				free(sText);
				return XL_RC_IO_ERROR;
			}
			sText = sNewText;
		}
		sText[iLen++] = ch;
	}

	free(sText);
	return XL_RC_FAIL;
}

static int __xlXleParseBoolLike(XL_XleJsonCursor* pCur, int* pbValue)
{
	const char* pStart;
	char sNumber[16];
	size_t iLen;

	if ( (pCur == NULL) || (pbValue == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlXleSkipWs(pCur);
	if ( __xlXleMatchLiteral(pCur, "true") ) {
		*pbValue = 1;
		return XL_RC_OK;
	}
	if ( __xlXleMatchLiteral(pCur, "false") ) {
		*pbValue = 0;
		return XL_RC_OK;
	}

	pStart = pCur->pCur;
	while ( (pCur->pCur < pCur->pEnd) && ((*pCur->pCur == '-') || ((*pCur->pCur >= '0') && (*pCur->pCur <= '9'))) ) {
		pCur->pCur += 1;
	}
	iLen = (size_t)(pCur->pCur - pStart);
	if ( (iLen == 0) || (iLen >= sizeof(sNumber)) ) {
		return XL_RC_FAIL;
	}

	memcpy(sNumber, pStart, iLen);
	sNumber[iLen] = '\0';
	*pbValue = (atoi(sNumber) != 0) ? 1 : 0;
	return XL_RC_OK;
}

static int __xlXleParseIntLike(XL_XleJsonCursor* pCur, int* piValue)
{
	const char* pStart;
	char sNumber[16];
	size_t iLen;

	if ( (pCur == NULL) || (piValue == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlXleSkipWs(pCur);
	pStart = pCur->pCur;
	if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == '-') ) {
		pCur->pCur += 1;
	}
	while ( (pCur->pCur < pCur->pEnd) && ((*pCur->pCur >= '0') && (*pCur->pCur <= '9')) ) {
		pCur->pCur += 1;
	}
	iLen = (size_t)(pCur->pCur - pStart);
	if ( (iLen == 0) || ((iLen == 1) && (*pStart == '-')) || (iLen >= sizeof(sNumber)) ) {
		return XL_RC_FAIL;
	}

	memcpy(sNumber, pStart, iLen);
	sNumber[iLen] = '\0';
	*piValue = atoi(sNumber);
	return XL_RC_OK;
}

static int __xlXleSkipValue(XL_XleJsonCursor* pCur);

static int __xlXleSkipArray(XL_XleJsonCursor* pCur)
{
	__xlXleSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '[') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;
	__xlXleSkipWs(pCur);
	if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == ']') ) {
		pCur->pCur += 1;
		return XL_RC_OK;
	}

	while ( pCur->pCur < pCur->pEnd ) {
		if ( __xlXleSkipValue(pCur) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(pCur);
		if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == ',') ) {
			pCur->pCur += 1;
			continue;
		}
		if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == ']') ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
		return XL_RC_FAIL;
	}

	return XL_RC_FAIL;
}

static int __xlXleSkipObject(XL_XleJsonCursor* pCur)
{
	char* sKey;

	__xlXleSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '{') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;
	__xlXleSkipWs(pCur);
	if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == '}') ) {
		pCur->pCur += 1;
		return XL_RC_OK;
	}

	while ( pCur->pCur < pCur->pEnd ) {
		sKey = NULL;
		if ( __xlXleParseString(pCur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXleSkipWs(pCur);
		if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != ':') ) {
			return XL_RC_FAIL;
		}
		pCur->pCur += 1;
		if ( __xlXleSkipValue(pCur) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(pCur);
		if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == ',') ) {
			pCur->pCur += 1;
			continue;
		}
		if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == '}') ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
		return XL_RC_FAIL;
	}

	return XL_RC_FAIL;
}

static int __xlXleSkipValue(XL_XleJsonCursor* pCur)
{
	char* sText;

	__xlXleSkipWs(pCur);
	if ( pCur->pCur >= pCur->pEnd ) {
		return XL_RC_FAIL;
	}

	switch ( *pCur->pCur ) {
		case '"':
			sText = NULL;
			if ( __xlXleParseString(pCur, &sText) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
			free(sText);
			return XL_RC_OK;
		case '{':
			return __xlXleSkipObject(pCur);
		case '[':
			return __xlXleSkipArray(pCur);
		case 't':
			return __xlXleMatchLiteral(pCur, "true") ? XL_RC_OK : XL_RC_FAIL;
		case 'f':
			return __xlXleMatchLiteral(pCur, "false") ? XL_RC_OK : XL_RC_FAIL;
		case 'n':
			return __xlXleMatchLiteral(pCur, "null") ? XL_RC_OK : XL_RC_FAIL;
		default:
			while ( pCur->pCur < pCur->pEnd ) {
				switch ( *pCur->pCur ) {
					case ',':
					case '}':
					case ']':
					case ' ':
					case '\t':
					case '\r':
					case '\n':
						return XL_RC_OK;
					default:
						pCur->pCur += 1;
						break;
				}
			}
			return XL_RC_OK;
	}
}

static int __xlXlePathIsAbsolute(const char* sPath)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		return 0;
	}
	if ( xlPathIsSep((unsigned char)sPath[0]) ) {
		return 1;
	}
	if ( ((sPath[0] >= 'A') && (sPath[0] <= 'Z')) || ((sPath[0] >= 'a') && (sPath[0] <= 'z')) ) {
		return sPath[1] == ':';
	}
	return 0;
}

static void __xlXleStripFileName(char* sPath)
{
	size_t iLen;

	if ( sPath == NULL ) {
		return;
	}
	iLen = strlen(sPath);
	while ( iLen > 0 ) {
		if ( xlPathIsSep((unsigned char)sPath[iLen - 1]) ) {
			sPath[iLen - 1] = '\0';
			return;
		}
		--iLen;
	}
}

static void __xlXleNormalizePath(char* sOut, size_t iOutSize, const char* sPath)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sPath == NULL ) {
		sPath = "";
	}

#if defined(_WIN32) || defined(_WIN64)
	if ( GetFullPathNameA(sPath, (DWORD)iOutSize, sOut, NULL) == 0 ) {
		snprintf(sOut, iOutSize, "%s", sPath);
	}
#else
	snprintf(sOut, iOutSize, "%s", sPath);
#endif
}

static void __xlXleCopyText(char* sOut, size_t iOutSize, const char* sText)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sText == NULL ) {
		sText = "";
	}
	snprintf(sOut, iOutSize, "%s", sText);
}

static char* __xlXleDupText(const char* sText)
{
	size_t iLen;
	char* sOut;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	memcpy(sOut, sText, iLen + 1u);
	return sOut;
}

static int __xlXleParsePackageSection(const char* sJson, XL_XleResolveInfo* pInfo)
{
	XL_XleJsonCursor cur;
	char* sKey;

	if ( (sJson == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXleSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			return XL_RC_OK;
		}
		if ( __xlXleParseString(&cur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "package") == 0 ) {
			char* sField;

			__xlXleSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
				free(sKey);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			while ( cur.pCur < cur.pEnd ) {
				char* sValue;

				__xlXleSkipWs(&cur);
				if ( *cur.pCur == '}' ) {
					free(sKey);
					return XL_RC_OK;
				}
				sField = NULL;
				if ( __xlXleParseString(&cur, &sField) != XL_RC_OK ) {
					free(sKey);
					return XL_RC_FAIL;
				}
				__xlXleSkipWs(&cur);
				if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;

				sValue = NULL;
				if ( __xlXleParseString(&cur, &sValue) == XL_RC_OK ) {
					if ( strcmp(sField, "name") == 0 ) {
						__xlXleCopyText(pInfo->sPackageName, sizeof(pInfo->sPackageName), sValue);
					} else if ( strcmp(sField, "version") == 0 ) {
						__xlXleCopyText(pInfo->sPackageVersion, sizeof(pInfo->sPackageVersion), sValue);
					} else if ( strcmp(sField, "abiVersion") == 0 ) {
						__xlXleCopyText(pInfo->sPackageAbiVersion, sizeof(pInfo->sPackageAbiVersion), sValue);
					} else if ( strcmp(sField, "abiId") == 0 ) {
						__xlXleCopyText(pInfo->sPackageAbiId, sizeof(pInfo->sPackageAbiId), sValue);
					}
					free(sValue);
				} else if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}

				free(sField);
				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
				if ( *cur.pCur == '}' ) {
					free(sKey);
					return XL_RC_OK;
				}
			}
			free(sKey);
			return XL_RC_FAIL;
		}

		if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}

	return XL_RC_OK;
}

static int __xlXleParseArtifactSection(const char* sJson, XL_XleResolveInfo* pInfo)
{
	XL_XleJsonCursor cur;
	char* sKey;

	if ( (sJson == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXleSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			return XL_RC_OK;
		}
		if ( __xlXleParseString(&cur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "artifact") == 0 ) {
			__xlXleSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
				free(sKey);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			while ( cur.pCur < cur.pEnd ) {
				char* sField;
				char* sValue;

				__xlXleSkipWs(&cur);
				if ( *cur.pCur == '}' ) {
					free(sKey);
					return XL_RC_OK;
				}
				sField = NULL;
				if ( __xlXleParseString(&cur, &sField) != XL_RC_OK ) {
					free(sKey);
					return XL_RC_FAIL;
				}
				__xlXleSkipWs(&cur);
				if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;
				sValue = NULL;
				if ( __xlXleParseString(&cur, &sValue) == XL_RC_OK ) {
					if ( strcmp(sField, "kind") == 0 ) {
						__xlXleCopyText(pInfo->sArtifactKind, sizeof(pInfo->sArtifactKind), sValue);
					} else if ( strcmp(sField, "path") == 0 ) {
						__xlXleCopyText(pInfo->sArtifactPath, sizeof(pInfo->sArtifactPath), sValue);
					}
					free(sValue);
				} else if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}
				free(sField);
				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
				if ( *cur.pCur == '}' ) {
					free(sKey);
					return XL_RC_OK;
				}
			}
			free(sKey);
			return XL_RC_FAIL;
		}

		if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}

	return XL_RC_OK;
}

static int __xlXleAddManifestTextItem(char arrItems[XL_XLE_LINK_ITEM_MAX][XL_PATH_MAX_LEN], size_t* piCount, const char* sValue)
{
	size_t i;

	if ( (arrItems == NULL) || (piCount == NULL) || (sValue == NULL) || (sValue[0] == '\0') ) {
		return XL_RC_OK;
	}
	for ( i = 0; i < *piCount; ++i ) {
		if ( strcmp(arrItems[i], sValue) == 0 ) {
			return XL_RC_OK;
		}
	}
	if ( *piCount >= XL_XLE_LINK_ITEM_MAX ) {
		return XL_RC_FAIL;
	}
	__xlXleCopyText(arrItems[*piCount], XL_PATH_MAX_LEN, sValue);
	*piCount += 1u;
	return XL_RC_OK;
}

static int __xlXleParseManifestStringArray(
	XL_XleJsonCursor* pCur,
	char arrItems[XL_XLE_LINK_ITEM_MAX][XL_PATH_MAX_LEN],
	size_t* piCount
)
{
	if ( (pCur == NULL) || (arrItems == NULL) || (piCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	__xlXleSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '[') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;
	while ( pCur->pCur < pCur->pEnd ) {
		char* sValue;

		__xlXleSkipWs(pCur);
		if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == ']') ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
		sValue = NULL;
		if ( __xlXleParseString(pCur, &sValue) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		if ( __xlXleAddManifestTextItem(arrItems, piCount, sValue) != XL_RC_OK ) {
			free(sValue);
			return XL_RC_FAIL;
		}
		free(sValue);
		__xlXleSkipWs(pCur);
		if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == ',') ) {
			pCur->pCur += 1;
			continue;
		}
		if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == ']') ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
		return XL_RC_FAIL;
	}
	return XL_RC_FAIL;
}

static int __xlXleParseArtifactLinkArrays(const char* sJson, XL_XleResolveInfo* pInfo)
{
	XL_XleJsonCursor cur;
	char* sKey;

	if ( (sJson == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXleSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;
	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			return XL_RC_OK;
		}
		if ( __xlXleParseString(&cur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;
		if ( strcmp(sKey, "artifact") == 0 ) {
			__xlXleSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
				free(sKey);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			while ( cur.pCur < cur.pEnd ) {
				char* sField;
				int iRc;

				__xlXleSkipWs(&cur);
				if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
					cur.pCur += 1;
					free(sKey);
					return XL_RC_OK;
				}
				sField = NULL;
				if ( __xlXleParseString(&cur, &sField) != XL_RC_OK ) {
					free(sKey);
					return XL_RC_FAIL;
				}
				__xlXleSkipWs(&cur);
				if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;
				iRc = XL_RC_OK;
				if ( strcmp(sField, "linkLibs") == 0 ) {
					iRc = __xlXleParseManifestStringArray(&cur, pInfo->arrLinkLibs, &pInfo->iLinkLibCount);
				} else if ( strcmp(sField, "libPaths") == 0 ) {
					iRc = __xlXleParseManifestStringArray(&cur, pInfo->arrLibPaths, &pInfo->iLibPathCount);
				} else if ( strcmp(sField, "incPaths") == 0 ) {
					iRc = __xlXleParseManifestStringArray(&cur, pInfo->arrIncPaths, &pInfo->iIncPathCount);
				} else if ( strcmp(sField, "cSources") == 0 ) {
					iRc = __xlXleParseManifestStringArray(&cur, pInfo->arrCSourcePaths, &pInfo->iCSourceCount);
				} else {
					iRc = __xlXleSkipValue(&cur);
				}
				free(sField);
				if ( iRc != XL_RC_OK ) {
					free(sKey);
					return iRc;
				}
				__xlXleSkipWs(&cur);
				if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
					cur.pCur += 1;
					continue;
				}
			}
			free(sKey);
			return XL_RC_FAIL;
		}
		if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}
	return XL_RC_OK;
}

static int __xlXleFindDependencyVersionInJson(const char* sJson, const char* sImportKey, char* sOutVersion, size_t iOutVersionSize)
{
	XL_XleJsonCursor cur;
	char* sKey;

	if ( (sJson == NULL) || (sImportKey == NULL) || (sOutVersion == NULL) || (iOutVersionSize == 0) ) {
		return XL_RC_BAD_ARG;
	}

	sOutVersion[0] = '\0';
	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXleSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			return XL_RC_FAIL;
		}
		if ( __xlXleParseString(&cur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "dependencies") == 0 ) {
			__xlXleSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			__xlXleSkipWs(&cur);
			while ( cur.pCur < cur.pEnd ) {
				char* sDepName;
				char* sDepVersion;

				if ( *cur.pCur == ']' ) {
					free(sKey);
					return XL_RC_FAIL;
				}
				if ( *cur.pCur != '{' ) {
					free(sKey);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;
				sDepName = NULL;
				sDepVersion = NULL;
				while ( cur.pCur < cur.pEnd ) {
					char* sField;
					char* sValue;

					__xlXleSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}
					sField = NULL;
					if ( __xlXleParseString(&cur, &sField) != XL_RC_OK ) {
						free(sDepName);
						free(sDepVersion);
						free(sKey);
						return XL_RC_FAIL;
					}
					__xlXleSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sDepName);
						free(sDepVersion);
						free(sKey);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;
					sValue = NULL;
					if ( __xlXleParseString(&cur, &sValue) == XL_RC_OK ) {
						if ( strcmp(sField, "name") == 0 ) {
							free(sDepName);
							sDepName = sValue;
							sValue = NULL;
						} else if ( strcmp(sField, "version") == 0 ) {
							free(sDepVersion);
							sDepVersion = sValue;
							sValue = NULL;
						}
					} else if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
						free(sField);
						free(sValue);
						free(sDepName);
						free(sDepVersion);
						free(sKey);
						return XL_RC_FAIL;
					}
					free(sValue);
					free(sField);
					__xlXleSkipWs(&cur);
					if ( *cur.pCur == ',' ) {
						cur.pCur += 1;
						continue;
					}
				}

				if ( (sDepName != NULL) && (strcmp(sDepName, sImportKey) == 0) && (sDepVersion != NULL) && (sDepVersion[0] != '\0') ) {
					__xlXleCopyText(sOutVersion, iOutVersionSize, sDepVersion);
					free(sDepName);
					free(sDepVersion);
					free(sKey);
					return XL_RC_OK;
				}

				free(sDepName);
				free(sDepVersion);
				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
				if ( *cur.pCur == ']' ) {
					free(sKey);
					return XL_RC_FAIL;
				}
			}
			free(sKey);
			return XL_RC_FAIL;
		}

		if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}

	return XL_RC_FAIL;
}

static int __xlXleFindDependencyVersion(const char* sManifestPath, const char* sImportKey, char* sOutVersion, size_t iOutVersionSize)
{
	char* sJson;
	int iRc;

	if ( (sManifestPath == NULL) || (sImportKey == NULL) || (sOutVersion == NULL) || (iOutVersionSize == 0) ) {
		return XL_RC_BAD_ARG;
	}

	iRc = __xlXleReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	iRc = __xlXleFindDependencyVersionInJson(sJson, sImportKey, sOutVersion, iOutVersionSize);
	free(sJson);
	return iRc;
}

static void __xlXleResolveManifestRelativePath(char* sOut, size_t iOutSize, const char* sManifestPath, const char* sValue)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	sOut[0] = '\0';
	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		return;
	}
	if ( __xlXlePathIsAbsolute(sValue) ) {
		__xlXleNormalizePath(sOut, iOutSize, sValue);
		return;
	}
	if ( (sManifestPath != NULL) && (sManifestPath[0] != '\0') ) {
		char sBaseDir[XL_PATH_MAX_LEN];
		char sJoined[XL_PATH_MAX_LEN];

		snprintf(sBaseDir, sizeof(sBaseDir), "%s", sManifestPath);
		__xlXleStripFileName(sBaseDir);
		xlPathJoin(sJoined, sizeof(sJoined), sBaseDir, sValue);
		__xlXleNormalizePath(sOut, iOutSize, sJoined);
		return;
	}
	__xlXleNormalizePath(sOut, iOutSize, sValue);
}

static int __xlXleFinalizeArtifactInfo(XL_XleResolveInfo* pInfo, const char* sManifestPath, const char* sModuleFile, int bIsNative)
{
	if ( pInfo == NULL ) {
		return XL_RC_BAD_ARG;
	}

	if ( pInfo->sArtifactKind[0] == '\0' ) {
		__xlXleCopyText(pInfo->sArtifactKind, sizeof(pInfo->sArtifactKind), bIsNative ? "native" : "source");
	}
	if ( (pInfo->sArtifactPath[0] == '\0') && (sModuleFile != NULL) && (sModuleFile[0] != '\0') ) {
		__xlXleCopyText(pInfo->sArtifactPath, sizeof(pInfo->sArtifactPath), sModuleFile);
	}

	if ( bIsNative ) {
		if ( strcmp(pInfo->sArtifactKind, "source") == 0 ) {
			return XL_RC_FAIL;
		}
		if ( (strcmp(pInfo->sArtifactKind, "native") != 0) &&
			(strcmp(pInfo->sArtifactKind, "shared") != 0) &&
			(strcmp(pInfo->sArtifactKind, "static") != 0) &&
			(strcmp(pInfo->sArtifactKind, "c-source") != 0) ) {
			return XL_RC_FAIL;
		}
		if ( pInfo->sBindingName[0] == '\0' ) {
			return XL_RC_FAIL;
		}
		if ( strcmp(pInfo->sArtifactKind, "native") == 0 ) {
			__xlXleCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "native-registry");
			__xlXleResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
		} else if ( strcmp(pInfo->sArtifactKind, "shared") == 0 ) {
			__xlXleCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "shared-bridge");
			__xlXleResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
		} else if ( strcmp(pInfo->sArtifactKind, "static") == 0 ) {
			__xlXleCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "static-bridge");
			__xlXleResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
		} else {
			__xlXleCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "c-source-bridge");
			__xlXleResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
		}
	} else {
		size_t i;

		if ( strcmp(pInfo->sArtifactKind, "source") != 0 ) {
			return XL_RC_FAIL;
		}
		__xlXleCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "source-parse");
		__xlXleResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
		for ( i = 0; i < pInfo->iCSourceCount; ++i ) {
			__xlXleResolveManifestRelativePath(
				pInfo->arrCSourceFiles[i],
				sizeof(pInfo->arrCSourceFiles[i]),
				sManifestPath,
				pInfo->arrCSourcePaths[i]
			);
		}
	}

	return XL_RC_OK;
}

static int __xlXleTryResolveFromSearchRoot(const char* sSearchRoot, const char* sImportKey, const char* sVersion, XL_XleResolveInfo* pInfo)
{
	char sPackageRoot[XL_PATH_MAX_LEN];

	if ( (sSearchRoot == NULL) || (sImportKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	snprintf(sPackageRoot, sizeof(sPackageRoot), "%s", sSearchRoot);
	if ( sPackageRoot[0] == '\0' ) {
		return XL_RC_FAIL;
	}
	return __xlXleTryResolveFromPackageDir(sPackageRoot, sImportKey, sVersion, pInfo);
}

static int __xlXleTryResolveFromPackageDir(const char* sPackageDir, const char* sImportKey, const char* sVersion, XL_XleResolveInfo* pInfo)
{
	char sCandidateDir[XL_PATH_MAX_LEN];
	char sCandidateFile[XL_PATH_MAX_LEN];

	if ( (sPackageDir == NULL) || (sImportKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	if ( (sVersion != NULL) && (sVersion[0] != '\0') ) {
		snprintf(sCandidateDir, sizeof(sCandidateDir), "%s@%s", sImportKey, sVersion);
		xlPathJoin(sCandidateFile, sizeof(sCandidateFile), sPackageDir, sCandidateDir);
		xlPathJoin(sCandidateDir, sizeof(sCandidateDir), sCandidateFile, "package.json");
		if ( __xlXleFileExists(sCandidateDir) ) {
			return __xlXleResolveModuleFromManifest(sCandidateDir, sImportKey, pInfo);
		}
	}

	xlPathJoin(sCandidateDir, sizeof(sCandidateDir), sPackageDir, sImportKey);
	xlPathJoin(sCandidateFile, sizeof(sCandidateFile), sCandidateDir, "package.json");
	if ( __xlXleFileExists(sCandidateFile) ) {
		return __xlXleResolveModuleFromManifest(sCandidateFile, sImportKey, pInfo);
	}

	return XL_RC_FAIL;
}

static int __xlXleResolveModuleFromManifest(
	const char* sManifestPath,
	const char* sImportKey,
	XL_XleResolveInfo* pInfo
)
{
	char* sJson;
	XL_XleJsonCursor cur;
	int iRc;
	char* sKey;

	if ( (sManifestPath == NULL) || (sImportKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlXleResolveInfoInit(pInfo);
	__xlXleNormalizePath(pInfo->sManifestPath, sizeof(pInfo->sManifestPath), sManifestPath);
	iRc = __xlXleReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	__xlXleParsePackageSection(sJson, pInfo);
	__xlXleParseArtifactSection(sJson, pInfo);
	__xlXleParseArtifactLinkArrays(sJson, pInfo);

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXleSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		free(sJson);
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			break;
		}
		if ( __xlXleParseString(&cur, &sKey) != XL_RC_OK ) {
			free(sJson);
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			free(sJson);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "modules") == 0 ) {
			__xlXleSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				free(sJson);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			__xlXleSkipWs(&cur);
			while ( cur.pCur < cur.pEnd ) {
				char* sModuleKey;
				char* sModuleFile;
				char* sBindingName;
				char* sInitSymbol;
				char* sExportsSymbol;
				char* sDescriptorSymbol;
				int bIsNative;

				if ( *cur.pCur == ']' ) {
					cur.pCur += 1;
					break;
				}
				if ( *cur.pCur != '{' ) {
					free(sKey);
					free(sJson);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;
				sModuleKey = NULL;
				sModuleFile = NULL;
				sBindingName = NULL;
				sInitSymbol = NULL;
				sExportsSymbol = NULL;
				sDescriptorSymbol = NULL;
				bIsNative = 0;
				while ( cur.pCur < cur.pEnd ) {
					char* sField;

					__xlXleSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}
					sField = NULL;
					if ( __xlXleParseString(&cur, &sField) != XL_RC_OK ) {
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sInitSymbol);
						free(sExportsSymbol);
						free(sDescriptorSymbol);
						free(sKey);
						free(sJson);
						return XL_RC_FAIL;
					}
					__xlXleSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sInitSymbol);
						free(sExportsSymbol);
						free(sDescriptorSymbol);
						free(sKey);
						free(sJson);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;
					if ( strcmp(sField, "key") == 0 ) {
						free(sModuleKey);
						sModuleKey = NULL;
						iRc = __xlXleParseString(&cur, &sModuleKey);
					} else if ( strcmp(sField, "file") == 0 ) {
						free(sModuleFile);
						sModuleFile = NULL;
						iRc = __xlXleParseString(&cur, &sModuleFile);
					} else if ( strcmp(sField, "binding") == 0 ) {
						free(sBindingName);
						sBindingName = NULL;
						iRc = __xlXleParseString(&cur, &sBindingName);
					} else if ( strcmp(sField, "initSymbol") == 0 ) {
						free(sInitSymbol);
						sInitSymbol = NULL;
						iRc = __xlXleParseString(&cur, &sInitSymbol);
					} else if ( strcmp(sField, "exportsSymbol") == 0 ) {
						free(sExportsSymbol);
						sExportsSymbol = NULL;
						iRc = __xlXleParseString(&cur, &sExportsSymbol);
					} else if ( strcmp(sField, "descriptorSymbol") == 0 ) {
						free(sDescriptorSymbol);
						sDescriptorSymbol = NULL;
						iRc = __xlXleParseString(&cur, &sDescriptorSymbol);
					} else if ( strcmp(sField, "native") == 0 ) {
						__xlXleSkipWs(&cur);
						if ( __xlXleMatchLiteral(&cur, "true") || __xlXleMatchLiteral(&cur, "1") ) {
							bIsNative = 1;
							iRc = XL_RC_OK;
						} else if ( __xlXleMatchLiteral(&cur, "false") || __xlXleMatchLiteral(&cur, "0") ) {
							bIsNative = 0;
							iRc = XL_RC_OK;
						} else {
							iRc = XL_RC_FAIL;
						}
					} else {
						iRc = __xlXleSkipValue(&cur);
					}
					free(sField);
					if ( iRc != XL_RC_OK ) {
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sInitSymbol);
						free(sExportsSymbol);
						free(sDescriptorSymbol);
						free(sKey);
						free(sJson);
						return iRc;
					}
					__xlXleSkipWs(&cur);
					if ( *cur.pCur == ',' ) {
						cur.pCur += 1;
						continue;
					}
				}

				if ( (sModuleKey != NULL) && (strcmp(sModuleKey, sImportKey) == 0) ) {
					if ( bIsNative ) {
						__xlXleCopyText(pInfo->sModuleFile, sizeof(pInfo->sModuleFile), (sModuleFile != NULL) ? sModuleFile : "");
						__xlXleCopyText(pInfo->sBindingName, sizeof(pInfo->sBindingName), (sBindingName != NULL) ? sBindingName : sImportKey);
						__xlXleCopyText(pInfo->sInitSymbol, sizeof(pInfo->sInitSymbol), (sInitSymbol != NULL) ? sInitSymbol : "");
						__xlXleCopyText(pInfo->sExportsSymbol, sizeof(pInfo->sExportsSymbol), (sExportsSymbol != NULL) ? sExportsSymbol : "");
						__xlXleCopyText(pInfo->sDescriptorSymbol, sizeof(pInfo->sDescriptorSymbol), (sDescriptorSymbol != NULL) ? sDescriptorSymbol : "");
						iRc = __xlXleFinalizeArtifactInfo(pInfo, sManifestPath, sModuleFile, 1);
						if ( iRc != XL_RC_OK ) {
							free(sModuleKey);
							free(sModuleFile);
							free(sBindingName);
							free(sInitSymbol);
							free(sExportsSymbol);
							free(sDescriptorSymbol);
							free(sKey);
							free(sJson);
							return iRc;
						}
						pInfo->bIsNative = 1;
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sInitSymbol);
						free(sExportsSymbol);
						free(sDescriptorSymbol);
						free(sKey);
						free(sJson);
						return XL_RC_OK;
					}
					if ( (sModuleFile == NULL) || (sModuleFile[0] == '\0') ) {
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sInitSymbol);
						free(sExportsSymbol);
						free(sDescriptorSymbol);
						free(sKey);
						free(sJson);
						return XL_RC_FAIL;
					}
					if ( __xlXlePathIsAbsolute(sModuleFile) ) {
						__xlXleNormalizePath(pInfo->sModuleFile, sizeof(pInfo->sModuleFile), sModuleFile);
					} else {
						char sBaseDir[XL_PATH_MAX_LEN];
						char sJoined[XL_PATH_MAX_LEN];

						snprintf(sBaseDir, sizeof(sBaseDir), "%s", sManifestPath);
						__xlXleStripFileName(sBaseDir);
						xlPathJoin(sJoined, sizeof(sJoined), sBaseDir, sModuleFile);
						__xlXleNormalizePath(pInfo->sModuleFile, sizeof(pInfo->sModuleFile), sJoined);
					}
					iRc = __xlXleFinalizeArtifactInfo(pInfo, sManifestPath, sModuleFile, 0);
					if ( iRc != XL_RC_OK ) {
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sInitSymbol);
						free(sExportsSymbol);
						free(sDescriptorSymbol);
						free(sKey);
						free(sJson);
						return iRc;
					}
					__xlXleCopyText(pInfo->sBindingName, sizeof(pInfo->sBindingName), (sBindingName != NULL) ? sBindingName : "");
					__xlXleCopyText(pInfo->sInitSymbol, sizeof(pInfo->sInitSymbol), (sInitSymbol != NULL) ? sInitSymbol : "");
					__xlXleCopyText(pInfo->sExportsSymbol, sizeof(pInfo->sExportsSymbol), (sExportsSymbol != NULL) ? sExportsSymbol : "");
					__xlXleCopyText(pInfo->sDescriptorSymbol, sizeof(pInfo->sDescriptorSymbol), (sDescriptorSymbol != NULL) ? sDescriptorSymbol : "");
					pInfo->bIsNative = 0;
					free(sModuleKey);
					free(sModuleFile);
					free(sBindingName);
					free(sInitSymbol);
					free(sExportsSymbol);
					free(sDescriptorSymbol);
					free(sKey);
					free(sJson);
					return XL_RC_OK;
				}

				free(sModuleKey);
				free(sModuleFile);
				free(sBindingName);
				free(sInitSymbol);
				free(sExportsSymbol);
				free(sDescriptorSymbol);
				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
			}
		} else {
			iRc = __xlXleSkipValue(&cur);
			if ( iRc != XL_RC_OK ) {
				free(sKey);
				free(sJson);
				return iRc;
			}
		}

		free(sKey);
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			break;
		}
	}

	free(sJson);
	return XL_RC_FAIL;
}

int xlXleFindModuleSource(
	const char* sCurrentFile,
	const char* sCurrentManifestPath,
	const char* sImportKey,
	XL_XleResolveInfo* pInfo
)
{
	char sSearchDir[XL_PATH_MAX_LEN];
	char sVersion[64];

	if ( (sCurrentFile == NULL) || (sImportKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlXleResolveInfoInit(pInfo);
	sVersion[0] = '\0';
	if ( (sCurrentManifestPath != NULL) && (sCurrentManifestPath[0] != '\0') &&
		(__xlXleFindDependencyVersion(sCurrentManifestPath, sImportKey, sVersion, sizeof(sVersion)) == XL_RC_OK) ) {
		snprintf(sSearchDir, sizeof(sSearchDir), "%s", sCurrentManifestPath);
		__xlXleStripFileName(sSearchDir);
		xlPathJoin(sSearchDir, sizeof(sSearchDir), sSearchDir, "libs");
		if ( __xlXleTryResolveFromSearchRoot(sSearchDir, sImportKey, sVersion, pInfo) == XL_RC_OK ) {
			return XL_RC_OK;
		}
	}
	snprintf(sSearchDir, sizeof(sSearchDir), "%s", sCurrentFile);
	__xlXleStripFileName(sSearchDir);
	xlPathJoin(sSearchDir, sizeof(sSearchDir), sSearchDir, "libs");
	return __xlXleTryResolveFromSearchRoot(sSearchDir, sImportKey, NULL, pInfo);
}

int xlXleFindModuleSourceInRoots(
	const char** ppSearchRoots,
	size_t iSearchRootCount,
	const char* sCurrentManifestPath,
	const char* sImportKey,
	XL_XleResolveInfo* pInfo
)
{
	char sVersion[64];
	size_t i;

	if ( (ppSearchRoots == NULL) || (iSearchRootCount == 0) || (sImportKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlXleResolveInfoInit(pInfo);
	sVersion[0] = '\0';
	if ( (sCurrentManifestPath != NULL) && (sCurrentManifestPath[0] != '\0') ) {
		(void)__xlXleFindDependencyVersion(sCurrentManifestPath, sImportKey, sVersion, sizeof(sVersion));
	}
	for ( i = 0; i < iSearchRootCount; ++i ) {
		if ( (ppSearchRoots[i] == NULL) || (ppSearchRoots[i][0] == '\0') ) {
			continue;
		}
		if ( __xlXleTryResolveFromPackageDir(ppSearchRoots[i], sImportKey, sVersion, pInfo) == XL_RC_OK ) {
			return XL_RC_OK;
		}
	}
	return XL_RC_FAIL;
}

int xlXleFindDependencyVersion(
	const char* sManifestPath,
	const char* sImportKey,
	char* sOutVersion,
	size_t iOutVersionSize
)
{
	return __xlXleFindDependencyVersion(sManifestPath, sImportKey, sOutVersion, iOutVersionSize);
}

void xlXleDependencyInfoFree(XL_XleDependencyInfo* pDeps)
{
	free(pDeps);
}

static void __xlXleFunctionInfoReset(XL_XleFunctionInfo* pFunc)
{
	size_t i;

	if ( pFunc == NULL ) {
		return;
	}
	for ( i = 0; i < pFunc->iParamCount; ++i ) {
		free(pFunc->pParams[i].sDefaultValueText);
		free(pFunc->pParams[i].sDefaultValueJson);
	}
	free(pFunc->pParams);
	free(pFunc->pReturns);
	free(pFunc->pTypeParams);
	memset(pFunc, 0, sizeof(*pFunc));
}

static int __xlXleFunctionInfoAddParam(XL_XleFunctionInfo* pFunc, const XL_XleParamInfo* pParam)
{
	XL_XleParamInfo* pNewParams;
	size_t iNewCount;

	if ( (pFunc == NULL) || (pParam == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iNewCount = pFunc->iParamCount + 1;
	pNewParams = (XL_XleParamInfo*)realloc(pFunc->pParams, sizeof(*pFunc->pParams) * iNewCount);
	if ( pNewParams == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pFunc->pParams = pNewParams;
	pFunc->pParams[pFunc->iParamCount] = *pParam;
	pFunc->iParamCount = iNewCount;
	return XL_RC_OK;
}

static int __xlXleFunctionInfoAddReturn(XL_XleFunctionInfo* pFunc, const XL_XleReturnInfo* pReturn)
{
	XL_XleReturnInfo* pNewReturns;
	size_t iNewCount;

	if ( (pFunc == NULL) || (pReturn == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iNewCount = pFunc->iReturnCount + 1;
	pNewReturns = (XL_XleReturnInfo*)realloc(pFunc->pReturns, sizeof(*pFunc->pReturns) * iNewCount);
	if ( pNewReturns == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pFunc->pReturns = pNewReturns;
	pFunc->pReturns[pFunc->iReturnCount] = *pReturn;
	pFunc->iReturnCount = iNewCount;
	return XL_RC_OK;
}

static int __xlXleFunctionInfoAddTypeParam(XL_XleFunctionInfo* pFunc, const XL_XleTypeParamInfo* pTypeParam)
{
	XL_XleTypeParamInfo* pNewTypeParams;
	size_t iNewCount;

	if ( (pFunc == NULL) || (pTypeParam == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iNewCount = pFunc->iTypeParamCount + 1;
	pNewTypeParams = (XL_XleTypeParamInfo*)realloc(pFunc->pTypeParams, sizeof(*pFunc->pTypeParams) * iNewCount);
	if ( pNewTypeParams == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pFunc->pTypeParams = pNewTypeParams;
	pFunc->pTypeParams[pFunc->iTypeParamCount] = *pTypeParam;
	pFunc->iTypeParamCount = iNewCount;
	return XL_RC_OK;
}

void xlXleFunctionInfoFree(XL_XleFunctionInfo* pFuncs, size_t iFuncCount)
{
	size_t i;
	size_t j;

	if ( pFuncs == NULL ) {
		return;
	}

	for ( i = 0; i < iFuncCount; ++i ) {
		for ( j = 0; j < pFuncs[i].iParamCount; ++j ) {
			free(pFuncs[i].pParams[j].sDefaultValueText);
			free(pFuncs[i].pParams[j].sDefaultValueJson);
		}
		free(pFuncs[i].pParams);
		free(pFuncs[i].pReturns);
		free(pFuncs[i].pTypeParams);
	}
	free(pFuncs);
}

void xlXleExportInfoFree(XL_XleExportInfo* pExports)
{
	free(pExports);
}

static int __xlXleExportInfoAdd(XL_XleExportInfo** ppExports, size_t* piExportCount, const XL_XleExportInfo* pExport)
{
	XL_XleExportInfo* pNewExports;
	size_t iNewCount;

	if ( (ppExports == NULL) || (piExportCount == NULL) || (pExport == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iNewCount = (*piExportCount) + 1u;
	pNewExports = (XL_XleExportInfo*)realloc(*ppExports, sizeof(*pNewExports) * iNewCount);
	if ( pNewExports == NULL ) {
		return XL_RC_IO_ERROR;
	}

	*ppExports = pNewExports;
	(*ppExports)[*piExportCount] = *pExport;
	*piExportCount = iNewCount;
	return XL_RC_OK;
}

static int __xlXleParseParams(XL_XleJsonCursor* pCur, XL_XleFunctionInfo* pFunc)
{
	if ( (pCur == NULL) || (pFunc == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlXleSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '[') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;

	while ( pCur->pCur < pCur->pEnd ) {
		XL_XleParamInfo param;

		memset(&param, 0, sizeof(param));
		__xlXleSkipWs(pCur);
		if ( *pCur->pCur == ']' ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
		if ( *pCur->pCur != '{' ) {
			return XL_RC_FAIL;
		}
		pCur->pCur += 1;

		while ( pCur->pCur < pCur->pEnd ) {
			char* sField;
			char* sValue;

			__xlXleSkipWs(pCur);
			if ( *pCur->pCur == '}' ) {
				pCur->pCur += 1;
				break;
			}
			sField = NULL;
			if ( __xlXleParseString(pCur, &sField) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
			__xlXleSkipWs(pCur);
			if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != ':') ) {
				free(sField);
				return XL_RC_FAIL;
			}
			pCur->pCur += 1;
			sValue = NULL;
			if ( __xlXleParseString(pCur, &sValue) == XL_RC_OK ) {
				if ( strcmp(sField, "name") == 0 ) {
					__xlXleCopyText(param.sName, sizeof(param.sName), sValue);
				} else if ( strcmp(sField, "label") == 0 ) {
					__xlXleCopyText(param.sLabel, sizeof(param.sLabel), sValue);
				} else if ( strcmp(sField, "type") == 0 ) {
					__xlXleCopyText(param.sType, sizeof(param.sType), sValue);
				} else if ( strcmp(sField, "defaultValueText") == 0 ) {
					free(param.sDefaultValueText);
					param.sDefaultValueText = __xlXleDupText(sValue);
					if ( param.sDefaultValueText == NULL ) {
						free(sValue);
						free(sField);
						return XL_RC_IO_ERROR;
					}
				} else if ( strcmp(sField, "defaultValueJson") == 0 ) {
					free(param.sDefaultValueJson);
					param.sDefaultValueJson = __xlXleDupText(sValue);
					if ( param.sDefaultValueJson == NULL ) {
						free(sValue);
						free(sField);
						return XL_RC_IO_ERROR;
					}
				}
				free(sValue);
			} else if ( strcmp(sField, "hasDefault") == 0 ) {
				if ( __xlXleParseBoolLike(pCur, &param.bHasDefault) != XL_RC_OK ) {
					free(sField);
					return XL_RC_FAIL;
				}
			} else if ( __xlXleSkipValue(pCur) != XL_RC_OK ) {
				free(sField);
				return XL_RC_FAIL;
			}
			free(sField);
			__xlXleSkipWs(pCur);
			if ( *pCur->pCur == ',' ) {
				pCur->pCur += 1;
				continue;
			}
		}

		if ( __xlXleFunctionInfoAddParam(pFunc, &param) != XL_RC_OK ) {
			free(param.sDefaultValueText);
			free(param.sDefaultValueJson);
			return XL_RC_IO_ERROR;
		}

		__xlXleSkipWs(pCur);
		if ( *pCur->pCur == ',' ) {
			pCur->pCur += 1;
			continue;
		}
		if ( *pCur->pCur == ']' ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
	}

	return XL_RC_FAIL;
}

static int __xlXleParseReturns(XL_XleJsonCursor* pCur, XL_XleFunctionInfo* pFunc)
{
	if ( (pCur == NULL) || (pFunc == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlXleSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '[') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;

	while ( pCur->pCur < pCur->pEnd ) {
		XL_XleReturnInfo ret;

		memset(&ret, 0, sizeof(ret));
		__xlXleSkipWs(pCur);
		if ( *pCur->pCur == ']' ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
		if ( *pCur->pCur != '{' ) {
			return XL_RC_FAIL;
		}
		pCur->pCur += 1;

		while ( pCur->pCur < pCur->pEnd ) {
			char* sField;
			char* sValue;

			__xlXleSkipWs(pCur);
			if ( *pCur->pCur == '}' ) {
				pCur->pCur += 1;
				break;
			}
			sField = NULL;
			if ( __xlXleParseString(pCur, &sField) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
			__xlXleSkipWs(pCur);
			if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != ':') ) {
				free(sField);
				return XL_RC_FAIL;
			}
			pCur->pCur += 1;
			sValue = NULL;
			if ( __xlXleParseString(pCur, &sValue) == XL_RC_OK ) {
				if ( strcmp(sField, "type") == 0 ) {
					__xlXleCopyText(ret.sType, sizeof(ret.sType), sValue);
				}
				free(sValue);
			} else if ( __xlXleSkipValue(pCur) != XL_RC_OK ) {
				free(sField);
				return XL_RC_FAIL;
			}
			free(sField);
			__xlXleSkipWs(pCur);
			if ( *pCur->pCur == ',' ) {
				pCur->pCur += 1;
				continue;
			}
		}

		if ( ret.sType[0] != '\0' ) {
			if ( __xlXleFunctionInfoAddReturn(pFunc, &ret) != XL_RC_OK ) {
				return XL_RC_IO_ERROR;
			}
		}

		__xlXleSkipWs(pCur);
		if ( *pCur->pCur == ',' ) {
			pCur->pCur += 1;
			continue;
		}
		if ( *pCur->pCur == ']' ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
	}

	return XL_RC_FAIL;
}

static int __xlXleParseTypeParams(XL_XleJsonCursor* pCur, XL_XleFunctionInfo* pFunc)
{
	if ( (pCur == NULL) || (pFunc == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlXleSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '[') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;

	while ( pCur->pCur < pCur->pEnd ) {
		XL_XleTypeParamInfo typeParam;

		memset(&typeParam, 0, sizeof(typeParam));
		__xlXleSkipWs(pCur);
		if ( *pCur->pCur == ']' ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
		if ( *pCur->pCur != '{' ) {
			return XL_RC_FAIL;
		}
		pCur->pCur += 1;

		while ( pCur->pCur < pCur->pEnd ) {
			char* sField;
			char* sValue;

			__xlXleSkipWs(pCur);
			if ( *pCur->pCur == '}' ) {
				pCur->pCur += 1;
				break;
			}
			sField = NULL;
			if ( __xlXleParseString(pCur, &sField) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
			__xlXleSkipWs(pCur);
			if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != ':') ) {
				free(sField);
				return XL_RC_FAIL;
			}
			pCur->pCur += 1;
			sValue = NULL;
			if ( __xlXleParseString(pCur, &sValue) == XL_RC_OK ) {
				if ( strcmp(sField, "name") == 0 ) {
					__xlXleCopyText(typeParam.sName, sizeof(typeParam.sName), sValue);
				}
				free(sValue);
			} else if ( __xlXleSkipValue(pCur) != XL_RC_OK ) {
				free(sField);
				return XL_RC_FAIL;
			}
			free(sField);
			__xlXleSkipWs(pCur);
			if ( *pCur->pCur == ',' ) {
				pCur->pCur += 1;
				continue;
			}
		}

		if ( typeParam.sName[0] != '\0' ) {
			if ( __xlXleFunctionInfoAddTypeParam(pFunc, &typeParam) != XL_RC_OK ) {
				return XL_RC_IO_ERROR;
			}
		}

		__xlXleSkipWs(pCur);
		if ( *pCur->pCur == ',' ) {
			pCur->pCur += 1;
			continue;
		}
		if ( *pCur->pCur == ']' ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
	}

	return XL_RC_FAIL;
}

static int __xlXleParseModuleExports(
	XL_XleJsonCursor* pCur,
	const char* sFilterModuleKey,
	const char* sModuleKey,
	const char* sModuleAbiId,
	XL_XleExportInfo** ppExports,
	size_t* piExportCount
)
{
	if ( (pCur == NULL) || (ppExports == NULL) || (piExportCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlXleSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '[') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;

	while ( pCur->pCur < pCur->pEnd ) {
		XL_XleExportInfo info;

		memset(&info, 0, sizeof(info));
		__xlXleCopyText(info.sModule, sizeof(info.sModule), sModuleKey);
		__xlXleCopyText(info.sModuleAbiId, sizeof(info.sModuleAbiId), sModuleAbiId);

		__xlXleSkipWs(pCur);
		if ( *pCur->pCur == ']' ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
		if ( *pCur->pCur != '{' ) {
			return XL_RC_FAIL;
		}
		pCur->pCur += 1;

		while ( pCur->pCur < pCur->pEnd ) {
			char* sField;
			char* sValue;

			__xlXleSkipWs(pCur);
			if ( *pCur->pCur == '}' ) {
				pCur->pCur += 1;
				break;
			}

			sField = NULL;
			if ( __xlXleParseString(pCur, &sField) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
			__xlXleSkipWs(pCur);
			if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != ':') ) {
				free(sField);
				return XL_RC_FAIL;
			}
			pCur->pCur += 1;

			sValue = NULL;
			if ( __xlXleParseString(pCur, &sValue) == XL_RC_OK ) {
				if ( strcmp(sField, "name") == 0 ) {
					__xlXleCopyText(info.sName, sizeof(info.sName), sValue);
				} else if ( strcmp(sField, "symbol") == 0 ) {
					__xlXleCopyText(info.sSymbol, sizeof(info.sSymbol), sValue);
				} else if ( strcmp(sField, "abiId") == 0 ) {
					__xlXleCopyText(info.sAbiId, sizeof(info.sAbiId), sValue);
				} else if ( strcmp(sField, "versionedSymbol") == 0 ) {
					__xlXleCopyText(info.sVersionedSymbol, sizeof(info.sVersionedSymbol), sValue);
				} else if ( strcmp(sField, "kind") == 0 ) {
					__xlXleCopyText(info.sKind, sizeof(info.sKind), sValue);
				}
				free(sValue);
			} else if ( strcmp(sField, "overloadCount") == 0 ) {
				if ( __xlXleParseBoolLike(pCur, &info.iOverloadCount) != XL_RC_OK ) {
					free(sField);
					return XL_RC_FAIL;
				}
			} else if ( __xlXleSkipValue(pCur) != XL_RC_OK ) {
				free(sField);
				return XL_RC_FAIL;
			}

			free(sField);
			__xlXleSkipWs(pCur);
			if ( *pCur->pCur == ',' ) {
				pCur->pCur += 1;
				continue;
			}
		}

		if ( ((sFilterModuleKey == NULL) || (sFilterModuleKey[0] == '\0') || (strcmp(info.sModule, sFilterModuleKey) == 0)) &&
			(info.sName[0] != '\0') &&
			(info.sSymbol[0] != '\0') ) {
			if ( __xlXleExportInfoAdd(ppExports, piExportCount, &info) != XL_RC_OK ) {
				return XL_RC_IO_ERROR;
			}
		}

		__xlXleSkipWs(pCur);
		if ( *pCur->pCur == ',' ) {
			pCur->pCur += 1;
			continue;
		}
		if ( *pCur->pCur == ']' ) {
			pCur->pCur += 1;
			return XL_RC_OK;
		}
	}

	return XL_RC_FAIL;
}

static int __xlXleParseFunctions(const char* sJson, const char* sModuleKey, XL_XleFunctionInfo** ppFuncs, size_t* piFuncCount)
{
	XL_XleJsonCursor cur;
	char* sKey;
	XL_XleFunctionInfo* pFuncs;
	size_t iFuncCount;

	if ( (sJson == NULL) || (ppFuncs == NULL) || (piFuncCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppFuncs = NULL;
	*piFuncCount = 0;
	pFuncs = NULL;
	iFuncCount = 0;
	sKey = NULL;

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXleSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			break;
		}
		if ( __xlXleParseString(&cur, &sKey) != XL_RC_OK ) {
			xlXleFunctionInfoFree(pFuncs, iFuncCount);
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			xlXleFunctionInfoFree(pFuncs, iFuncCount);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "functions") == 0 ) {
			__xlXleSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				xlXleFunctionInfoFree(pFuncs, iFuncCount);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;

			while ( cur.pCur < cur.pEnd ) {
				XL_XleFunctionInfo func;
				int bKeepFunc;

				memset(&func, 0, sizeof(func));
				bKeepFunc = 1;
				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ']' ) {
					cur.pCur += 1;
					break;
				}
				if ( *cur.pCur != '{' ) {
					free(sKey);
					xlXleFunctionInfoFree(pFuncs, iFuncCount);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;

				while ( cur.pCur < cur.pEnd ) {
					char* sField;
					char* sValue;

					__xlXleSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}
					sField = NULL;
					if ( __xlXleParseString(&cur, &sField) != XL_RC_OK ) {
						free(sKey);
						__xlXleFunctionInfoReset(&func);
						xlXleFunctionInfoFree(pFuncs, iFuncCount);
						return XL_RC_FAIL;
					}
					__xlXleSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sKey);
						__xlXleFunctionInfoReset(&func);
						xlXleFunctionInfoFree(pFuncs, iFuncCount);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;
					sValue = NULL;
					if ( __xlXleParseString(&cur, &sValue) == XL_RC_OK ) {
						if ( strcmp(sField, "module") == 0 ) {
							__xlXleCopyText(func.sModule, sizeof(func.sModule), sValue);
						} else if ( strcmp(sField, "moduleAbiId") == 0 ) {
							__xlXleCopyText(func.sModuleAbiId, sizeof(func.sModuleAbiId), sValue);
						} else if ( strcmp(sField, "exportName") == 0 ) {
							__xlXleCopyText(func.sExportName, sizeof(func.sExportName), sValue);
						} else if ( strcmp(sField, "receiverType") == 0 ) {
							__xlXleCopyText(func.sReceiverType, sizeof(func.sReceiverType), sValue);
						} else if ( strcmp(sField, "methodName") == 0 ) {
							__xlXleCopyText(func.sMethodName, sizeof(func.sMethodName), sValue);
						} else if ( strcmp(sField, "symbol") == 0 ) {
							__xlXleCopyText(func.sSymbol, sizeof(func.sSymbol), sValue);
						} else if ( strcmp(sField, "overloadGroup") == 0 ) {
							__xlXleCopyText(func.sOverloadGroup, sizeof(func.sOverloadGroup), sValue);
						} else if ( strcmp(sField, "signatureId") == 0 ) {
							__xlXleCopyText(func.sSignatureId, sizeof(func.sSignatureId), sValue);
						} else if ( strcmp(sField, "dispatchKey") == 0 ) {
							__xlXleCopyText(func.sDispatchKey, sizeof(func.sDispatchKey), sValue);
						} else if ( strcmp(sField, "languageAbi") == 0 ) {
							__xlXleCopyText(func.sLanguageAbi, sizeof(func.sLanguageAbi), sValue);
						} else if ( strcmp(sField, "nativeAbi") == 0 ) {
							__xlXleCopyText(func.sNativeAbi, sizeof(func.sNativeAbi), sValue);
						} else if ( strcmp(sField, "languageAbiId") == 0 ) {
							__xlXleCopyText(func.sLanguageAbiId, sizeof(func.sLanguageAbiId), sValue);
						} else if ( strcmp(sField, "nativeAbiId") == 0 ) {
							__xlXleCopyText(func.sNativeAbiId, sizeof(func.sNativeAbiId), sValue);
						} else if ( strcmp(sField, "abiSymbol") == 0 ) {
							__xlXleCopyText(func.sAbiSymbol, sizeof(func.sAbiSymbol), sValue);
						} else if ( strcmp(sField, "returnType") == 0 ) {
							__xlXleCopyText(func.sReturnType, sizeof(func.sReturnType), sValue);
						} else if ( strcmp(sField, "storageReturnType") == 0 ) {
							__xlXleCopyText(func.sStorageReturnType, sizeof(func.sStorageReturnType), sValue);
						}
						free(sValue);
					} else if ( strcmp(sField, "params") == 0 ) {
						if ( __xlXleParseParams(&cur, &func) != XL_RC_OK ) {
							free(sField);
							free(sKey);
							__xlXleFunctionInfoReset(&func);
							xlXleFunctionInfoFree(pFuncs, iFuncCount);
							return XL_RC_FAIL;
						}
					} else if ( strcmp(sField, "returns") == 0 ) {
						if ( __xlXleParseReturns(&cur, &func) != XL_RC_OK ) {
							free(sField);
							free(sKey);
							__xlXleFunctionInfoReset(&func);
							xlXleFunctionInfoFree(pFuncs, iFuncCount);
							return XL_RC_FAIL;
						}
					} else if ( strcmp(sField, "typeParams") == 0 ) {
						if ( __xlXleParseTypeParams(&cur, &func) != XL_RC_OK ) {
							free(sField);
							free(sKey);
							__xlXleFunctionInfoReset(&func);
							xlXleFunctionInfoFree(pFuncs, iFuncCount);
							return XL_RC_FAIL;
						}
					} else if ( (strcmp(sField, "async") == 0) || (strcmp(sField, "generator") == 0) ) {
						int bValue;
						if ( __xlXleParseBoolLike(&cur, &bValue) != XL_RC_OK ) {
							free(sField);
							free(sKey);
							__xlXleFunctionInfoReset(&func);
							xlXleFunctionInfoFree(pFuncs, iFuncCount);
							return XL_RC_FAIL;
						}
						if ( strcmp(sField, "async") == 0 ) {
							func.bAsync = bValue;
						} else {
							func.bGenerator = bValue;
						}
					} else if ( strcmp(sField, "varargKind") == 0 ) {
						int iValue;
						if ( __xlXleParseIntLike(&cur, &iValue) != XL_RC_OK ) {
							free(sField);
							free(sKey);
							__xlXleFunctionInfoReset(&func);
							xlXleFunctionInfoFree(pFuncs, iFuncCount);
							return XL_RC_FAIL;
						}
						func.iVarArgKind = iValue;
					} else if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
						free(sField);
						free(sKey);
						__xlXleFunctionInfoReset(&func);
						xlXleFunctionInfoFree(pFuncs, iFuncCount);
						return XL_RC_FAIL;
					}
					free(sField);
					__xlXleSkipWs(&cur);
					if ( *cur.pCur == ',' ) {
						cur.pCur += 1;
						continue;
					}
				}

				if ( (sModuleKey != NULL) && (sModuleKey[0] != '\0') && (func.sModule[0] != '\0') &&
					(strcmp(func.sModule, sModuleKey) != 0) ) {
					bKeepFunc = 0;
				}
				if ( (func.sExportName[0] == '\0') || (func.sSymbol[0] == '\0') ) {
					bKeepFunc = 0;
				}

				if ( bKeepFunc ) {
					XL_XleFunctionInfo* pNewFuncs;

					pNewFuncs = (XL_XleFunctionInfo*)realloc(pFuncs, sizeof(*pFuncs) * (iFuncCount + 1));
					if ( pNewFuncs == NULL ) {
						free(sKey);
						__xlXleFunctionInfoReset(&func);
						xlXleFunctionInfoFree(pFuncs, iFuncCount);
						return XL_RC_IO_ERROR;
					}
					pFuncs = pNewFuncs;
					pFuncs[iFuncCount++] = func;
					memset(&func, 0, sizeof(func));
				}
				__xlXleFunctionInfoReset(&func);

				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
				if ( *cur.pCur == ']' ) {
					cur.pCur += 1;
					break;
				}
			}
			break;
		}

		if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			xlXleFunctionInfoFree(pFuncs, iFuncCount);
			return XL_RC_FAIL;
		}
		free(sKey);
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}

	free(sKey);
	*ppFuncs = pFuncs;
	*piFuncCount = iFuncCount;
	return XL_RC_OK;
}

static int __xlXleParseDependencies(const char* sJson, XL_XleDependencyInfo** ppDeps, size_t* piDepCount)
{
	XL_XleJsonCursor cur;
	char* sKey;
	XL_XleDependencyInfo* pDeps;
	size_t iDepCount;
	size_t iDepCap;

	if ( (sJson == NULL) || (ppDeps == NULL) || (piDepCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppDeps = NULL;
	*piDepCount = 0;
	sKey = NULL;
	pDeps = NULL;
	iDepCount = 0;
	iDepCap = 0;

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXleSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			break;
		}
		if ( __xlXleParseString(&cur, &sKey) != XL_RC_OK ) {
			free(pDeps);
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			free(pDeps);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "dependencies") == 0 ) {
			__xlXleSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				free(pDeps);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			__xlXleSkipWs(&cur);
			while ( cur.pCur < cur.pEnd ) {
				XL_XleDependencyInfo dep;

				if ( *cur.pCur == ']' ) {
					cur.pCur += 1;
					break;
				}
				if ( *cur.pCur != '{' ) {
					free(sKey);
					free(pDeps);
					return XL_RC_FAIL;
				}
				memset(&dep, 0, sizeof(dep));
				cur.pCur += 1;
				while ( cur.pCur < cur.pEnd ) {
					char* sField;
					char* sValue;

					__xlXleSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}
					sField = NULL;
					if ( __xlXleParseString(&cur, &sField) != XL_RC_OK ) {
						free(sKey);
						free(pDeps);
						return XL_RC_FAIL;
					}
					__xlXleSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sKey);
						free(pDeps);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;
					sValue = NULL;
					if ( __xlXleParseString(&cur, &sValue) == XL_RC_OK ) {
						if ( strcmp(sField, "name") == 0 ) {
							__xlXleCopyText(dep.sName, sizeof(dep.sName), sValue);
						} else if ( strcmp(sField, "kind") == 0 ) {
							__xlXleCopyText(dep.sKind, sizeof(dep.sKind), sValue);
						} else if ( strcmp(sField, "version") == 0 ) {
							__xlXleCopyText(dep.sVersion, sizeof(dep.sVersion), sValue);
						} else if ( strcmp(sField, "abiVersion") == 0 ) {
							__xlXleCopyText(dep.sAbiVersion, sizeof(dep.sAbiVersion), sValue);
						} else if ( strcmp(sField, "abiId") == 0 ) {
							__xlXleCopyText(dep.sAbiId, sizeof(dep.sAbiId), sValue);
						}
						free(sValue);
					} else if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
						free(sField);
						free(sKey);
						free(pDeps);
						return XL_RC_FAIL;
					}
					free(sField);
					__xlXleSkipWs(&cur);
					if ( *cur.pCur == ',' ) {
						cur.pCur += 1;
						continue;
					}
				}

				if ( dep.sName[0] != '\0' ) {
					XL_XleDependencyInfo* pNewDeps;

					if ( iDepCount >= iDepCap ) {
						iDepCap = (iDepCap == 0) ? 4 : (iDepCap * 2);
						pNewDeps = (XL_XleDependencyInfo*)realloc(pDeps, sizeof(*pDeps) * iDepCap);
						if ( pNewDeps == NULL ) {
							free(sKey);
							free(pDeps);
							return XL_RC_IO_ERROR;
						}
						pDeps = pNewDeps;
					}
					pDeps[iDepCount++] = dep;
				}

				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
			}
			break;
		}

		if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			free(pDeps);
			return XL_RC_FAIL;
		}
		free(sKey);
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}

	free(sKey);
	*ppDeps = pDeps;
	*piDepCount = iDepCount;
	return XL_RC_OK;
}

static int __xlXleParseExports(const char* sJson, const char* sModuleKey, XL_XleExportInfo** ppExports, size_t* piExportCount)
{
	XL_XleJsonCursor cur;
	char* sKey;
	XL_XleExportInfo* pExports;
	size_t iExportCount;

	if ( (sJson == NULL) || (ppExports == NULL) || (piExportCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppExports = NULL;
	*piExportCount = 0;
	pExports = NULL;
	iExportCount = 0;
	sKey = NULL;

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXleSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			break;
		}
		if ( __xlXleParseString(&cur, &sKey) != XL_RC_OK ) {
			xlXleExportInfoFree(pExports);
			return XL_RC_FAIL;
		}
		__xlXleSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			xlXleExportInfoFree(pExports);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "modules") == 0 ) {
			__xlXleSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				xlXleExportInfoFree(pExports);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;

			while ( cur.pCur < cur.pEnd ) {
				char sModuleName[XL_PATH_MAX_LEN];
				char sModuleAbiId[XL_PATH_MAX_LEN];

				memset(sModuleName, 0, sizeof(sModuleName));
				memset(sModuleAbiId, 0, sizeof(sModuleAbiId));
				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ']' ) {
					cur.pCur += 1;
					break;
				}
				if ( *cur.pCur != '{' ) {
					free(sKey);
					xlXleExportInfoFree(pExports);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;

				while ( cur.pCur < cur.pEnd ) {
					char* sField;
					char* sValue;

					__xlXleSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}

					sField = NULL;
					if ( __xlXleParseString(&cur, &sField) != XL_RC_OK ) {
						free(sKey);
						xlXleExportInfoFree(pExports);
						return XL_RC_FAIL;
					}
					__xlXleSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sKey);
						xlXleExportInfoFree(pExports);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;

					sValue = NULL;
					if ( __xlXleParseString(&cur, &sValue) == XL_RC_OK ) {
						if ( strcmp(sField, "key") == 0 ) {
							__xlXleCopyText(sModuleName, sizeof(sModuleName), sValue);
						} else if ( strcmp(sField, "abiId") == 0 ) {
							__xlXleCopyText(sModuleAbiId, sizeof(sModuleAbiId), sValue);
						}
						free(sValue);
					} else if ( strcmp(sField, "exports") == 0 ) {
						if ( __xlXleParseModuleExports(&cur, sModuleKey, sModuleName, sModuleAbiId, &pExports, &iExportCount) != XL_RC_OK ) {
							free(sField);
							free(sKey);
							xlXleExportInfoFree(pExports);
							return XL_RC_FAIL;
						}
					} else if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
						free(sField);
						free(sKey);
						xlXleExportInfoFree(pExports);
						return XL_RC_FAIL;
					}

					free(sField);
					__xlXleSkipWs(&cur);
					if ( *cur.pCur == ',' ) {
						cur.pCur += 1;
						continue;
					}
				}

				__xlXleSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
			}
		} else if ( __xlXleSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			xlXleExportInfoFree(pExports);
			return XL_RC_FAIL;
		}

		free(sKey);
		sKey = NULL;
		__xlXleSkipWs(&cur);
		if ( *cur.pCur == ',' ) {
			cur.pCur += 1;
			continue;
		}
	}

	*ppExports = pExports;
	*piExportCount = iExportCount;
	return XL_RC_OK;
}

int xlXleReadDependencies(
	const char* sManifestPath,
	XL_XleDependencyInfo** ppDeps,
	size_t* piDepCount
)
{
	char* sJson;
	int iRc;

	if ( (sManifestPath == NULL) || (ppDeps == NULL) || (piDepCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppDeps = NULL;
	*piDepCount = 0;
	iRc = __xlXleReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = __xlXleParseDependencies(sJson, ppDeps, piDepCount);
	free(sJson);
	return iRc;
}

int xlXleReadFunctions(
	const char* sManifestPath,
	const char* sModuleKey,
	XL_XleFunctionInfo** ppFuncs,
	size_t* piFuncCount
)
{
	char* sJson;
	int iRc;

	if ( (sManifestPath == NULL) || (ppFuncs == NULL) || (piFuncCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppFuncs = NULL;
	*piFuncCount = 0;
	iRc = __xlXleReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = __xlXleParseFunctions(sJson, sModuleKey, ppFuncs, piFuncCount);
	free(sJson);
	return iRc;
}

int xlXleReadExports(
	const char* sManifestPath,
	const char* sModuleKey,
	XL_XleExportInfo** ppExports,
	size_t* piExportCount
)
{
	char* sJson;
	int iRc;

	if ( (sManifestPath == NULL) || (ppExports == NULL) || (piExportCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppExports = NULL;
	*piExportCount = 0;
	sJson = NULL;
	iRc = __xlXleReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	iRc = __xlXleParseExports(sJson, sModuleKey, ppExports, piExportCount);
	free(sJson);
	return iRc;
}
