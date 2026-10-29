#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_SMTP)

/* 验证 envelope 路径没有分隔符或控制字符。 */
XRT_API bool xrtSmtpPathValid(xstrview Path, bool AllowEmpty)
{
	if ( !__xrtMailViewValid(Path) || (!AllowEmpty && (Path.Size == 0)) ) {
		return false;
	}
	for ( size_t i = 0; i < Path.Size; i++ ) {
		unsigned char iByte = (unsigned char)Path.Data[i];

		if ( (iByte <= 32u) || (iByte == 127u) ||
			(iByte == (unsigned char)'<') ||
			(iByte == (unsigned char)'>') ) {
			return false;
		}
	}
	return true;
}



/* 返回 ASCII 字节是否可以用于 SMTP 命令名称。 */
static bool __xrtSmtpVerbByte(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ||
		(iByte == (unsigned char)'-');
}



/* 解析固定三位 SMTP 响应码。 */
static bool __xrtSmtpReplyCode(xstrview Line, int* pCode)
{
	int iCode = 0;

	if ( Line.Size < 4u ) {
		return false;
	}
	for ( size_t i = 0; i < 3u; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ) {
			return false;
		}
		iCode = (iCode * 10) + (int)(iByte - (unsigned char)'0');
	}
	if ( (iCode < 100) || (iCode > 599) ) {
		return false;
	}
	*pCode = iCode;
	return true;
}



/* 解析一条 SMTP 响应行。 */
XRT_API bool xrtSmtpReplyLineParse(
	xstrview Line,
	xsmtpreplyline* pReply
)
{
	xsmtpreplyline Reply;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		 xrtMemRangesOverlap(pReply, sizeof(*pReply), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtSmtpReplyCode(Line, &Reply.Code) ||
		 ((Line.Data[3] != ' ') && (Line.Data[3] != '-')) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid SMTP reply line"
		);
		return false;
	}
	for ( size_t i = 4u; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			 (iByte == (unsigned char)'\n') ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"SMTP reply contains a control character"
			);
			return false;
		}
	}
	Reply.Source = Line;
	Reply.Text = __xrtMailSlice(Line, 4u, Line.Size - 4u);
	Reply.Continued = Line.Data[3] == '-';
	*pReply = Reply;
	return true;
}



/* 初始化多行 SMTP 响应验证器。 */
XRT_API bool xrtSmtpReplyParserInit(
	xsmtpreplyparser* pParser,
	size_t iMaxLines
)
{
	if ( !xrtMemRangeValid(pParser, sizeof(*pParser)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iMaxLines == 0 ) {
		iMaxLines = XSMTP_REPLY_LINES_DEFAULT;
	}
	pParser->Code = 0;
	pParser->Lines = 0;
	pParser->MaxLines = iMaxLines;
	pParser->Started = false;
	pParser->Done = false;
	return true;
}



/* 接受一条属于当前响应的 SMTP 行。 */
XRT_API bool xrtSmtpReplyRead(
	xsmtpreplyparser* pParser,
	xstrview Line,
	xsmtpreplyline* pReply
)
{
	xsmtpreplyline Reply;

	if ( !xrtMemRangeValid(pParser, sizeof(*pParser)) ||
		 !xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		 (pParser == NULL) || pParser->Done ||
		 (pParser->Lines > pParser->MaxLines) ||
		 xrtMemRangesOverlap(pParser, sizeof(*pParser), pReply,
			sizeof(*pReply)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtSmtpReplyLineParse(Line, &Reply) ) {
		return false;
	}
	if ( pParser->Lines == pParser->MaxLines ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"SMTP reply exceeds the line limit"
		);
		return false;
	}
	if ( pParser->Started && (Reply.Code != pParser->Code) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"SMTP multiline reply changed its status code"
		);
		return false;
	}
	if ( !pParser->Started ) {
		pParser->Code = Reply.Code;
		pParser->Started = true;
	}
	pParser->Lines++;
	pParser->Done = !Reply.Continued;
	*pReply = Reply;
	return true;
}



/* 解析 EHLO 能力名称和参数。 */
XRT_API bool xrtSmtpCapabilityParse(
	xstrview Text,
	xsmtpcapabilityview* pCapability
)
{
	xsmtpcapabilityview Capability;
	size_t iPosition = 0;
	size_t iParameters;
	size_t iEqual = XRT_NPOS;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pCapability, sizeof(*pCapability)) ||
		 xrtMemRangesOverlap(pCapability, sizeof(*pCapability), Text.Data,
			Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	while ( (iPosition < Text.Size) &&
		 (Text.Data[iPosition] != ' ') &&
		 (Text.Data[iPosition] != '\t') ) {
		unsigned char iByte = (unsigned char)Text.Data[iPosition];

		if ( iByte == (unsigned char)'=' ) {
			if ( iEqual != XRT_NPOS ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_PROTOCOL,
					"invalid SMTP capability name"
				);
				return false;
			}
			iEqual = iPosition++;
			continue;
		}
		if ( !__xrtSmtpVerbByte(iByte) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid SMTP capability name"
			);
			return false;
		}
		iPosition++;
	}
	if ( iPosition == 0 ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"empty SMTP capability name"
		);
		return false;
	}
	if ( iEqual != XRT_NPOS ) {
		if ( (iEqual != 4u) || ((iEqual + 1u) >= iPosition) ||
			 !__xrtMailAsciiEqualI(
				__xrtMailSlice(Text, 0, iEqual),
				XRT_STR_LITERAL("AUTH")
			 )) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid SMTP AUTH capability"
			);
			return false;
		}
		iParameters = iEqual + 1u;
	} else {
		iParameters = iPosition;
		while ( (iParameters < Text.Size) &&
			 ((Text.Data[iParameters] == ' ') ||
			  (Text.Data[iParameters] == '\t')) ) {
			iParameters++;
		}
	}
	for ( size_t i = iParameters; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"SMTP capability contains a control character"
			);
			return false;
		}
	}
	Capability.Source = Text;
	Capability.Name = __xrtMailSlice(
		Text,
		0,
		iEqual != XRT_NPOS ? iEqual : iPosition
	);
	Capability.Parameters = __xrtMailSlice(
		Text,
		iParameters,
		Text.Size - iParameters
	);
	*pCapability = Capability;
	return true;
}



/* 查找 SMTP 能力名称的稳定标记。 */
XRT_API uint64 xrtSmtpCapability(xstrview Name)
{
	static const struct {
		cstr Name;
		size_t Size;
		uint64 Value;
	} arrCapabilities[] = {
		{ "PIPELINING", 10u, XSMTP_CAP_PIPELINING },
		{ "SIZE", 4u, XSMTP_CAP_SIZE },
		{ "STARTTLS", 8u, XSMTP_CAP_STARTTLS },
		{ "8BITMIME", 8u, XSMTP_CAP_8BITMIME },
		{ "SMTPUTF8", 8u, XSMTP_CAP_SMTPUTF8 },
		{ "DSN", 3u, XSMTP_CAP_DSN },
		{ "CHUNKING", 8u, XSMTP_CAP_CHUNKING },
		{ "BINARYMIME", 10u, XSMTP_CAP_BINARYMIME },
		{ "ENHANCEDSTATUSCODES", 19u, XSMTP_CAP_ENHANCED_STATUS }
	};

	if ( !__xrtMailViewValid(Name) ) {
		return 0;
	}
	for ( size_t i = 0; i < sizeof(arrCapabilities) /
		sizeof(arrCapabilities[0]); i++ ) {
		if ( __xrtMailAsciiEqualI(
			Name,
			__xrtMailView(arrCapabilities[i].Name, arrCapabilities[i].Size)
		) ) {
			return arrCapabilities[i].Value;
		}
	}
	return 0;
}



/* 解析无符号十进制 SIZE 参数。 */
static bool __xrtSmtpSize(xstrview Text, uint64* pValue)
{
	uint64 iValue = 0;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ||
			 (iValue > ((UINT64_MAX - (uint64)(iByte - (unsigned char)'0')) /
			  UINT64_C(10))) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) +
			(uint64)(iByte - (unsigned char)'0');
	}
	*pValue = iValue;
	return true;
}



/* 把 AUTH 参数中的机制合并到能力位集。 */
static bool __xrtSmtpAuthAdd(xstrview Parameters, uint64* pCapabilities)
{
	size_t iPosition = 0;

	while ( iPosition < Parameters.Size ) {
		size_t iStart;
		xstrview Mechanism;

		while ( (iPosition < Parameters.Size) &&
			((Parameters.Data[iPosition] == ' ') ||
			 (Parameters.Data[iPosition] == '\t')) ) {
			iPosition++;
		}
		iStart = iPosition;
		while ( (iPosition < Parameters.Size) &&
			(Parameters.Data[iPosition] != ' ') &&
			(Parameters.Data[iPosition] != '\t') ) {
			iPosition++;
		}
		Mechanism = __xrtMailSlice(Parameters, iStart, iPosition - iStart);
		if ( __xrtMailAsciiEqualI(Mechanism, XRT_STR_LITERAL("PLAIN")) ) {
			*pCapabilities |= XSMTP_CAP_AUTH_PLAIN;
		} else if ( __xrtMailAsciiEqualI(
			Mechanism,
			XRT_STR_LITERAL("LOGIN")
		) ) {
			*pCapabilities |= XSMTP_CAP_AUTH_LOGIN;
		} else if ( __xrtMailAsciiEqualI(
			Mechanism,
			XRT_STR_LITERAL("XOAUTH2")
		) ) {
			*pCapabilities |= XSMTP_CAP_AUTH_XOAUTH2;
		} else if ( __xrtMailAsciiEqualI(
			Mechanism,
			XRT_STR_LITERAL("OAUTHBEARER")
		) ) {
			*pCapabilities |= XSMTP_CAP_AUTH_OAUTHBEARER;
		}
	}
	return true;
}



/* 合并 EHLO 能力。 */
XRT_API bool xrtSmtpCapabilityAdd(
	const xsmtpcapabilityview* pCapability,
	uint64* pCapabilities,
	uint64* pSizeLimit
)
{
	uint64 iCapabilities;
	uint64 iSizeLimit;
	uint64 iCapability;

	if ( !xrtMemRangeValid(
		pCapability,
		pCapability != NULL ? sizeof(*pCapability) : 0
	) || (pCapability == NULL) ||
		 !__xrtMailViewValid(pCapability->Name) ||
		 !__xrtMailViewValid(pCapability->Parameters) ||
		 !xrtMemRangeValid(pCapabilities, sizeof(*pCapabilities)) ||
		 !xrtMemRangeValid(pSizeLimit, sizeof(*pSizeLimit)) ||
		 xrtMemRangesOverlap(pCapabilities, sizeof(*pCapabilities),
			pSizeLimit, sizeof(*pSizeLimit)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iCapabilities = *pCapabilities;
	iSizeLimit = *pSizeLimit;
	iCapability = xrtSmtpCapability(pCapability->Name);
	if ( __xrtMailAsciiEqualI(pCapability->Name, XRT_STR_LITERAL("AUTH")) ) {
		if ( !__xrtSmtpAuthAdd(pCapability->Parameters, &iCapabilities) ) {
			return false;
		}
	} else if ( iCapability == XSMTP_CAP_SIZE ) {
		if ( (pCapability->Parameters.Size != 0) &&
			 !__xrtSmtpSize(pCapability->Parameters, &iSizeLimit) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid SMTP SIZE capability"
			);
			return false;
		}
		iCapabilities |= XSMTP_CAP_SIZE;
	} else {
		iCapabilities |= iCapability;
	}
	*pCapabilities = iCapabilities;
	*pSizeLimit = iSizeLimit;
	return true;
}



/* 验证 SMTP 命令名称与参数。 */
static bool __xrtSmtpCommandValid(xstrview Verb, xstrview Arguments)
{
	if ( (Verb.Size == 0) || (Verb.Size > 32u) ) {
		return false;
	}
	for ( size_t i = 0; i < Verb.Size; i++ ) {
		if ( !__xrtSmtpVerbByte((unsigned char)Verb.Data[i]) ) {
			return false;
		}
	}
	for ( size_t i = 0; i < Arguments.Size; i++ ) {
		unsigned char iByte = (unsigned char)Arguments.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 写出经过控制字符检查的 SMTP 命令。 */
XRT_API bool xrtSmtpCommandWrite(
	xstrview Verb,
	xstrview Arguments,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iSeparator = Arguments.Size != 0 ? 1u : 0;

	if ( !__xrtMailViewValid(Verb) || !__xrtMailViewValid(Arguments) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtSmtpCommandValid(Verb, Arguments) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid SMTP command"
		);
		return false;
	}
	if ( !__xrtMailSizeAdd(Verb.Size, iSeparator, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, Arguments.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ) {
		return false;
	}
	if ( iRequired > XSMTP_COMMAND_MAX ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"SMTP command exceeds 512 bytes"
		);
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Verb.Data,
			Verb.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Arguments.Data,
			Arguments.Size) ||
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Verb.Data, Verb.Size) ||
		 xrtMemRangesOverlap(sOutput, iRequired + 1u, Arguments.Data,
			Arguments.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	memcpy(sOutput, Verb.Data, Verb.Size);
	if ( iSeparator != 0 ) {
		sOutput[Verb.Size] = ' ';
		memcpy(sOutput + Verb.Size + 1u, Arguments.Data, Arguments.Size);
	}
	sOutput[iRequired - 2u] = '\r';
	sOutput[iRequired - 1u] = '\n';
	sOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 分配并写出 SMTP 命令。 */
XRT_API str xrtSmtpCommand(
	xstrview Verb,
	xstrview Arguments,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtSmtpCommandWrite(
		Verb,
		Arguments,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtSmtpCommandWrite(
		Verb,
		Arguments,
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
