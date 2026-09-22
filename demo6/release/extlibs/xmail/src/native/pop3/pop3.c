#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_POP3)

/* 跳过 POP3 token 之间的空格和制表符。 */
static size_t __xrtPop3Space(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) &&
		 ((Text.Data[iPosition] == ' ') || (Text.Data[iPosition] == '\t')) ) {
		iPosition++;
	}
	return iPosition;
}



/* 严格读取一个十进制 uint64 token。 */
static bool __xrtPop3Uint64(
	xstrview Text,
	size_t* pPosition,
	uint64* pValue
)
{
	size_t iPosition = *pPosition;
	uint64 iValue = 0;
	size_t iDigits = 0;

	while ( iPosition < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[iPosition];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ) {
			break;
		}
		if ( iValue > ((UINT64_MAX -
			(uint64)(iByte - (unsigned char)'0')) / UINT64_C(10)) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) +
			(uint64)(iByte - (unsigned char)'0');
		iPosition++;
		iDigits++;
	}
	if ( iDigits == 0 ) {
		return false;
	}
	*pPosition = iPosition;
	*pValue = iValue;
	return true;
}



/* 解析 POP3 状态行。 */
XRT_API bool xrtPop3ReplyParse(xstrview Line, xpop3replyview* pReply)
{
	xpop3replyview Reply;
	size_t iPrefix;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		 xrtMemRangesOverlap(pReply, sizeof(*pReply), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (Line.Size >= 3u) &&
		 (memcmp(Line.Data, "+OK", 3u) == 0) ) {
		Reply.Ok = true;
		iPrefix = 3u;
	} else if ( (Line.Size >= 4u) &&
		 (memcmp(Line.Data, "-ERR", 4u) == 0) ) {
		Reply.Ok = false;
		iPrefix = 4u;
	} else {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 status line"
		);
		return false;
	}
	if ( (iPrefix < Line.Size) && (Line.Data[iPrefix] != ' ') &&
		 (Line.Data[iPrefix] != '\t') ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 status separator"
		);
		return false;
	}
	for ( size_t i = iPrefix; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			 (iByte == (unsigned char)'\n') ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"POP3 status contains a control character"
			);
			return false;
		}
	}
	iPrefix = __xrtPop3Space(Line, iPrefix);
	Reply.Source = Line;
	Reply.Text = __xrtMailSlice(Line, iPrefix, Line.Size - iPrefix);
	*pReply = Reply;
	return true;
}



/* 解析两个无符号十进制 token，并要求行尾无额外内容。 */
static bool __xrtPop3Pair(
	xstrview Text,
	uint64* pFirst,
	uint64* pSecond
)
{
	size_t iPosition = __xrtPop3Space(Text, 0);
	uint64 iFirst;
	uint64 iSecond;

	if ( !__xrtPop3Uint64(Text, &iPosition, &iFirst) ) {
		return false;
	}
	if ( (iPosition == Text.Size) ||
		 ((Text.Data[iPosition] != ' ') && (Text.Data[iPosition] != '\t')) ) {
		return false;
	}
	iPosition = __xrtPop3Space(Text, iPosition);
	if ( !__xrtPop3Uint64(Text, &iPosition, &iSecond) ) {
		return false;
	}
	iPosition = __xrtPop3Space(Text, iPosition);
	if ( iPosition != Text.Size ) {
		return false;
	}
	*pFirst = iFirst;
	*pSecond = iSecond;
	return true;
}



/* 解析 STAT 成功响应。 */
XRT_API bool xrtPop3StatParse(xstrview Line, xpop3stat* pStat)
{
	xpop3replyview Reply;
	xpop3stat Stat;

	if ( !xrtMemRangeValid(pStat, sizeof(*pStat)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ReplyParse(Line, &Reply) ) {
		return false;
	}
	if ( !Reply.Ok ||
		 !__xrtPop3Pair(Reply.Text, &Stat.Messages, &Stat.Bytes) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 STAT response"
		);
		return false;
	}
	*pStat = Stat;
	return true;
}



/* 解析 LIST 响应项。 */
XRT_API bool xrtPop3ListParse(xstrview Line, xpop3listview* pItem)
{
	xpop3listview Item;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		 xrtMemRangesOverlap(pItem, sizeof(*pItem), Line.Data, Line.Size) ||
		 !__xrtPop3Pair(Line, &Item.Message, &Item.Bytes) ||
		 (Item.Message == 0) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 LIST item"
		);
		return false;
	}
	*pItem = Item;
	return true;
}



/* 解析 UIDL 响应项。 */
XRT_API bool xrtPop3UidlParse(xstrview Line, xpop3uidlview* pItem)
{
	xpop3uidlview Item;
	size_t iPosition;
	size_t iStart;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		 xrtMemRangesOverlap(pItem, sizeof(*pItem), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iPosition = __xrtPop3Space(Line, 0);
	if ( !__xrtPop3Uint64(Line, &iPosition, &Item.Message) ||
		 (Item.Message == 0) || (iPosition == Line.Size) ||
		 ((Line.Data[iPosition] != ' ') && (Line.Data[iPosition] != '\t')) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 UIDL item"
		);
		return false;
	}
	iPosition = __xrtPop3Space(Line, iPosition);
	iStart = iPosition;
	while ( iPosition < Line.Size ) {
		unsigned char iByte = (unsigned char)Line.Data[iPosition];

		if ( (iByte <= 32u) || (iByte >= 127u) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid POP3 unique ID"
			);
			return false;
		}
		iPosition++;
	}
	if ( iPosition == iStart ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"empty POP3 unique ID"
		);
		return false;
	}
	Item.Id = __xrtMailSlice(Line, iStart, iPosition - iStart);
	*pItem = Item;
	return true;
}



/* 解析 CAPA 行名称和参数。 */
XRT_API bool xrtPop3CapabilityParse(
	xstrview Line,
	xpop3capabilityview* pCapability
)
{
	xpop3capabilityview Capability;
	size_t iPosition = 0;
	size_t iParameters;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pCapability, sizeof(*pCapability)) ||
		 xrtMemRangesOverlap(pCapability, sizeof(*pCapability), Line.Data,
			Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	while ( (iPosition < Line.Size) &&
		 (Line.Data[iPosition] != ' ') && (Line.Data[iPosition] != '\t') ) {
		unsigned char iByte = (unsigned char)Line.Data[iPosition];

		if ( !(((iByte >= (unsigned char)'A') &&
			 (iByte <= (unsigned char)'Z')) ||
			((iByte >= (unsigned char)'a') &&
			 (iByte <= (unsigned char)'z')) ||
			(iByte == (unsigned char)'-')) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid POP3 capability name"
			);
			return false;
		}
		iPosition++;
	}
	if ( iPosition == 0 ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"empty POP3 capability name"
		);
		return false;
	}
	iParameters = __xrtPop3Space(Line, iPosition);
	for ( size_t i = iParameters; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"POP3 capability contains a control character"
			);
			return false;
		}
	}
	Capability.Source = Line;
	Capability.Name = __xrtMailSlice(Line, 0, iPosition);
	Capability.Parameters = __xrtMailSlice(
		Line,
		iParameters,
		Line.Size - iParameters
	);
	*pCapability = Capability;
	return true;
}



/* 查找 POP3 能力名称的稳定标记。 */
XRT_API uint32 xrtPop3Capability(xstrview Name)
{
	static const struct {
		cstr Name;
		size_t Size;
		uint32 Value;
	} arrCapabilities[] = {
		{ "TOP", 3u, XPOP3_CAP_TOP },
		{ "USER", 4u, XPOP3_CAP_USER },
		{ "SASL", 4u, XPOP3_CAP_SASL },
		{ "RESP-CODES", 10u, XPOP3_CAP_RESP_CODES },
		{ "LOGIN-DELAY", 11u, XPOP3_CAP_LOGIN_DELAY },
		{ "PIPELINING", 10u, XPOP3_CAP_PIPELINING },
		{ "EXPIRE", 6u, XPOP3_CAP_EXPIRE },
		{ "UIDL", 4u, XPOP3_CAP_UIDL },
		{ "IMPLEMENTATION", 14u, XPOP3_CAP_IMPLEMENTATION },
		{ "STLS", 4u, XPOP3_CAP_STLS }
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



/* 查找 POP3 SASL 机制名称的稳定标记。 */
XRT_API uint32 xrtPop3SaslMechanism(xstrview Name)
{
	static const struct {
		cstr Name;
		size_t Size;
		uint32 Value;
	} arrMechanisms[] = {
		{ "PLAIN", 5u, XPOP3_SASL_PLAIN },
		{ "XOAUTH2", 7u, XPOP3_SASL_XOAUTH2 },
		{ "OAUTHBEARER", 11u, XPOP3_SASL_OAUTHBEARER }
	};

	if ( !__xrtMailViewValid(Name) ) {
		return 0;
	}
	for ( size_t i = 0; i < sizeof(arrMechanisms) /
		sizeof(arrMechanisms[0]); i++ ) {
		if ( __xrtMailAsciiEqualI(
			Name,
			__xrtMailView(arrMechanisms[i].Name, arrMechanisms[i].Size)
		) ) {
			return arrMechanisms[i].Value;
		}
	}
	return 0;
}



/* 验证 POP3 命令名称和参数。 */
static bool __xrtPop3CommandValid(xstrview Verb, xstrview Arguments)
{
	if ( (Verb.Size == 0) || (Verb.Size > 16u) ) {
		return false;
	}
	for ( size_t i = 0; i < Verb.Size; i++ ) {
		unsigned char iByte = (unsigned char)Verb.Data[i];

		if ( !((iByte >= (unsigned char)'A') &&
			(iByte <= (unsigned char)'Z')) ) {
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



/* 写出安全 POP3 命令。 */
XRT_API bool xrtPop3CommandWrite(
	xstrview Verb,
	xstrview Arguments,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iSeparator = Arguments.Size != 0 ? 1u : 0;
	size_t iRequired;

	if ( !__xrtMailViewValid(Verb) || !__xrtMailViewValid(Arguments) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtPop3CommandValid(Verb, Arguments) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 command"
		);
		return false;
	}
	if ( !__xrtMailSizeAdd(Verb.Size, iSeparator, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, Arguments.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ) {
		return false;
	}
	if ( iRequired > XPOP3_COMMAND_MAX ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"POP3 command exceeds 512 bytes"
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



/* 分配并写出 POP3 命令。 */
XRT_API str xrtPop3Command(
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
	) || !xrtPop3CommandWrite(
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
	if ( !xrtPop3CommandWrite(
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
