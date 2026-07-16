#include "xrt_ini_internal.h"

#include <string.h>



static bool __xrtIniOutput(xrtiniconfig* pConfig, xbuffer pOutput,
	const void* pData, size_t iSize)
{
	if ( iSize == 0u ) { return TRUE; }
	if ( iSize > UINT32_MAX ||
		(pConfig->Options.tLimits.iMaxOutputBytes != 0u &&
		 ((uint64)iSize > pConfig->Options.tLimits.iMaxOutputBytes ||
		  pOutput->Length > pConfig->Options.tLimits.iMaxOutputBytes - (uint64)iSize)) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
			"INI output exceeds the configured limit");
		return FALSE;
	}
	if ( !xrtBufferAppend(pOutput, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot grow INI output buffer");
		return FALSE;
	}
	return TRUE;
}

static bool __xrtIniOutputText(xrtiniconfig* pConfig, xbuffer pOutput, const char* sText)
{
	return __xrtIniOutput(pConfig, pOutput, sText, sText != NULL ? strlen(sText) : 0u);
}

static bool __xrtIniOutputValue(xrtiniconfig* pConfig, xbuffer pOutput, const char* sValue)
{
	const char* p = sValue != NULL ? sValue : "";
	const char* pLine = p;
	bool bFirst = TRUE;
	for ( ;; ) {
		const char* pEnd = strchr(pLine, '\n');
		size_t iLength = pEnd != NULL ? (size_t)(pEnd - pLine) : strlen(pLine);
		if ( !bFirst && !__xrtIniOutputText(pConfig, pOutput, "    ") ) { return FALSE; }
		if ( !__xrtIniOutput(pConfig, pOutput, pLine, iLength) ||
			!__xrtIniOutputText(pConfig, pOutput, "\n") ) { return FALSE; }
		if ( pEnd == NULL ) { break; }
		pLine = pEnd + 1;
		bFirst = FALSE;
	}
	return TRUE;
}

XXAPI xbuffer xrtIniStringify(const xrtiniconfig* pConst)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	xbuffer pOutput;
	uint32 i;
	uint32 j;
	if ( pConfig == NULL ) { return NULL; }
	xrtIniErrorInit(&pConfig->Error);
	pConfig->Failed = FALSE;
	pOutput = xrtBufferCreate(4096u);
	if ( pOutput == NULL ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate INI output buffer");
		return NULL;
	}
	for ( i = 0u; i < pConfig->Count; ++i ) {
		const xrt_ini_section* pSection = &pConfig->Sections[i];
		if ( !__xrtIniOutputText(pConfig, pOutput, (const char*)pSection->Leading) ) {
			xrtBufferDestroy(pOutput);
			return NULL;
		}
		if ( pSection->Name[0] != 0 ) {
			if ( !__xrtIniOutputText(pConfig, pOutput, "[") ||
				!__xrtIniOutputText(pConfig, pOutput, (const char*)pSection->Name) ||
				!__xrtIniOutputText(pConfig, pOutput, "]\n") ) {
				xrtBufferDestroy(pOutput);
				return NULL;
			}
		}
		for ( j = 0u; j < pSection->Count; ++j ) {
			const xrt_ini_entry* pEntry = &pSection->Entries[j];
			if ( !__xrtIniOutputText(pConfig, pOutput, (const char*)pEntry->Leading) ||
				!__xrtIniOutputText(pConfig, pOutput, (const char*)pEntry->Key) ||
				!__xrtIniOutputText(pConfig, pOutput, " = ") ||
				!__xrtIniOutputValue(pConfig, pOutput, (const char*)pEntry->Value) ) {
				xrtBufferDestroy(pOutput);
				return NULL;
			}
		}
	}
	if ( !__xrtIniOutputText(pConfig, pOutput,
		(const char*)(pConfig->Trailing != NULL ? pConfig->Trailing : (str)"")) ) {
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	pConfig->OutputBytes = pOutput->Length;
	return pOutput;
}

XXAPI bool xrtIniWrite(const xrtiniconfig* pConfig, xwriter pWriter, bool bTakeOwnership)
{
	xbuffer pOutput;
	bool bResult = FALSE;
	if ( pConfig == NULL || pWriter == NULL ) {
		if ( bTakeOwnership ) { xrtWriterDestroy(pWriter); }
		return FALSE;
	}
	pOutput = xrtIniStringify(pConfig);
	if ( pOutput != NULL ) {
		bResult = xrtWriterWriteAll(pWriter, pOutput->Buffer, pOutput->Length) &&
			xrtWriterFlush(pWriter);
		if ( !bResult ) {
			__xrtIniSetError((xrtiniconfig*)pConfig, XRT_INI_ERROR_IO, 0u, 0u, 0u,
				"cannot write INI stream");
		}
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership ) { xrtWriterDestroy(pWriter); }
	return bResult;
}

XXAPI bool xrtIniSave(const xrtiniconfig* pConfig, str sPath)
{
	xbuffer pOutput;
	bool bResult;
	if ( pConfig == NULL || sPath == NULL ) { return FALSE; }
	pOutput = xrtIniStringify(pConfig);
	if ( pOutput == NULL ) { return FALSE; }
	bResult = xrtFileWriteAllAtomic(sPath, (str)pOutput->Buffer,
		pOutput->Length, XRT_CP_BINARY);
	if ( !bResult ) {
		__xrtIniSetError((xrtiniconfig*)pConfig, XRT_INI_ERROR_IO, 0u, 0u, 0u,
			"cannot save INI file: %s", sPath);
	}
	xrtBufferDestroy(pOutput);
	return bResult;
}
