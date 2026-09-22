#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_CODEC)

/* 校验 Quoted-Printable 行宽和编码标志。 */
static bool __xrtMailQpEncodeConfig(
	size_t* pLineSize,
	uint32 iFlags
)
{
	if ( iFlags & ~(uint32)XMAIL_QP_TEXT ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid quoted-printable encode flags"
		);
		return false;
	}
	if ( *pLineSize == 0 ) {
		*pLineSize = XMAIL_QP_LINE_DEFAULT;
	}
	if ( (*pLineSize < 4u) || (*pLineSize > XMAIL_QP_LINE_DEFAULT) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_CONFIG,
			"quoted-printable line size must be between 4 and 76"
		);
		return false;
	}
	return true;
}



/* 判断当前位置是否是文本模式下的一次换行。 */
static bool __xrtMailQpNewline(
	const uint8* pData,
	size_t iSize,
	size_t iPosition,
	size_t* pWidth
)
{
	if ( pData[iPosition] == (uint8)'\r' ) {
		*pWidth = (((iPosition + 1u) < iSize) &&
			(pData[iPosition + 1u] == (uint8)'\n')) ? 2u : 1u;
		return true;
	}
	if ( pData[iPosition] == (uint8)'\n' ) {
		*pWidth = 1u;
		return true;
	}
	return false;
}



/* 判断一个字节是否可以在当前位置直接输出。 */
static bool __xrtMailQpLiteral(
	const uint8* pData,
	size_t iSize,
	size_t iPosition,
	bool bText
)
{
	uint8 iByte = pData[iPosition];

	if ( ((iByte >= 33u) && (iByte <= 60u)) ||
		 ((iByte >= 62u) && (iByte <= 126u)) ) {
		return true;
	}
	if ( (iByte != (uint8)' ') && (iByte != (uint8)'\t') ) {
		return false;
	}
	if ( (iPosition + 1u) == iSize ) {
		return false;
	}
	if ( bText ) {
		size_t iWidth;

		if ( __xrtMailQpNewline(pData, iSize, iPosition + 1u, &iWidth) ) {
			return false;
		}
	}
	return true;
}



/* 判断当前令牌后面是否仍有同一逻辑行的数据。 */
static bool __xrtMailQpMoreOnLine(
	const uint8* pData,
	size_t iSize,
	size_t iNext,
	bool bText
)
{
	size_t iWidth;

	if ( iNext >= iSize ) {
		return false;
	}
	return !bText || !__xrtMailQpNewline(pData, iSize, iNext, &iWidth);
}



/* 计算或写出 Quoted-Printable 正文。 */
static bool __xrtMailQpBody(
	const uint8* pData,
	size_t iSize,
	size_t iLineSize,
	bool bText,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iOutput = 0;
	size_t iColumn = 0;

	for ( size_t i = 0; i < iSize; ) {
		size_t iNewlineWidth;
		size_t iTokenSize;
		bool bLiteral;
		bool bMore;

		if ( bText && __xrtMailQpNewline(
			pData,
			iSize,
			i,
			&iNewlineWidth
		) ) {
			if ( !__xrtMailSizeAdd(iOutput, 2u, &iOutput) ) {
				return false;
			}
			if ( sOutput != NULL ) {
				sOutput[iOutput - 2u] = '\r';
				sOutput[iOutput - 1u] = '\n';
			}
			i += iNewlineWidth;
			iColumn = 0;
			continue;
		}

		bLiteral = __xrtMailQpLiteral(pData, iSize, i, bText);
		iTokenSize = bLiteral ? 1u : 3u;
		bMore = __xrtMailQpMoreOnLine(pData, iSize, i + 1u, bText);
		if ( (iColumn != 0) &&
			 ((iColumn + iTokenSize + (bMore ? 1u : 0u)) > iLineSize) ) {
			if ( !__xrtMailSizeAdd(iOutput, 3u, &iOutput) ) {
				return false;
			}
			if ( sOutput != NULL ) {
				sOutput[iOutput - 3u] = '=';
				sOutput[iOutput - 2u] = '\r';
				sOutput[iOutput - 1u] = '\n';
			}
			iColumn = 0;
		}
		if ( !__xrtMailSizeAdd(iOutput, iTokenSize, &iOutput) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			if ( bLiteral ) {
				sOutput[iOutput - 1u] = (char)pData[i];
			} else {
				sOutput[iOutput - 3u] = '=';
				sOutput[iOutput - 2u] = __xrtMailHex(pData[i] >> 4u);
				sOutput[iOutput - 1u] = __xrtMailHex(pData[i]);
			}
		}
		iColumn += iTokenSize;
		i++;
	}
	if ( sOutput != NULL ) {
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 写出严格的 Quoted-Printable 文本。 */
XRT_API bool xrtMailQpWrite(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !xrtMemRangeValid(pData, iSize) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailQpEncodeConfig(&iLineSize, iFlags) ||
		 !__xrtMailQpBody(
			(const uint8*)pData,
			iSize,
			iLineSize,
			(iFlags & (uint32)XMAIL_QP_TEXT) != 0,
			NULL,
			&iRequired
		) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), pData, iSize) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailQpBody(
		(const uint8*)pData,
		iSize,
		iLineSize,
		(iFlags & (uint32)XMAIL_QP_TEXT) != 0,
		sOutput,
		pOutputSize
	);
}



/* 创建独立的 Quoted-Printable 文本。 */
XRT_API str xrtMailQp(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailQpWrite(
		pData,
		iSize,
		iLineSize,
		iFlags,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		pData,
		iSize
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailQpWrite(
		pData,
		iSize,
		iLineSize,
		iFlags,
		sOutput,
		iRequired + 1u,
		&iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 验证并计算 Quoted-Printable 解码长度。 */
static bool __xrtMailQpDecodedSize(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;

	if ( iFlags & ~(uint32)XMAIL_QP_RELAXED_SOFT_BREAK ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid quoted-printable decode flags"
		);
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( Text.Data[i] == '=' ) {
			if ( ((i + 2u) < Text.Size) &&
				 (__xrtMailHexValue((unsigned char)Text.Data[i + 1u]) >= 0) &&
				 (__xrtMailHexValue((unsigned char)Text.Data[i + 2u]) >= 0) ) {
				i += 2u;
			} else if ( ((i + 2u) < Text.Size) &&
				 (Text.Data[i + 1u] == '\r') &&
				 (Text.Data[i + 2u] == '\n') ) {
				i += 2u;
				continue;
			} else if ( ((iFlags & (uint32)XMAIL_QP_RELAXED_SOFT_BREAK) != 0) &&
				 ((i + 1u) < Text.Size) && (Text.Data[i + 1u] == '\n') ) {
				i++;
				continue;
			} else {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_ENCODING,
					"invalid quoted-printable escape"
				);
				return false;
			}
		}
		if ( iRequired == SIZE_MAX ) {
			__xrtMailSetSizeOverflow();
			return false;
		}
		iRequired++;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 在已验证输入上执行前向 Quoted-Printable 解码。 */
static void __xrtMailQpDecodeBody(
	xstrview Text,
	uint32 iFlags,
	uint8* pOutput
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( Text.Data[i] != '=' ) {
			pOutput[iOutput++] = (uint8)Text.Data[i];
			continue;
		}
		if ( ((i + 2u) < Text.Size) &&
			 (Text.Data[i + 1u] == '\r') &&
			 (Text.Data[i + 2u] == '\n') ) {
			i += 2u;
			continue;
		}
		if ( ((iFlags & (uint32)XMAIL_QP_RELAXED_SOFT_BREAK) != 0) &&
			 (Text.Data[i + 1u] == '\n') ) {
			i++;
			continue;
		}
		pOutput[iOutput++] = (uint8)(
			(__xrtMailHexValue((unsigned char)Text.Data[i + 1u]) << 4) |
			__xrtMailHexValue((unsigned char)Text.Data[i + 2u])
		);
		i += 2u;
	}
}



/* 解码 Quoted-Printable 到调用方缓冲区。 */
XRT_API bool xrtMailQpDecodeWrite(
	xstrview Text,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailQpDecodedSize(Text, iFlags, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) || ((pOutput != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		pOutput,
		iCapacity
	)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(pOutput, iRequired, Text.Data, Text.Size) &&
		 (pOutput != (const void*)Text.Data) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iRequired != 0 ) {
		__xrtMailQpDecodeBody(Text, iFlags, (uint8*)pOutput);
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 Quoted-Printable 解码结果。 */
XRT_API bytes xrtMailQpDecode(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bytes pOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailQpDecodeWrite(
		Text,
		iFlags,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iRequired + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailQpDecodeWrite(
		Text,
		iFlags,
		pOutput,
		iRequired,
		&iRequired
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iRequired] = 0;
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return pOutput;
}



/* 校验 MIME Base64 行宽。 */
static bool __xrtMailBase64Line(size_t* pLineSize)
{
	if ( *pLineSize == 0 ) {
		*pLineSize = XMAIL_BASE64_LINE_DEFAULT;
	}
	if ( (*pLineSize < 4u) ||
		 (*pLineSize > XMAIL_BASE64_LINE_DEFAULT) ||
		 ((*pLineSize % 4u) != 0) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_CONFIG,
			"MIME Base64 line size must be a multiple of four from 4 to 76"
		);
		return false;
	}
	return true;
}



/* 计算 MIME Base64 的总文本长度。 */
static bool __xrtMailBase64Size(
	size_t iSize,
	size_t iLineSize,
	size_t* pOutputSize
)
{
	size_t iChunkSize = (iLineSize / 4u) * 3u;
	size_t iRequired = 0;

	for ( size_t i = 0; i < iSize; ) {
		size_t iChunk = iSize - i;
		size_t iEncoded;

		if ( iChunk > iChunkSize ) {
			iChunk = iChunkSize;
		}
		iEncoded = ((iChunk + 2u) / 3u) * 4u;
		if ( !__xrtMailSizeAdd(iRequired, iEncoded + 2u, &iRequired) ) {
			return false;
		}
		i += iChunk;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 写出逐行 MIME Base64。 */
XRT_API bool xrtMailBase64Write(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iChunkSize;
	size_t iOutput = 0;

	if ( !xrtMemRangeValid(pData, iSize) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailBase64Line(&iLineSize) ||
		 !__xrtMailBase64Size(iSize, iLineSize, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), pData, iSize) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iChunkSize = (iLineSize / 4u) * 3u;
	for ( size_t i = 0; i < iSize; ) {
		size_t iChunk = iSize - i;
		size_t iEncoded;

		if ( iChunk > iChunkSize ) {
			iChunk = iChunkSize;
		}
		if ( !xrtBase64Encode(
			(const uint8*)pData + i,
			iChunk,
			sOutput + iOutput,
			iCapacity - iOutput,
			&iEncoded,
			NULL
		) ) {
			return false;
		}
		iOutput += iEncoded;
		sOutput[iOutput++] = '\r';
		sOutput[iOutput++] = '\n';
		i += iChunk;
	}
	sOutput[iOutput] = 0;
	*pOutputSize = iOutput;
	return true;
}



/* 创建独立的逐行 MIME Base64 文本。 */
XRT_API str xrtMailBase64(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailBase64Write(
		pData,
		iSize,
		iLineSize,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		pData,
		iSize
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailBase64Write(
		pData,
		iSize,
		iLineSize,
		sOutput,
		iRequired + 1u,
		&iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 使用 XRT 严格 Base64 解码器处理 MIME 空白。 */
XRT_API bool xrtMailBase64DecodeWrite(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xbase64config Config;

	Config.Alphabet = NULL;
	Config.Flags = XBASE64_IGNORE_SPACE;
	return xrtBase64Decode(
		Text.Data,
		Text.Size,
		pOutput,
		iCapacity,
		pOutputSize,
		&Config
	);
}



/* 创建独立的 MIME Base64 解码结果。 */
XRT_API bytes xrtMailBase64Decode(xstrview Text, size_t* pOutputSize)
{
	xbase64config Config;
	size_t iOutputSize;

	Config.Alphabet = NULL;
	Config.Flags = XBASE64_IGNORE_SPACE;
	return xrtBase64DecodeNew(
		Text.Data,
		Text.Size,
		pOutputSize != NULL ? pOutputSize : &iOutputSize,
		&Config
	);
}

#endif
