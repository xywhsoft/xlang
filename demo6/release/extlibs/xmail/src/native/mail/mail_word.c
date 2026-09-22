#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_WORD)

#define XMAIL_WORD_LIMIT 75u
#define XMAIL_WORD_PREFIX_SIZE 10u
#define XMAIL_WORD_SUFFIX_SIZE 2u
#define XMAIL_WORD_PAYLOAD_SIZE \
	(XMAIL_WORD_LIMIT - XMAIL_WORD_PREFIX_SIZE - XMAIL_WORD_SUFFIX_SIZE)



typedef enum __xmailwordparse {
	__XMAIL_WORD_NONE = 0,
	__XMAIL_WORD_VALID,
	__XMAIL_WORD_INVALID
} __xmailwordparse;



typedef enum __xmailworddecode {
	__XMAIL_WORD_DECODED = 0,
	__XMAIL_WORD_KEEP,
	__XMAIL_WORD_FAILED
} __xmailworddecode;



/* 比较不区分大小写的 ASCII 视图与常量。 */
static bool __xrtMailWordCaseEqual(xstrview Text, cstr sValue)
{
	return __xrtMailAsciiEqualI(
		Text,
		__xrtMailView(sValue, strlen(sValue))
	);
}



/* 不发布错误地识别输入开头的编码词。 */
static __xmailwordparse __xrtMailWordParseBody(
	xstrview Text,
	xmailwordview* pWord
)
{
	size_t iCharsetEnd;
	size_t iEncodedStart;
	size_t iEncodedEnd;
	xmailwordview Word;
	unsigned char iEncoding;

	if ( (Text.Size < 2u) || (Text.Data[0] != '=') || (Text.Data[1] != '?') ) {
		return __XMAIL_WORD_NONE;
	}
	iCharsetEnd = 2u;
	while ( (iCharsetEnd < Text.Size) && (Text.Data[iCharsetEnd] != '?') ) {
		unsigned char iByte = (unsigned char)Text.Data[iCharsetEnd];

		if ( (iByte < 33u) || (iByte > 126u) ) {
			return __XMAIL_WORD_INVALID;
		}
		iCharsetEnd++;
	}
	if ( (iCharsetEnd == 2u) || ((iCharsetEnd + 3u) > Text.Size) ) {
		return __XMAIL_WORD_INVALID;
	}
	iEncoding = __xrtMailAsciiLower(
		(unsigned char)Text.Data[iCharsetEnd + 1u]
	);
	if ( ((iEncoding != (unsigned char)'b') &&
		  (iEncoding != (unsigned char)'q')) ||
		 (Text.Data[iCharsetEnd + 2u] != '?') ) {
		return __XMAIL_WORD_INVALID;
	}
	iEncodedStart = iCharsetEnd + 3u;
	iEncodedEnd = iEncodedStart;
	while ( iEncodedEnd < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[iEncodedEnd];

		if ( iByte == (unsigned char)'?' ) {
			break;
		}
		if ( (iByte < 33u) || (iByte > 126u) ) {
			return __XMAIL_WORD_INVALID;
		}
		iEncodedEnd++;
	}
	if ( (iEncodedEnd == iEncodedStart) || ((iEncodedEnd + 1u) >= Text.Size) ||
		 (Text.Data[iEncodedEnd + 1u] != '=') ||
		 ((iEncodedEnd + 2u) > XMAIL_WORD_LIMIT) ) {
		return __XMAIL_WORD_INVALID;
	}
	Word.Source = __xrtMailView(Text.Data, iEncodedEnd + 2u);
	Word.Charset = __xrtMailView(Text.Data + 2u, iCharsetEnd - 2u);
	Word.Encoded = __xrtMailView(
		Text.Data + iEncodedStart,
		iEncodedEnd - iEncodedStart
	);
	Word.Encoding = iEncoding == (unsigned char)'b' ?
		XMAIL_WORD_BASE64 : XMAIL_WORD_Q;
	*pWord = Word;
	return __XMAIL_WORD_VALID;
}



/* 从输入开头读取一个编码词。 */
XRT_API xmailnext xrtMailWordParse(xstrview Text, xmailwordview* pWord)
{
	__xmailwordparse Status;
	xmailwordview Word;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pWord, sizeof(*pWord)) ||
		 xrtMemRangesOverlap(pWord, sizeof(*pWord), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Status = __xrtMailWordParseBody(Text, &Word);
	if ( Status == __XMAIL_WORD_NONE ) {
		return XMAIL_NEXT_END;
	}
	if ( Status == __XMAIL_WORD_INVALID ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_ENCODING,
			"invalid RFC 2047 encoded-word"
		);
		return XMAIL_NEXT_ERROR;
	}
	*pWord = Word;
	return XMAIL_NEXT_ITEM;
}



/* 检查文本能否安全进入邮件字段值。 */
static bool __xrtMailWordTextValid(xstrview Text)
{
	if ( !xrtUtf8Valid(Text, NULL) ) {
		__xrtMailError(
			XERR_VALUE,
			XMAIL_ERROR_CHARSET,
			"mail encoded-word input is not valid UTF-8"
		);
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			__xrtMailError(
				XERR_VALUE,
				XMAIL_ERROR_HEADER,
				"mail encoded-word text contains a control byte"
			);
			return false;
		}
	}
	return true;
}



/* 判断纯 ASCII 文本是否仍需避免被误识别为编码词。 */
static bool __xrtMailWordNeedsEncoding(xstrview Text)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( ((unsigned char)Text.Data[i] >= 128u) ||
			 ((Text.Data[i] == '=') && ((i + 1u) < Text.Size) &&
			  (Text.Data[i + 1u] == '?')) ) {
			return true;
		}
	}
	return false;
}



/* Q 编码只原样保留在 phrase 与任意 encoded-word 中都安全的字节。 */
static bool __xrtMailWordQSafe(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ||
		(iByte == (unsigned char)'!') || (iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'+') || (iByte == (unsigned char)'-') ||
		(iByte == (unsigned char)'/');
}



/* 返回一个字节的 Q 编码长度。 */
static size_t __xrtMailWordQSize(unsigned char iByte)
{
	return ((iByte == (unsigned char)' ') || __xrtMailWordQSafe(iByte)) ?
		1u : 3u;
}



/* 返回下一个不拆开 UTF-8 标量的编码块。 */
static size_t __xrtMailWordChunk(
	xstrview Text,
	size_t iPosition,
	xmailwordencoding Encoding,
	size_t* pPayloadSize
)
{
	size_t iEnd = iPosition;
	size_t iPayload = 0;

	while ( iEnd < Text.Size ) {
		uint32 iScalar;
		size_t iRead;
		size_t iToken = 0;

		(void)xrtUtf8Decode(
			__xrtMailView(Text.Data + iEnd, Text.Size - iEnd),
			&iScalar,
			&iRead
		);
		(void)iScalar;
		if ( Encoding == XMAIL_WORD_BASE64 ) {
			size_t iBytes = (iEnd + iRead) - iPosition;

			iToken = ((iBytes + 2u) / 3u) * 4u;
			if ( iToken > XMAIL_WORD_PAYLOAD_SIZE ) {
				break;
			}
			iPayload = iToken;
		} else {
			for ( size_t i = 0; i < iRead; i++ ) {
				iToken += __xrtMailWordQSize(
					(unsigned char)Text.Data[iEnd + i]
				);
			}
			if ( iToken > (XMAIL_WORD_PAYLOAD_SIZE - iPayload) ) {
				break;
			}
			iPayload += iToken;
		}
		iEnd += iRead;
	}
	*pPayloadSize = iPayload;
	return iEnd;
}



/* 计算或写出完整编码词序列。 */
static bool __xrtMailWordEncodeBody(
	xstrview Text,
	xmailwordencoding Encoding,
	char* sOutput,
	size_t* pOutputSize
)
{
	static const char arrPrefix[][11] = {
		"=?UTF-8?B?",
		"=?UTF-8?Q?"
	};
	size_t iPosition = 0;
	size_t iOutput = 0;
	bool bFirst = true;

	while ( iPosition < Text.Size ) {
		size_t iPayload;
		size_t iEnd = __xrtMailWordChunk(
			Text,
			iPosition,
			Encoding,
			&iPayload
		);
		size_t iWordSize = XMAIL_WORD_PREFIX_SIZE + iPayload +
			XMAIL_WORD_SUFFIX_SIZE;

		if ( !bFirst && !__xrtMailSizeAdd(iOutput, 1u, &iOutput) ) {
			return false;
		}
		if ( !__xrtMailSizeAdd(iOutput, iWordSize, &iOutput) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			size_t iWrite = iOutput - iWordSize;

			if ( !bFirst ) {
				sOutput[iWrite - 1u] = ' ';
			}
			memcpy(sOutput + iWrite, arrPrefix[Encoding],
				XMAIL_WORD_PREFIX_SIZE);
			iWrite += XMAIL_WORD_PREFIX_SIZE;
			if ( Encoding == XMAIL_WORD_BASE64 ) {
				size_t iEncoded;

				(void)xrtBase64Encode(
					Text.Data + iPosition,
					iEnd - iPosition,
					sOutput + iWrite,
					iPayload + 1u,
					&iEncoded,
					NULL
				);
			} else {
				for ( size_t i = iPosition; i < iEnd; i++ ) {
					unsigned char iByte = (unsigned char)Text.Data[i];

					if ( iByte == (unsigned char)' ' ) {
						sOutput[iWrite++] = '_';
					} else if ( __xrtMailWordQSafe(iByte) ) {
						sOutput[iWrite++] = (char)iByte;
					} else {
						sOutput[iWrite++] = '=';
						sOutput[iWrite++] = __xrtMailHex(iByte >> 4u);
						sOutput[iWrite++] = __xrtMailHex(iByte);
					}
				}
			}
			sOutput[iOutput - 2u] = '?';
			sOutput[iOutput - 1u] = '=';
		}
		iPosition = iEnd;
		bFirst = false;
	}
	if ( sOutput != NULL ) {
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 写出 RFC 2047 编码词序列。 */
XRT_API bool xrtMailWordEncodeWrite(
	xstrview Text,
	xmailwordencoding Encoding,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bool bEncode;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((Encoding != XMAIL_WORD_BASE64) && (Encoding != XMAIL_WORD_Q)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailWordTextValid(Text) ) {
		return false;
	}
	bEncode = __xrtMailWordNeedsEncoding(Text);
	if ( bEncode ) {
		if ( !__xrtMailWordEncodeBody(Text, Encoding, NULL, &iRequired) ) {
			return false;
		}
	} else {
		iRequired = Text.Size;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
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
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Text.Data,
		Text.Size
	) && ((const void*)sOutput != (const void*)Text.Data || bEncode) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( bEncode ) {
		return __xrtMailWordEncodeBody(
			Text,
			Encoding,
			sOutput,
			pOutputSize
		);
	}
	memmove(sOutput, Text.Data, Text.Size);
	sOutput[Text.Size] = 0;
	*pOutputSize = Text.Size;
	return true;
}



/* 创建独立的 RFC 2047 字段文本。 */
XRT_API str xrtMailWordEncode(
	xstrview Text,
	xmailwordencoding Encoding,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailWordEncodeWrite(
		Text,
		Encoding,
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
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailWordEncodeWrite(
		Text,
		Encoding,
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



/* 返回标准 Base64 字节值。 */
static int __xrtMailWordBase64Value(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z') ) {
		return (int)(iByte - (unsigned char)'A');
	}
	if ( (iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z') ) {
		return (int)(iByte - (unsigned char)'a') + 26;
	}
	if ( (iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0') + 52;
	}
	if ( iByte == (unsigned char)'+' ) {
		return 62;
	}
	return iByte == (unsigned char)'/' ? 63 : -1;
}



/* 在调用 XRT 前无副作用地验证规范 Base64。 */
static bool __xrtMailWordBase64Valid(xstrview Text)
{
	size_t iPadding = 0;
	size_t iData;
	int iLast;

	if ( (Text.Size == 0) || ((Text.Size % 4u) != 0) ) {
		return false;
	}
	while ( (iPadding < Text.Size) &&
		 (Text.Data[Text.Size - iPadding - 1u] == '=') ) {
		iPadding++;
	}
	if ( iPadding > 2u ) {
		return false;
	}
	iData = Text.Size - iPadding;
	if ( (iData == 0) || ((iPadding == 1u) && ((iData % 4u) != 3u)) ||
		 ((iPadding == 2u) && ((iData % 4u) != 2u)) ) {
		return false;
	}
	for ( size_t i = 0; i < iData; i++ ) {
		if ( __xrtMailWordBase64Value((unsigned char)Text.Data[i]) < 0 ) {
			return false;
		}
	}
	for ( size_t i = iData; i < Text.Size; i++ ) {
		if ( Text.Data[i] != '=' ) {
			return false;
		}
	}
	iLast = __xrtMailWordBase64Value((unsigned char)Text.Data[iData - 1u]);
	return ((iPadding != 2u) || ((iLast & 0x0F) == 0)) &&
		((iPadding != 1u) || ((iLast & 0x03) == 0));
}



/* 解码并严格校验 Q 编码正文。 */
static bool __xrtMailWordQDecode(
	xstrview Text,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( iByte == (unsigned char)'_' ) {
			sOutput[iOutput++] = ' ';
			continue;
		}
		if ( iByte != (unsigned char)'=' ) {
			sOutput[iOutput++] = (char)iByte;
			continue;
		}
		if ( ((i + 2u) >= Text.Size) ||
			 (__xrtMailHexValue((unsigned char)Text.Data[i + 1u]) < 0) ||
			 (__xrtMailHexValue((unsigned char)Text.Data[i + 2u]) < 0) ) {
			return false;
		}
		sOutput[iOutput++] = (char)(
			(__xrtMailHexValue((unsigned char)Text.Data[i + 1u]) << 4) |
			__xrtMailHexValue((unsigned char)Text.Data[i + 2u])
		);
		i += 2u;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 判断字符集并校验解码后的安全 UTF-8 文本。 */
static bool __xrtMailWordDecodedValid(
	xmailwordview Word,
	xstrview Decoded
)
{
	bool bUtf8 = __xrtMailWordCaseEqual(Word.Charset, "UTF-8") ||
		__xrtMailWordCaseEqual(Word.Charset, "UTF8");
	bool bAscii = __xrtMailWordCaseEqual(Word.Charset, "US-ASCII") ||
		__xrtMailWordCaseEqual(Word.Charset, "ASCII");

	if ( !bUtf8 && !bAscii ) {
		return false;
	}
	for ( size_t i = 0; i < Decoded.Size; i++ ) {
		unsigned char iByte = (unsigned char)Decoded.Data[i];

		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ||
			 (bAscii && (iByte >= 128u)) ) {
			return false;
		}
	}
	return bAscii || xrtUtf8Valid(Decoded, NULL);
}



/* 解码单个词；容错模式把任何无法可靠解释的词留给调用方。 */
static __xmailworddecode __xrtMailWordDecodeOne(
	xmailwordview Word,
	uint32 iFlags,
	char arrDecoded[64],
	size_t* pDecodedSize,
	bool bPublishError
)
{
	bool bKnown = __xrtMailWordCaseEqual(Word.Charset, "UTF-8") ||
		__xrtMailWordCaseEqual(Word.Charset, "UTF8") ||
		__xrtMailWordCaseEqual(Word.Charset, "US-ASCII") ||
		__xrtMailWordCaseEqual(Word.Charset, "ASCII");
	bool bValid;

	if ( !bKnown ) {
		bValid = false;
	} else if ( Word.Encoding == XMAIL_WORD_BASE64 ) {
		bValid = __xrtMailWordBase64Valid(Word.Encoded);
		if ( bValid ) {
			bValid = xrtBase64Decode(
				Word.Encoded.Data,
				Word.Encoded.Size,
				arrDecoded,
				64u,
				pDecodedSize,
				NULL
			);
		}
	} else {
		bValid = __xrtMailWordQDecode(
			Word.Encoded,
			arrDecoded,
			pDecodedSize
		);
	}
	if ( bValid ) {
		bValid = __xrtMailWordDecodedValid(
			Word,
			__xrtMailView(arrDecoded, *pDecodedSize)
		);
	}
	if ( bValid ) {
		return __XMAIL_WORD_DECODED;
	}
	if ( (iFlags & (uint32)XMAIL_WORD_RELAXED) != 0 ) {
		return __XMAIL_WORD_KEEP;
	}
	if ( bPublishError ) {
		__xrtMailError(
			XERR_PROTOCOL,
			bKnown ? XMAIL_ERROR_ENCODING : XMAIL_ERROR_CHARSET,
			bKnown ? "invalid RFC 2047 encoded text" :
				"unsupported RFC 2047 charset"
		);
	}
	return __XMAIL_WORD_FAILED;
}



/* 校验并累计一个将进入 UTF-8 结果的片段。 */
static bool __xrtMailWordEmit(
	xstrview Text,
	char* sOutput,
	size_t* pPosition
)
{
	if ( !__xrtMailWordTextValid(Text) ||
		 !__xrtMailSizeAdd(*pPosition, Text.Size, pPosition) ) {
		return false;
	}
	if ( sOutput != NULL ) {
		memmove(sOutput + *pPosition - Text.Size, Text.Data, Text.Size);
	}
	return true;
}



/* 返回线性空白结尾，并记录其中是否含合法折叠。 */
static bool __xrtMailWordSpaceEnd(
	xstrview Text,
	size_t iStart,
	size_t* pEnd,
	bool* pFolded
)
{
	size_t i = iStart;
	bool bFolded = false;

	while ( i < Text.Size ) {
		if ( (Text.Data[i] == ' ') || (Text.Data[i] == '\t') ) {
			i++;
			continue;
		}
		if ( Text.Data[i] == '\n' ) {
			return false;
		}
		if ( Text.Data[i] != '\r' ) {
			break;
		}
		if ( ((i + 2u) >= Text.Size) || (Text.Data[i + 1u] != '\n') ||
			 ((Text.Data[i + 2u] != ' ') && (Text.Data[i + 2u] != '\t')) ) {
			return false;
		}
		bFolded = true;
		i += 3u;
	}
	*pEnd = i;
	*pFolded = bFolded;
	return true;
}



/* 判断指定位置是否是可以被实际解码的编码词。 */
static bool __xrtMailWordDecodedAt(
	xstrview Text,
	size_t iPosition,
	uint32 iFlags
)
{
	xmailwordview Word;
	char arrDecoded[64];
	size_t iDecoded = 0;

	if ( __xrtMailWordParseBody(
		__xrtMailView(Text.Data + iPosition, Text.Size - iPosition),
		&Word
	) != __XMAIL_WORD_VALID ) {
		return false;
	}
	return __xrtMailWordDecodeOne(
		Word,
		iFlags,
		arrDecoded,
		&iDecoded,
		false
	) == __XMAIL_WORD_DECODED;
}



/* 验证、计量或写出完整解码字段文本。 */
static bool __xrtMailWordDecodeBody(
	xstrview Text,
	uint32 iFlags,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iPosition = 0;
	size_t iOutput = 0;
	bool bPreviousDecoded = false;

	while ( iPosition < Text.Size ) {
		if ( (Text.Data[iPosition] == ' ') ||
			 (Text.Data[iPosition] == '\t') ||
			 (Text.Data[iPosition] == '\r') ||
			 (Text.Data[iPosition] == '\n') ) {
			size_t iEnd;
			bool bFolded;

			if ( !__xrtMailWordSpaceEnd(
				Text,
				iPosition,
				&iEnd,
				&bFolded
			) ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_LINE,
					"invalid folding in RFC 2047 field text"
				);
				return false;
			}
			if ( bPreviousDecoded && (iEnd < Text.Size) &&
				 __xrtMailWordDecodedAt(Text, iEnd, iFlags) ) {
				iPosition = iEnd;
				continue;
			}
			if ( bFolded ) {
				if ( !__xrtMailWordEmit(
					XRT_STR_LITERAL(" "),
					sOutput,
					&iOutput
				) ) {
					return false;
				}
			} else if ( !__xrtMailWordEmit(
				__xrtMailView(Text.Data + iPosition, iEnd - iPosition),
				sOutput,
				&iOutput
			) ) {
				return false;
			}
			iPosition = iEnd;
			bPreviousDecoded = false;
			continue;
		}
		if ( (Text.Data[iPosition] == '=') &&
			 ((iPosition + 1u) < Text.Size) &&
			 (Text.Data[iPosition + 1u] == '?') ) {
			xmailwordview Word;
			char arrDecoded[64];
			size_t iDecoded = 0;
			__xmailwordparse Parse = __xrtMailWordParseBody(
				__xrtMailView(Text.Data + iPosition, Text.Size - iPosition),
				&Word
			);
			__xmailworddecode Decode;

			if ( Parse == __XMAIL_WORD_VALID ) {
				Decode = __xrtMailWordDecodeOne(
					Word,
					iFlags,
					arrDecoded,
					&iDecoded,
					true
				);
				if ( Decode == __XMAIL_WORD_FAILED ) {
					return false;
				}
				if ( Decode == __XMAIL_WORD_DECODED ) {
					if ( !__xrtMailWordEmit(
						__xrtMailView(arrDecoded, iDecoded),
						sOutput,
						&iOutput
					) ) {
						return false;
					}
					bPreviousDecoded = true;
				} else {
					if ( !__xrtMailWordEmit(
						Word.Source,
						sOutput,
						&iOutput
					) ) {
						return false;
					}
					bPreviousDecoded = false;
				}
				iPosition += Word.Source.Size;
				continue;
			}
			if ( (iFlags & (uint32)XMAIL_WORD_RELAXED) == 0 ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_ENCODING,
					"malformed RFC 2047 encoded-word"
				);
				return false;
			}
		}
		{
			size_t iEnd = iPosition + 1u;

			while ( iEnd < Text.Size ) {
				if ( (Text.Data[iEnd] == ' ') || (Text.Data[iEnd] == '\t') ||
					 (Text.Data[iEnd] == '\r') || (Text.Data[iEnd] == '\n') ||
					 ((Text.Data[iEnd] == '=') && ((iEnd + 1u) < Text.Size) &&
					  (Text.Data[iEnd + 1u] == '?')) ) {
					break;
				}
				iEnd++;
			}
			if ( !__xrtMailWordEmit(
				__xrtMailView(Text.Data + iPosition, iEnd - iPosition),
				sOutput,
				&iOutput
			) ) {
				return false;
			}
			iPosition = iEnd;
			bPreviousDecoded = false;
		}
	}
	if ( sOutput != NULL ) {
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 解码混合普通文本与 RFC 2047 编码词的字段值。 */
XRT_API bool xrtMailWordDecodeWrite(
	xstrview Text,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 (iFlags & ~(uint32)XMAIL_WORD_RELAXED) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailWordDecodeBody(Text, iFlags, NULL, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
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
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Text.Data,
		Text.Size
	) && ((const void*)sOutput != (const void*)Text.Data) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailWordDecodeBody(
		Text,
		iFlags,
		sOutput,
		pOutputSize
	);
}



/* 创建独立的 UTF-8 解码文本。 */
XRT_API str xrtMailWordDecode(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailWordDecodeWrite(
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
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailWordDecodeWrite(
		Text,
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

#endif
