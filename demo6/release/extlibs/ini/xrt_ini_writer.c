#include "xrt_ini_internal.h"

#include <string.h>



static bool __xrtIniOutput(xrtiniconfig* pConfig, xbuffer* pOutput,
	const void* pData, size_t iSize)
{
	if ( iSize == 0u ) { return true; }
	if ( iSize > UINT32_MAX ||
		(pConfig->Options.tLimits.iMaxOutputBytes != 0u &&
		 ((uint64)iSize > pConfig->Options.tLimits.iMaxOutputBytes ||
		  pOutput->Size > pConfig->Options.tLimits.iMaxOutputBytes - (uint64)iSize)) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
			"INI output exceeds the configured limit");
		return false;
	}
	if ( !xrtBufferAppend(pOutput, (xbytesview){ (cbytes)pData, iSize }) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot grow INI output buffer");
		return false;
	}
	return true;
}

static bool __xrtIniOutputText(xrtiniconfig* pConfig, xbuffer* pOutput, const char* sText)
{
	return __xrtIniOutput(pConfig, pOutput, sText, sText != NULL ? strlen(sText) : 0u);
}

/* set() 可保存内存数据；写盘必须保证再次解析后不改变键和值。 */
static bool __xrtIniWritableName(const char* sName, bool bKey)
{
	size_t iLength = strlen(sName);
	const char* p;
	if ( (bKey && iLength == 0u) ||
		(iLength != 0u && (sName[0] == ' ' || sName[0] == '\t' ||
		 sName[iLength - 1u] == ' ' || sName[iLength - 1u] == '\t')) ) {
		return false;
	}
	if ( bKey && (sName[0] == '#' || sName[0] == ';' || sName[0] == '[') ) {
		return false;
	}
	for ( p = sName; *p != 0; ++p ) {
		if ( *p == '\r' || *p == '\n' || (bKey && (*p == '=' || *p == ':')) ) {
			return false;
		}
	}
	return true;
}

static bool __xrtIniWritableValue(const char* sValue)
{
	const char* pLine = sValue;
	bool bFirst = true;
	for ( ;; ) {
		const char* pEnd = strchr(pLine, '\n');
		size_t iLength = pEnd != NULL ? (size_t)(pEnd - pLine) : strlen(pLine);
		if ( memchr(pLine, '\r', iLength) != NULL ||
			(iLength != 0u && (pLine[0] == ' ' || pLine[0] == '\t' ||
			 pLine[iLength - 1u] == ' ' || pLine[iLength - 1u] == '\t')) ||
			(!bFirst && (iLength == 0u || pLine[0] == '#' || pLine[0] == ';')) ) {
			return false;
		}
		if ( pEnd == NULL ) { return true; }
		pLine = pEnd + 1;
		bFirst = false;
	}
}

static bool __xrtIniOutputValue(xrtiniconfig* pConfig, xbuffer* pOutput, const char* sValue)
{
	const char* p = sValue != NULL ? sValue : "";
	const char* pLine = p;
	bool bFirst = true;
	if ( !__xrtIniWritableValue(p) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_FORMAT, 0u, 0u, 0u,
			"INI value cannot be written without changing its data");
		return false;
	}
	for ( ;; ) {
		const char* pEnd = strchr(pLine, '\n');
		size_t iLength = pEnd != NULL ? (size_t)(pEnd - pLine) : strlen(pLine);
		if ( !bFirst && !__xrtIniOutputText(pConfig, pOutput, "    ") ) { return false; }
		if ( !__xrtIniOutput(pConfig, pOutput, pLine, iLength) ||
			!__xrtIniOutputText(pConfig, pOutput, "\n") ) { return false; }
		if ( pEnd == NULL ) { break; }
		pLine = pEnd + 1;
		bFirst = false;
	}
	return true;
}

static bool __xrtIniOutputSection(xrtiniconfig* pConfig, xbuffer* pOutput,
	const xrt_ini_section* pSection)
{
	uint32 j;
	if ( !__xrtIniWritableName((const char*)pSection->Name, false) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_FORMAT, 0u, 0u, 0u,
			"INI section name cannot be written without changing its data");
		return false;
	}
	if ( !__xrtIniOutputText(pConfig, pOutput, (const char*)pSection->Leading) ) {
		return false;
	}
	if ( pSection->Name[0] != 0 &&
		(!__xrtIniOutputText(pConfig, pOutput, "[") ||
		 !__xrtIniOutputText(pConfig, pOutput, (const char*)pSection->Name) ||
		 !__xrtIniOutputText(pConfig, pOutput, "]\n")) ) {
		return false;
	}
	for ( j = 0u; j < pSection->Count; ++j ) {
		const xrt_ini_entry* pEntry = &pSection->Entries[j];
		if ( !__xrtIniWritableName((const char*)pEntry->Key, true) ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_FORMAT, 0u, 0u, 0u,
				"INI key cannot be written without changing its data");
			return false;
		}
		/* 文件起始的 U+FEFF 会被当作 BOM；用空行保住键自身的同一码点。 */
		if ( !__xrtIniOutputText(pConfig, pOutput, (const char*)pEntry->Leading) ||
			(pOutput->Size == 0u && strncmp((const char*)pEntry->Key, "\xef\xbb\xbf", 3u) == 0 &&
			 !__xrtIniOutputText(pConfig, pOutput, "\n")) ||
			!__xrtIniOutputText(pConfig, pOutput, (const char*)pEntry->Key) ||
			!__xrtIniOutputText(pConfig, pOutput, " = ") ||
			!__xrtIniOutputValue(pConfig, pOutput, (const char*)pEntry->Value) ) {
			return false;
		}
	}
	return true;
}

XRT_API xbuffer* xrtIniStringify(const xrtiniconfig* pConst)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	xbuffer* pOutput;
	uint32 i;
	if ( pConfig == NULL ) { return NULL; }
	xrtIniErrorInit(&pConfig->Error);
	pConfig->Failed = false;
	pOutput = xrtBufferCreate();
	if ( pOutput == NULL || !xrtBufferReserve(pOutput, 4096u) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate INI output buffer");
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	/* 全局节即使后来创建，也必须在任何具名节之前写出。 */
	for ( i = 0u; i < pConfig->Count; ++i ) {
		if ( pConfig->Sections[i].Name[0] == 0 &&
			!__xrtIniOutputSection(pConfig, pOutput, &pConfig->Sections[i]) ) {
			xrtBufferDestroy(pOutput);
			return NULL;
		}
	}
	for ( i = 0u; i < pConfig->Count; ++i ) {
		if ( pConfig->Sections[i].Name[0] != 0 &&
			!__xrtIniOutputSection(pConfig, pOutput, &pConfig->Sections[i]) ) {
			xrtBufferDestroy(pOutput);
			return NULL;
		}
	}
	if ( !__xrtIniOutputText(pConfig, pOutput,
		(const char*)(pConfig->Trailing != NULL ? pConfig->Trailing : (str)"")) ) {
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	pConfig->OutputBytes = pOutput->Size;
	return pOutput;
}

/* 写入或关闭失败时保留底层内存错误，其他失败仍按 IO 报告。 */
static int __xrtIniOutputErrorCode(void)
{
	const xerror* pCause = xrtGetError();
	return pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
		XRT_INI_ERROR_MEMORY : XRT_INI_ERROR_IO;
}

XRT_API bool xrtIniWrite(const xrtiniconfig* pConfig, xwriter* pWriter, bool bTakeOwnership)
{
	xbuffer* pOutput;
	bool bResult = false;
	if ( pConfig == NULL || pWriter == NULL ) {
		if ( bTakeOwnership ) { xrtWriterDestroy(pWriter); }
		return false;
	}
	pOutput = xrtIniStringify(pConfig);
	if ( pOutput != NULL ) {
		xrtClearError();
		bResult = xrtWriterWriteFull(pWriter, pOutput->Data, pOutput->Size, NULL) &&
			xrtWriterFlush(pWriter);
		if ( !bResult ) {
			__xrtIniSetError((xrtiniconfig*)pConfig, __xrtIniOutputErrorCode(), 0u, 0u, 0u,
				"cannot write INI stream");
		} else {
			xrtClearError();
		}
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership && !xrtWriterDestroy(pWriter) && bResult ) {
		/* 正文写入与刷新成功后，关闭失败仍表示写出未完成。 */
		bResult = false;
		__xrtIniSetError((xrtiniconfig*)pConfig, __xrtIniOutputErrorCode(), 0u, 0u, 0u,
			"cannot close INI stream");
	}
	return bResult;
}

XRT_API bool xrtIniSave(const xrtiniconfig* pConfig, str sPath)
{
	xbuffer* pOutput;
	bool bResult;
	if ( pConfig == NULL || sPath == NULL ) { return false; }
	pOutput = xrtIniStringify(pConfig);
	if ( pOutput == NULL ) { return false; }
	xrtClearError();
	bResult = xrtFileWriteAtomic(sPath,
		(xbytesview){ pOutput->Data, pOutput->Size });
	if ( !bResult ) {
		__xrtIniSetError((xrtiniconfig*)pConfig, __xrtIniOutputErrorCode(), 0u, 0u, 0u,
			"cannot save INI file: %s", sPath);
	}
	xrtBufferDestroy(pOutput);
	return bResult;
}
