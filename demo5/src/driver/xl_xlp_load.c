#include "xl_xlp_load.h"

#include "../common/xl_path.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif

typedef struct XL_XlpJsonCursor {
	const char* pCur;
	const char* pEnd;
} XL_XlpJsonCursor;

static int __xlXlpResolveModuleFromManifest(const char* sManifestPath, const char* sImportKey, XL_XlpResolveInfo* pInfo);
static int __xlXlpFindDependencyVersion(const char* sManifestPath, const char* sImportKey, char* sOutVersion, size_t iOutVersionSize);
static int __xlXlpParseDependencies(const char* sJson, XL_XlpDependencyInfo** ppDeps, size_t* piDepCount);
static int __xlXlpParseFunctions(const char* sJson, const char* sModuleKey, XL_XlpFunctionInfo** ppFuncs, size_t* piFuncCount);
static int __xlXlpParseArtifactSection(const char* sJson, XL_XlpResolveInfo* pInfo);
static int __xlXlpFinalizeArtifactInfo(XL_XlpResolveInfo* pInfo, const char* sManifestPath, const char* sModuleFile, int bIsNative);

void xlXlpResolveInfoInit(XL_XlpResolveInfo* pInfo)
{
	if ( pInfo != NULL ) {
		memset(pInfo, 0, sizeof(*pInfo));
	}
}

static int __xlXlpFileExists(const char* sFilePath)
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

static int __xlXlpReadFileText(const char* sFilePath, char** psText)
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

static void __xlXlpSkipWs(XL_XlpJsonCursor* pCur)
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

static int __xlXlpMatchLiteral(XL_XlpJsonCursor* pCur, const char* sText)
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

static int __xlXlpParseString(XL_XlpJsonCursor* pCur, char** psText)
{
	char* sText;
	size_t iCap;
	size_t iLen;

	if ( (pCur == NULL) || (psText == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	*psText = NULL;

	__xlXlpSkipWs(pCur);
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

static int __xlXlpParseBoolLike(XL_XlpJsonCursor* pCur, int* pbValue)
{
	const char* pStart;
	char sNumber[16];
	size_t iLen;

	if ( (pCur == NULL) || (pbValue == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlXlpSkipWs(pCur);
	if ( __xlXlpMatchLiteral(pCur, "true") ) {
		*pbValue = 1;
		return XL_RC_OK;
	}
	if ( __xlXlpMatchLiteral(pCur, "false") ) {
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

static int __xlXlpSkipValue(XL_XlpJsonCursor* pCur);

static int __xlXlpSkipArray(XL_XlpJsonCursor* pCur)
{
	__xlXlpSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '[') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;
	__xlXlpSkipWs(pCur);
	if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == ']') ) {
		pCur->pCur += 1;
		return XL_RC_OK;
	}

	while ( pCur->pCur < pCur->pEnd ) {
		if ( __xlXlpSkipValue(pCur) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXlpSkipWs(pCur);
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

static int __xlXlpSkipObject(XL_XlpJsonCursor* pCur)
{
	char* sKey;

	__xlXlpSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '{') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;
	__xlXlpSkipWs(pCur);
	if ( (pCur->pCur < pCur->pEnd) && (*pCur->pCur == '}') ) {
		pCur->pCur += 1;
		return XL_RC_OK;
	}

	while ( pCur->pCur < pCur->pEnd ) {
		sKey = NULL;
		if ( __xlXlpParseString(pCur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXlpSkipWs(pCur);
		if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != ':') ) {
			return XL_RC_FAIL;
		}
		pCur->pCur += 1;
		if ( __xlXlpSkipValue(pCur) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXlpSkipWs(pCur);
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

static int __xlXlpSkipValue(XL_XlpJsonCursor* pCur)
{
	char* sText;

	__xlXlpSkipWs(pCur);
	if ( pCur->pCur >= pCur->pEnd ) {
		return XL_RC_FAIL;
	}

	switch ( *pCur->pCur ) {
		case '"':
			sText = NULL;
			if ( __xlXlpParseString(pCur, &sText) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
			free(sText);
			return XL_RC_OK;
		case '{':
			return __xlXlpSkipObject(pCur);
		case '[':
			return __xlXlpSkipArray(pCur);
		case 't':
			return __xlXlpMatchLiteral(pCur, "true") ? XL_RC_OK : XL_RC_FAIL;
		case 'f':
			return __xlXlpMatchLiteral(pCur, "false") ? XL_RC_OK : XL_RC_FAIL;
		case 'n':
			return __xlXlpMatchLiteral(pCur, "null") ? XL_RC_OK : XL_RC_FAIL;
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

static int __xlXlpPathIsAbsolute(const char* sPath)
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

static void __xlXlpStripFileName(char* sPath)
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

static void __xlXlpNormalizePath(char* sOut, size_t iOutSize, const char* sPath)
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

static void __xlXlpCopyText(char* sOut, size_t iOutSize, const char* sText)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sText == NULL ) {
		sText = "";
	}
	snprintf(sOut, iOutSize, "%s", sText);
}

static int __xlXlpParsePackageSection(const char* sJson, XL_XlpResolveInfo* pInfo)
{
	XL_XlpJsonCursor cur;
	char* sKey;

	if ( (sJson == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXlpSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			return XL_RC_OK;
		}
		if ( __xlXlpParseString(&cur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "package") == 0 ) {
			char* sField;

			__xlXlpSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
				free(sKey);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			while ( cur.pCur < cur.pEnd ) {
				char* sValue;

				__xlXlpSkipWs(&cur);
				if ( *cur.pCur == '}' ) {
					free(sKey);
					return XL_RC_OK;
				}
				sField = NULL;
				if ( __xlXlpParseString(&cur, &sField) != XL_RC_OK ) {
					free(sKey);
					return XL_RC_FAIL;
				}
				__xlXlpSkipWs(&cur);
				if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;

				sValue = NULL;
				if ( __xlXlpParseString(&cur, &sValue) == XL_RC_OK ) {
					if ( strcmp(sField, "name") == 0 ) {
						__xlXlpCopyText(pInfo->sPackageName, sizeof(pInfo->sPackageName), sValue);
					} else if ( strcmp(sField, "version") == 0 ) {
						__xlXlpCopyText(pInfo->sPackageVersion, sizeof(pInfo->sPackageVersion), sValue);
					} else if ( strcmp(sField, "abiVersion") == 0 ) {
						__xlXlpCopyText(pInfo->sPackageAbiVersion, sizeof(pInfo->sPackageAbiVersion), sValue);
					} else if ( strcmp(sField, "abiId") == 0 ) {
						__xlXlpCopyText(pInfo->sPackageAbiId, sizeof(pInfo->sPackageAbiId), sValue);
					}
					free(sValue);
				} else if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}

				free(sField);
				__xlXlpSkipWs(&cur);
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

		if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}

	return XL_RC_OK;
}

static int __xlXlpParseArtifactSection(const char* sJson, XL_XlpResolveInfo* pInfo)
{
	XL_XlpJsonCursor cur;
	char* sKey;

	if ( (sJson == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXlpSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			return XL_RC_OK;
		}
		if ( __xlXlpParseString(&cur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "artifact") == 0 ) {
			__xlXlpSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
				free(sKey);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			while ( cur.pCur < cur.pEnd ) {
				char* sField;
				char* sValue;

				__xlXlpSkipWs(&cur);
				if ( *cur.pCur == '}' ) {
					free(sKey);
					return XL_RC_OK;
				}
				sField = NULL;
				if ( __xlXlpParseString(&cur, &sField) != XL_RC_OK ) {
					free(sKey);
					return XL_RC_FAIL;
				}
				__xlXlpSkipWs(&cur);
				if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;
				sValue = NULL;
				if ( __xlXlpParseString(&cur, &sValue) == XL_RC_OK ) {
					if ( strcmp(sField, "kind") == 0 ) {
						__xlXlpCopyText(pInfo->sArtifactKind, sizeof(pInfo->sArtifactKind), sValue);
					} else if ( strcmp(sField, "path") == 0 ) {
						__xlXlpCopyText(pInfo->sArtifactPath, sizeof(pInfo->sArtifactPath), sValue);
					}
					free(sValue);
				} else if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
					free(sField);
					free(sKey);
					return XL_RC_FAIL;
				}
				free(sField);
				__xlXlpSkipWs(&cur);
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

		if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}

	return XL_RC_OK;
}

static int __xlXlpFindDependencyVersionInJson(const char* sJson, const char* sImportKey, char* sOutVersion, size_t iOutVersionSize)
{
	XL_XlpJsonCursor cur;
	char* sKey;

	if ( (sJson == NULL) || (sImportKey == NULL) || (sOutVersion == NULL) || (iOutVersionSize == 0) ) {
		return XL_RC_BAD_ARG;
	}

	sOutVersion[0] = '\0';
	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXlpSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			return XL_RC_FAIL;
		}
		if ( __xlXlpParseString(&cur, &sKey) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "dependencies") == 0 ) {
			__xlXlpSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			__xlXlpSkipWs(&cur);
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

					__xlXlpSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}
					sField = NULL;
					if ( __xlXlpParseString(&cur, &sField) != XL_RC_OK ) {
						free(sDepName);
						free(sDepVersion);
						free(sKey);
						return XL_RC_FAIL;
					}
					__xlXlpSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sDepName);
						free(sDepVersion);
						free(sKey);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;
					sValue = NULL;
					if ( __xlXlpParseString(&cur, &sValue) == XL_RC_OK ) {
						if ( strcmp(sField, "name") == 0 ) {
							free(sDepName);
							sDepName = sValue;
							sValue = NULL;
						} else if ( strcmp(sField, "version") == 0 ) {
							free(sDepVersion);
							sDepVersion = sValue;
							sValue = NULL;
						}
					} else if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
						free(sField);
						free(sValue);
						free(sDepName);
						free(sDepVersion);
						free(sKey);
						return XL_RC_FAIL;
					}
					free(sValue);
					free(sField);
					__xlXlpSkipWs(&cur);
					if ( *cur.pCur == ',' ) {
						cur.pCur += 1;
						continue;
					}
				}

				if ( (sDepName != NULL) && (strcmp(sDepName, sImportKey) == 0) && (sDepVersion != NULL) && (sDepVersion[0] != '\0') ) {
					__xlXlpCopyText(sOutVersion, iOutVersionSize, sDepVersion);
					free(sDepName);
					free(sDepVersion);
					free(sKey);
					return XL_RC_OK;
				}

				free(sDepName);
				free(sDepVersion);
				__xlXlpSkipWs(&cur);
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

		if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			return XL_RC_FAIL;
		}
		free(sKey);
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == ',') ) {
			cur.pCur += 1;
			continue;
		}
	}

	return XL_RC_FAIL;
}

static int __xlXlpFindDependencyVersion(const char* sManifestPath, const char* sImportKey, char* sOutVersion, size_t iOutVersionSize)
{
	char* sJson;
	int iRc;

	if ( (sManifestPath == NULL) || (sImportKey == NULL) || (sOutVersion == NULL) || (iOutVersionSize == 0) ) {
		return XL_RC_BAD_ARG;
	}

	iRc = __xlXlpReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	iRc = __xlXlpFindDependencyVersionInJson(sJson, sImportKey, sOutVersion, iOutVersionSize);
	free(sJson);
	return iRc;
}

static void __xlXlpResolveManifestRelativePath(char* sOut, size_t iOutSize, const char* sManifestPath, const char* sValue)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	sOut[0] = '\0';
	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		return;
	}
	if ( __xlXlpPathIsAbsolute(sValue) ) {
		__xlXlpNormalizePath(sOut, iOutSize, sValue);
		return;
	}
	if ( (sManifestPath != NULL) && (sManifestPath[0] != '\0') ) {
		char sBaseDir[XL_PATH_MAX_LEN];
		char sJoined[XL_PATH_MAX_LEN];

		snprintf(sBaseDir, sizeof(sBaseDir), "%s", sManifestPath);
		__xlXlpStripFileName(sBaseDir);
		xlPathJoin(sJoined, sizeof(sJoined), sBaseDir, sValue);
		__xlXlpNormalizePath(sOut, iOutSize, sJoined);
		return;
	}
	__xlXlpNormalizePath(sOut, iOutSize, sValue);
}

static int __xlXlpFinalizeArtifactInfo(XL_XlpResolveInfo* pInfo, const char* sManifestPath, const char* sModuleFile, int bIsNative)
{
	if ( pInfo == NULL ) {
		return XL_RC_BAD_ARG;
	}

	if ( pInfo->sArtifactKind[0] == '\0' ) {
		__xlXlpCopyText(pInfo->sArtifactKind, sizeof(pInfo->sArtifactKind), bIsNative ? "native" : "source");
	}
	if ( (pInfo->sArtifactPath[0] == '\0') && (sModuleFile != NULL) && (sModuleFile[0] != '\0') ) {
		__xlXlpCopyText(pInfo->sArtifactPath, sizeof(pInfo->sArtifactPath), sModuleFile);
	}

	if ( bIsNative ) {
		if ( strcmp(pInfo->sArtifactKind, "source") == 0 ) {
			return XL_RC_FAIL;
		}
		if ( (strcmp(pInfo->sArtifactKind, "native") != 0) &&
			(strcmp(pInfo->sArtifactKind, "shared") != 0) &&
			(strcmp(pInfo->sArtifactKind, "static") != 0) ) {
			return XL_RC_FAIL;
		}
		if ( pInfo->sBindingName[0] == '\0' ) {
			return XL_RC_FAIL;
		}
		if ( strcmp(pInfo->sArtifactKind, "native") == 0 ) {
			__xlXlpCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "native-registry");
			__xlXlpResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
		} else if ( strcmp(pInfo->sArtifactKind, "shared") == 0 ) {
			__xlXlpCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "shared-bridge");
			__xlXlpResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
		} else {
			__xlXlpCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "static-bridge");
			__xlXlpResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
		}
	} else {
		if ( strcmp(pInfo->sArtifactKind, "source") != 0 ) {
			return XL_RC_FAIL;
		}
		__xlXlpCopyText(pInfo->sConsumeMode, sizeof(pInfo->sConsumeMode), "source-parse");
		__xlXlpResolveManifestRelativePath(pInfo->sArtifactFile, sizeof(pInfo->sArtifactFile), sManifestPath, pInfo->sArtifactPath);
	}

	return XL_RC_OK;
}

static int __xlXlpTryResolveFromSearchRoot(const char* sSearchRoot, const char* sImportKey, const char* sVersion, XL_XlpResolveInfo* pInfo)
{
	char sSearchDir[XL_PATH_MAX_LEN];

	if ( (sSearchRoot == NULL) || (sImportKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	snprintf(sSearchDir, sizeof(sSearchDir), "%s", sSearchRoot);
	while ( sSearchDir[0] != '\0' ) {
		char sCandidateDir[XL_PATH_MAX_LEN];
		char sCandidateFile[XL_PATH_MAX_LEN];
		char sPackageDir[XL_PATH_MAX_LEN];
		char sPrevDir[XL_PATH_MAX_LEN];

		snprintf(sPrevDir, sizeof(sPrevDir), "%s", sSearchDir);
		xlPathJoin(sPackageDir, sizeof(sPackageDir), sSearchDir, "packages");
		if ( (sVersion != NULL) && (sVersion[0] != '\0') ) {
			snprintf(sCandidateDir, sizeof(sCandidateDir), "%s@%s", sImportKey, sVersion);
			xlPathJoin(sCandidateFile, sizeof(sCandidateFile), sPackageDir, sCandidateDir);
			xlPathJoin(sCandidateDir, sizeof(sCandidateDir), sCandidateFile, "xlp.json");
			if ( __xlXlpFileExists(sCandidateDir) ) {
				return __xlXlpResolveModuleFromManifest(sCandidateDir, sImportKey, pInfo);
			}

			xlPathJoin(sCandidateFile, sizeof(sCandidateFile), sPackageDir, sImportKey);
			snprintf(sCandidateDir, sizeof(sCandidateDir), "%s", sCandidateFile);
			if ( strlen(sCandidateDir) < (sizeof(sCandidateDir) - 1) ) {
				strncat(sCandidateDir, "@", sizeof(sCandidateDir) - strlen(sCandidateDir) - 1);
			}
			strncat(sCandidateDir, sVersion, sizeof(sCandidateDir) - strlen(sCandidateDir) - 1);
			strncat(sCandidateDir, ".xlp.json", sizeof(sCandidateDir) - strlen(sCandidateDir) - 1);
			if ( __xlXlpFileExists(sCandidateDir) ) {
				return __xlXlpResolveModuleFromManifest(sCandidateDir, sImportKey, pInfo);
			}
		}

		xlPathJoin(sCandidateDir, sizeof(sCandidateDir), sPackageDir, sImportKey);
		xlPathJoin(sCandidateFile, sizeof(sCandidateFile), sCandidateDir, "xlp.json");
		if ( __xlXlpFileExists(sCandidateFile) ) {
			return __xlXlpResolveModuleFromManifest(sCandidateFile, sImportKey, pInfo);
		}

		xlPathJoin(sCandidateFile, sizeof(sCandidateFile), sPackageDir, sImportKey);
		snprintf(sCandidateDir, sizeof(sCandidateDir), "%s", sCandidateFile);
		strncat(sCandidateDir, ".xlp.json", sizeof(sCandidateDir) - strlen(sCandidateDir) - 1);
		if ( __xlXlpFileExists(sCandidateDir) ) {
			return __xlXlpResolveModuleFromManifest(sCandidateDir, sImportKey, pInfo);
		}

		__xlXlpStripFileName(sSearchDir);
		if ( strcmp(sPrevDir, sSearchDir) == 0 ) {
			break;
		}
	}

	return XL_RC_FAIL;
}

static int __xlXlpResolveModuleFromManifest(
	const char* sManifestPath,
	const char* sImportKey,
	XL_XlpResolveInfo* pInfo
)
{
	char* sJson;
	XL_XlpJsonCursor cur;
	int iRc;
	char* sKey;

	if ( (sManifestPath == NULL) || (sImportKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlXlpResolveInfoInit(pInfo);
	__xlXlpNormalizePath(pInfo->sManifestPath, sizeof(pInfo->sManifestPath), sManifestPath);
	iRc = __xlXlpReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	__xlXlpParsePackageSection(sJson, pInfo);
	__xlXlpParseArtifactSection(sJson, pInfo);

	cur.pCur = sJson;
	cur.pEnd = sJson + strlen(sJson);
	__xlXlpSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		free(sJson);
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			break;
		}
		if ( __xlXlpParseString(&cur, &sKey) != XL_RC_OK ) {
			free(sJson);
			return XL_RC_FAIL;
		}
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			free(sJson);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "modules") == 0 ) {
			__xlXlpSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				free(sJson);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			__xlXlpSkipWs(&cur);
			while ( cur.pCur < cur.pEnd ) {
				char* sModuleKey;
				char* sModuleFile;
				char* sBindingName;
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
				bIsNative = 0;
				while ( cur.pCur < cur.pEnd ) {
					char* sField;

					__xlXlpSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}
					sField = NULL;
					if ( __xlXlpParseString(&cur, &sField) != XL_RC_OK ) {
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sKey);
						free(sJson);
						return XL_RC_FAIL;
					}
					__xlXlpSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sKey);
						free(sJson);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;
					if ( strcmp(sField, "key") == 0 ) {
						free(sModuleKey);
						sModuleKey = NULL;
						iRc = __xlXlpParseString(&cur, &sModuleKey);
					} else if ( strcmp(sField, "file") == 0 ) {
						free(sModuleFile);
						sModuleFile = NULL;
						iRc = __xlXlpParseString(&cur, &sModuleFile);
					} else if ( strcmp(sField, "binding") == 0 ) {
						free(sBindingName);
						sBindingName = NULL;
						iRc = __xlXlpParseString(&cur, &sBindingName);
					} else if ( strcmp(sField, "native") == 0 ) {
						__xlXlpSkipWs(&cur);
						if ( __xlXlpMatchLiteral(&cur, "true") || __xlXlpMatchLiteral(&cur, "1") ) {
							bIsNative = 1;
							iRc = XL_RC_OK;
						} else if ( __xlXlpMatchLiteral(&cur, "false") || __xlXlpMatchLiteral(&cur, "0") ) {
							bIsNative = 0;
							iRc = XL_RC_OK;
						} else {
							iRc = XL_RC_FAIL;
						}
					} else {
						iRc = __xlXlpSkipValue(&cur);
					}
					free(sField);
					if ( iRc != XL_RC_OK ) {
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sKey);
						free(sJson);
						return iRc;
					}
					__xlXlpSkipWs(&cur);
					if ( *cur.pCur == ',' ) {
						cur.pCur += 1;
						continue;
					}
				}

				if ( (sModuleKey != NULL) && (strcmp(sModuleKey, sImportKey) == 0) ) {
					if ( bIsNative ) {
						__xlXlpCopyText(pInfo->sModuleFile, sizeof(pInfo->sModuleFile), (sModuleFile != NULL) ? sModuleFile : "");
						__xlXlpCopyText(pInfo->sBindingName, sizeof(pInfo->sBindingName), (sBindingName != NULL) ? sBindingName : sImportKey);
						iRc = __xlXlpFinalizeArtifactInfo(pInfo, sManifestPath, sModuleFile, 1);
						if ( iRc != XL_RC_OK ) {
							free(sModuleKey);
							free(sModuleFile);
							free(sBindingName);
							free(sKey);
							free(sJson);
							return iRc;
						}
						pInfo->bIsNative = 1;
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sKey);
						free(sJson);
						return XL_RC_OK;
					}
					if ( (sModuleFile == NULL) || (sModuleFile[0] == '\0') ) {
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sKey);
						free(sJson);
						return XL_RC_FAIL;
					}
					if ( __xlXlpPathIsAbsolute(sModuleFile) ) {
						__xlXlpNormalizePath(pInfo->sModuleFile, sizeof(pInfo->sModuleFile), sModuleFile);
					} else {
						char sBaseDir[XL_PATH_MAX_LEN];
						char sJoined[XL_PATH_MAX_LEN];

						snprintf(sBaseDir, sizeof(sBaseDir), "%s", sManifestPath);
						__xlXlpStripFileName(sBaseDir);
						xlPathJoin(sJoined, sizeof(sJoined), sBaseDir, sModuleFile);
						__xlXlpNormalizePath(pInfo->sModuleFile, sizeof(pInfo->sModuleFile), sJoined);
					}
					iRc = __xlXlpFinalizeArtifactInfo(pInfo, sManifestPath, sModuleFile, 0);
					if ( iRc != XL_RC_OK ) {
						free(sModuleKey);
						free(sModuleFile);
						free(sBindingName);
						free(sKey);
						free(sJson);
						return iRc;
					}
					__xlXlpCopyText(pInfo->sBindingName, sizeof(pInfo->sBindingName), (sBindingName != NULL) ? sBindingName : "");
					pInfo->bIsNative = 0;
					free(sModuleKey);
					free(sModuleFile);
					free(sBindingName);
					free(sKey);
					free(sJson);
					return XL_RC_OK;
				}

				free(sModuleKey);
				free(sModuleFile);
				free(sBindingName);
				__xlXlpSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
			}
		} else {
			iRc = __xlXlpSkipValue(&cur);
			if ( iRc != XL_RC_OK ) {
				free(sKey);
				free(sJson);
				return iRc;
			}
		}

		free(sKey);
		__xlXlpSkipWs(&cur);
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

int xlXlpFindModuleSource(
	const char* sCurrentFile,
	const char* sCurrentManifestPath,
	const char* sImportKey,
	XL_XlpResolveInfo* pInfo
)
{
	char sSearchDir[XL_PATH_MAX_LEN];
	char sVersion[64];

	if ( (sCurrentFile == NULL) || (sImportKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlXlpResolveInfoInit(pInfo);
	sVersion[0] = '\0';
	if ( (sCurrentManifestPath != NULL) && (sCurrentManifestPath[0] != '\0') &&
		(__xlXlpFindDependencyVersion(sCurrentManifestPath, sImportKey, sVersion, sizeof(sVersion)) == XL_RC_OK) ) {
		snprintf(sSearchDir, sizeof(sSearchDir), "%s", sCurrentManifestPath);
		__xlXlpStripFileName(sSearchDir);
		if ( __xlXlpTryResolveFromSearchRoot(sSearchDir, sImportKey, sVersion, pInfo) == XL_RC_OK ) {
			return XL_RC_OK;
		}
	}
	snprintf(sSearchDir, sizeof(sSearchDir), "%s", sCurrentFile);
	__xlXlpStripFileName(sSearchDir);
	return __xlXlpTryResolveFromSearchRoot(sSearchDir, sImportKey, NULL, pInfo);
}

int xlXlpFindDependencyVersion(
	const char* sManifestPath,
	const char* sImportKey,
	char* sOutVersion,
	size_t iOutVersionSize
)
{
	return __xlXlpFindDependencyVersion(sManifestPath, sImportKey, sOutVersion, iOutVersionSize);
}

void xlXlpDependencyInfoFree(XL_XlpDependencyInfo* pDeps)
{
	free(pDeps);
}

static void __xlXlpFunctionInfoReset(XL_XlpFunctionInfo* pFunc)
{
	if ( pFunc == NULL ) {
		return;
	}
	free(pFunc->pParams);
	memset(pFunc, 0, sizeof(*pFunc));
}

static int __xlXlpFunctionInfoAddParam(XL_XlpFunctionInfo* pFunc, const XL_XlpParamInfo* pParam)
{
	XL_XlpParamInfo* pNewParams;
	size_t iNewCount;

	if ( (pFunc == NULL) || (pParam == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iNewCount = pFunc->iParamCount + 1;
	pNewParams = (XL_XlpParamInfo*)realloc(pFunc->pParams, sizeof(*pFunc->pParams) * iNewCount);
	if ( pNewParams == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pFunc->pParams = pNewParams;
	pFunc->pParams[pFunc->iParamCount] = *pParam;
	pFunc->iParamCount = iNewCount;
	return XL_RC_OK;
}

void xlXlpFunctionInfoFree(XL_XlpFunctionInfo* pFuncs, size_t iFuncCount)
{
	size_t i;

	if ( pFuncs == NULL ) {
		return;
	}

	for ( i = 0; i < iFuncCount; ++i ) {
		free(pFuncs[i].pParams);
	}
	free(pFuncs);
}

static int __xlXlpParseParams(XL_XlpJsonCursor* pCur, XL_XlpFunctionInfo* pFunc)
{
	if ( (pCur == NULL) || (pFunc == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	__xlXlpSkipWs(pCur);
	if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != '[') ) {
		return XL_RC_FAIL;
	}
	pCur->pCur += 1;

	while ( pCur->pCur < pCur->pEnd ) {
		XL_XlpParamInfo param;

		memset(&param, 0, sizeof(param));
		__xlXlpSkipWs(pCur);
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

			__xlXlpSkipWs(pCur);
			if ( *pCur->pCur == '}' ) {
				pCur->pCur += 1;
				break;
			}
			sField = NULL;
			if ( __xlXlpParseString(pCur, &sField) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
			__xlXlpSkipWs(pCur);
			if ( (pCur->pCur >= pCur->pEnd) || (*pCur->pCur != ':') ) {
				free(sField);
				return XL_RC_FAIL;
			}
			pCur->pCur += 1;
			sValue = NULL;
			if ( __xlXlpParseString(pCur, &sValue) == XL_RC_OK ) {
				if ( strcmp(sField, "name") == 0 ) {
					__xlXlpCopyText(param.sName, sizeof(param.sName), sValue);
				} else if ( strcmp(sField, "label") == 0 ) {
					__xlXlpCopyText(param.sLabel, sizeof(param.sLabel), sValue);
				} else if ( strcmp(sField, "type") == 0 ) {
					__xlXlpCopyText(param.sType, sizeof(param.sType), sValue);
				}
				free(sValue);
			} else if ( strcmp(sField, "hasDefault") == 0 ) {
				if ( __xlXlpParseBoolLike(pCur, &param.bHasDefault) != XL_RC_OK ) {
					free(sField);
					return XL_RC_FAIL;
				}
			} else if ( __xlXlpSkipValue(pCur) != XL_RC_OK ) {
				free(sField);
				return XL_RC_FAIL;
			}
			free(sField);
			__xlXlpSkipWs(pCur);
			if ( *pCur->pCur == ',' ) {
				pCur->pCur += 1;
				continue;
			}
		}

		if ( __xlXlpFunctionInfoAddParam(pFunc, &param) != XL_RC_OK ) {
			return XL_RC_IO_ERROR;
		}

		__xlXlpSkipWs(pCur);
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

static int __xlXlpParseFunctions(const char* sJson, const char* sModuleKey, XL_XlpFunctionInfo** ppFuncs, size_t* piFuncCount)
{
	XL_XlpJsonCursor cur;
	char* sKey;
	XL_XlpFunctionInfo* pFuncs;
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
	__xlXlpSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			break;
		}
		if ( __xlXlpParseString(&cur, &sKey) != XL_RC_OK ) {
			xlXlpFunctionInfoFree(pFuncs, iFuncCount);
			return XL_RC_FAIL;
		}
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			xlXlpFunctionInfoFree(pFuncs, iFuncCount);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "functions") == 0 ) {
			__xlXlpSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				xlXlpFunctionInfoFree(pFuncs, iFuncCount);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;

			while ( cur.pCur < cur.pEnd ) {
				XL_XlpFunctionInfo func;
				int bKeepFunc;

				memset(&func, 0, sizeof(func));
				bKeepFunc = 1;
				__xlXlpSkipWs(&cur);
				if ( *cur.pCur == ']' ) {
					cur.pCur += 1;
					break;
				}
				if ( *cur.pCur != '{' ) {
					free(sKey);
					xlXlpFunctionInfoFree(pFuncs, iFuncCount);
					return XL_RC_FAIL;
				}
				cur.pCur += 1;

				while ( cur.pCur < cur.pEnd ) {
					char* sField;
					char* sValue;

					__xlXlpSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}
					sField = NULL;
					if ( __xlXlpParseString(&cur, &sField) != XL_RC_OK ) {
						free(sKey);
						__xlXlpFunctionInfoReset(&func);
						xlXlpFunctionInfoFree(pFuncs, iFuncCount);
						return XL_RC_FAIL;
					}
					__xlXlpSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sKey);
						__xlXlpFunctionInfoReset(&func);
						xlXlpFunctionInfoFree(pFuncs, iFuncCount);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;
					sValue = NULL;
					if ( __xlXlpParseString(&cur, &sValue) == XL_RC_OK ) {
						if ( strcmp(sField, "module") == 0 ) {
							__xlXlpCopyText(func.sModule, sizeof(func.sModule), sValue);
						} else if ( strcmp(sField, "moduleAbiId") == 0 ) {
							__xlXlpCopyText(func.sModuleAbiId, sizeof(func.sModuleAbiId), sValue);
						} else if ( strcmp(sField, "exportName") == 0 ) {
							__xlXlpCopyText(func.sExportName, sizeof(func.sExportName), sValue);
						} else if ( strcmp(sField, "symbol") == 0 ) {
							__xlXlpCopyText(func.sSymbol, sizeof(func.sSymbol), sValue);
						} else if ( strcmp(sField, "overloadGroup") == 0 ) {
							__xlXlpCopyText(func.sOverloadGroup, sizeof(func.sOverloadGroup), sValue);
						} else if ( strcmp(sField, "signatureId") == 0 ) {
							__xlXlpCopyText(func.sSignatureId, sizeof(func.sSignatureId), sValue);
						} else if ( strcmp(sField, "dispatchKey") == 0 ) {
							__xlXlpCopyText(func.sDispatchKey, sizeof(func.sDispatchKey), sValue);
						} else if ( strcmp(sField, "abiSymbol") == 0 ) {
							__xlXlpCopyText(func.sAbiSymbol, sizeof(func.sAbiSymbol), sValue);
						} else if ( strcmp(sField, "returnType") == 0 ) {
							__xlXlpCopyText(func.sReturnType, sizeof(func.sReturnType), sValue);
						}
						free(sValue);
					} else if ( strcmp(sField, "params") == 0 ) {
						if ( __xlXlpParseParams(&cur, &func) != XL_RC_OK ) {
							free(sField);
							free(sKey);
							__xlXlpFunctionInfoReset(&func);
							xlXlpFunctionInfoFree(pFuncs, iFuncCount);
							return XL_RC_FAIL;
						}
					} else if ( (strcmp(sField, "async") == 0) || (strcmp(sField, "generator") == 0) ) {
						int bValue;
						if ( __xlXlpParseBoolLike(&cur, &bValue) != XL_RC_OK ) {
							free(sField);
							free(sKey);
							__xlXlpFunctionInfoReset(&func);
							xlXlpFunctionInfoFree(pFuncs, iFuncCount);
							return XL_RC_FAIL;
						}
						if ( strcmp(sField, "async") == 0 ) {
							func.bAsync = bValue;
						} else {
							func.bGenerator = bValue;
						}
					} else if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
						free(sField);
						free(sKey);
						__xlXlpFunctionInfoReset(&func);
						xlXlpFunctionInfoFree(pFuncs, iFuncCount);
						return XL_RC_FAIL;
					}
					free(sField);
					__xlXlpSkipWs(&cur);
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
					XL_XlpFunctionInfo* pNewFuncs;

					pNewFuncs = (XL_XlpFunctionInfo*)realloc(pFuncs, sizeof(*pFuncs) * (iFuncCount + 1));
					if ( pNewFuncs == NULL ) {
						free(sKey);
						__xlXlpFunctionInfoReset(&func);
						xlXlpFunctionInfoFree(pFuncs, iFuncCount);
						return XL_RC_IO_ERROR;
					}
					pFuncs = pNewFuncs;
					pFuncs[iFuncCount++] = func;
					memset(&func, 0, sizeof(func));
				}
				__xlXlpFunctionInfoReset(&func);

				__xlXlpSkipWs(&cur);
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

		if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			xlXlpFunctionInfoFree(pFuncs, iFuncCount);
			return XL_RC_FAIL;
		}
		free(sKey);
		sKey = NULL;
		__xlXlpSkipWs(&cur);
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

static int __xlXlpParseDependencies(const char* sJson, XL_XlpDependencyInfo** ppDeps, size_t* piDepCount)
{
	XL_XlpJsonCursor cur;
	char* sKey;
	XL_XlpDependencyInfo* pDeps;
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
	__xlXlpSkipWs(&cur);
	if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '{') ) {
		return XL_RC_FAIL;
	}
	cur.pCur += 1;

	while ( cur.pCur < cur.pEnd ) {
		sKey = NULL;
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur < cur.pEnd) && (*cur.pCur == '}') ) {
			break;
		}
		if ( __xlXlpParseString(&cur, &sKey) != XL_RC_OK ) {
			free(pDeps);
			return XL_RC_FAIL;
		}
		__xlXlpSkipWs(&cur);
		if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
			free(sKey);
			free(pDeps);
			return XL_RC_FAIL;
		}
		cur.pCur += 1;

		if ( strcmp(sKey, "dependencies") == 0 ) {
			__xlXlpSkipWs(&cur);
			if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != '[') ) {
				free(sKey);
				free(pDeps);
				return XL_RC_FAIL;
			}
			cur.pCur += 1;
			__xlXlpSkipWs(&cur);
			while ( cur.pCur < cur.pEnd ) {
				XL_XlpDependencyInfo dep;

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

					__xlXlpSkipWs(&cur);
					if ( *cur.pCur == '}' ) {
						cur.pCur += 1;
						break;
					}
					sField = NULL;
					if ( __xlXlpParseString(&cur, &sField) != XL_RC_OK ) {
						free(sKey);
						free(pDeps);
						return XL_RC_FAIL;
					}
					__xlXlpSkipWs(&cur);
					if ( (cur.pCur >= cur.pEnd) || (*cur.pCur != ':') ) {
						free(sField);
						free(sKey);
						free(pDeps);
						return XL_RC_FAIL;
					}
					cur.pCur += 1;
					sValue = NULL;
					if ( __xlXlpParseString(&cur, &sValue) == XL_RC_OK ) {
						if ( strcmp(sField, "name") == 0 ) {
							__xlXlpCopyText(dep.sName, sizeof(dep.sName), sValue);
						} else if ( strcmp(sField, "kind") == 0 ) {
							__xlXlpCopyText(dep.sKind, sizeof(dep.sKind), sValue);
						} else if ( strcmp(sField, "version") == 0 ) {
							__xlXlpCopyText(dep.sVersion, sizeof(dep.sVersion), sValue);
						} else if ( strcmp(sField, "abiVersion") == 0 ) {
							__xlXlpCopyText(dep.sAbiVersion, sizeof(dep.sAbiVersion), sValue);
						} else if ( strcmp(sField, "abiId") == 0 ) {
							__xlXlpCopyText(dep.sAbiId, sizeof(dep.sAbiId), sValue);
						}
						free(sValue);
					} else if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
						free(sField);
						free(sKey);
						free(pDeps);
						return XL_RC_FAIL;
					}
					free(sField);
					__xlXlpSkipWs(&cur);
					if ( *cur.pCur == ',' ) {
						cur.pCur += 1;
						continue;
					}
				}

				if ( dep.sName[0] != '\0' ) {
					XL_XlpDependencyInfo* pNewDeps;

					if ( iDepCount >= iDepCap ) {
						iDepCap = (iDepCap == 0) ? 4 : (iDepCap * 2);
						pNewDeps = (XL_XlpDependencyInfo*)realloc(pDeps, sizeof(*pDeps) * iDepCap);
						if ( pNewDeps == NULL ) {
							free(sKey);
							free(pDeps);
							return XL_RC_IO_ERROR;
						}
						pDeps = pNewDeps;
					}
					pDeps[iDepCount++] = dep;
				}

				__xlXlpSkipWs(&cur);
				if ( *cur.pCur == ',' ) {
					cur.pCur += 1;
					continue;
				}
			}
			break;
		}

		if ( __xlXlpSkipValue(&cur) != XL_RC_OK ) {
			free(sKey);
			free(pDeps);
			return XL_RC_FAIL;
		}
		free(sKey);
		sKey = NULL;
		__xlXlpSkipWs(&cur);
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

int xlXlpReadDependencies(
	const char* sManifestPath,
	XL_XlpDependencyInfo** ppDeps,
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
	iRc = __xlXlpReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = __xlXlpParseDependencies(sJson, ppDeps, piDepCount);
	free(sJson);
	return iRc;
}

int xlXlpReadFunctions(
	const char* sManifestPath,
	const char* sModuleKey,
	XL_XlpFunctionInfo** ppFuncs,
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
	iRc = __xlXlpReadFileText(sManifestPath, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = __xlXlpParseFunctions(sJson, sModuleKey, ppFuncs, piFuncCount);
	free(sJson);
	return iRc;
}
