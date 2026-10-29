#include <string.h>

#include <xrt/ssh_auth_keyboard.h>



#if defined(XSSH_FEATURE_AUTH_KEYBOARD)

/* 比较借用文本与固定协议文本。 */
static bool xsshKeyboardTextEqual(
	xstrview Text,
	const char* sValue,
	size_t iSize
)
{
	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 将一个 SSH string 的编码长度加入总长度。 */
static xsshcode xsshKeyboardAddString(xbytesview Value, size_t* pTotal)
{
	if ( (pTotal == NULL) ||
		!xrtMemRangeValid(Value.Data, Value.Size) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 校验 UTF-8 文本并加入 SSH string 编码长度。 */
static xsshcode xsshKeyboardAddText(
	xstrview Text,
	bool bNonempty,
	size_t* pTotal
)
{
	if ( (bNonempty && (Text.Size == 0u)) ||
		!xrtUtf8Valid(Text, NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshKeyboardAddString(
		(xbytesview){ (const unsigned char*)Text.Data, Text.Size },
		pTotal
	);
}



/* 校验空值或逗号分隔的 keyboard-interactive 子方法列表。 */
static bool xsshKeyboardSubmethodsValid(xstrview Submethods)
{
	return ((Submethods.Size == 0u) &&
		xrtMemRangeValid(Submethods.Data, Submethods.Size)) ||
		xrtSshNameListValid(Submethods);
}



/* 写入带显式 language tag 的 keyboard-interactive 请求。 */
xsshcode xrtSshAuthKeyboardWriteLanguage(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Language,
	xstrview Submethods
)
{
	xbytesview arrInputs[3];
	xsshwriter Writer;
	size_t iFieldsSize = 0u;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = (xbytesview){
		(const unsigned char*)User.Data,
		User.Size
	};
	arrInputs[1] = (xbytesview){
		(const unsigned char*)Language.Data,
		Language.Size
	};
	arrInputs[2] = (xbytesview){
		(const unsigned char*)Submethods.Data,
		Submethods.Size
	};
	if ( !xrtSshLanguageValid(Language) ||
		!xsshKeyboardSubmethodsValid(Submethods) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshKeyboardAddString(
		arrInputs[1],
		&iFieldsSize
	)) != XSSH_OK) || ((Code = xsshKeyboardAddString(
		arrInputs[2],
		&iFieldsSize
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE),
		iFieldsSize,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, arrInputs, 3u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	Code = xrtSshAuthRequestWrite(
		&Writer,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE),
		(xbytesview){ NULL, 0u }
	);
	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	if ( (xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[2]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 使用规范建议的空 language tag 写入请求。 */
xsshcode xrtSshAuthKeyboardWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Submethods
)
{
	return xrtSshAuthKeyboardWriteLanguage(
		pWriter,
		User,
		XRT_STR_LITERAL(""),
		Submethods
	);
}



/* 严格读取 keyboard-interactive 请求。 */
xsshcode xrtSshAuthKeyboardRead(
	xbytesview Payload,
	xsshauthkeyboard* pKeyboard
)
{
	xsshauthrequest Request;
	xsshauthkeyboard Keyboard;
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	if ( pKeyboard == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshKeyboardTextEqual(
		Request.Service,
		XSSH_SERVICE_CONNECTION,
		sizeof(XSSH_SERVICE_CONNECTION) - 1u
	) || !xsshKeyboardTextEqual(
		Request.Method,
		XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE,
		sizeof(XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE) - 1u
	) || !xrtSshReaderInit(&Reader, Request.Fields) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Keyboard.User = Request.User;
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Keyboard.Language = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Keyboard.Submethods = (xstrview){ (const char*)Value.Data, Value.Size };
	if ( !xrtSshLanguageValid(Keyboard.Language) ||
		!xsshKeyboardSubmethodsValid(Keyboard.Submethods) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pKeyboard = Keyboard;
	return XSSH_OK;
}



/* 写入任意数量的 keyboard-interactive 提示。 */
xsshcode xrtSshAuthKeyboardChallengeWrite(
	xsshwriter* pWriter,
	xstrview Name,
	xstrview Instruction,
	xstrview Language,
	const xsshauthkeyboardprompt* pPrompts,
	size_t iCount
)
{
	xbytesview arrInputs[4];
	xbytesview Input;
	xsshwriter Writer;
	size_t iPromptsSize;
	size_t iTotal = 1u + 4u;
	size_t i;
	xsshcode Code;

	if ( (iCount > UINT32_MAX) ||
		(iCount > (SIZE_MAX / sizeof(*pPrompts))) ||
		((pPrompts == NULL) && (iCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iPromptsSize = iCount * sizeof(*pPrompts);
	if ( !xrtMemRangeValid(pPrompts, iPromptsSize) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrInputs[0] = (xbytesview){
		(const unsigned char*)Name.Data,
		Name.Size
	};
	arrInputs[1] = (xbytesview){
		(const unsigned char*)Instruction.Data,
		Instruction.Size
	};
	arrInputs[2] = (xbytesview){
		(const unsigned char*)Language.Data,
		Language.Size
	};
	arrInputs[3] = (xbytesview){
		(const unsigned char*)pPrompts,
		iPromptsSize
	};
	if ( ((Code = xsshKeyboardAddText(Name, false, &iTotal)) != XSSH_OK) ||
		((Code = xsshKeyboardAddText(
			Instruction,
			false,
			&iTotal
		)) != XSSH_OK) ) {
		return Code;
	}
	if ( !xrtSshLanguageValid(Language) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyboardAddString(arrInputs[2], &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Code = xsshKeyboardAddText(pPrompts[i].Prompt, true, &iTotal);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( iTotal == SIZE_MAX ) {
			return XSSH_ERROR_OVERFLOW;
		}
		++iTotal;
	}
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, arrInputs, 4u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Input = (xbytesview){
			(const unsigned char*)pPrompts[i].Prompt.Data,
			pPrompts[i].Prompt.Size
		};
		Code = xrtSshWriterReserveInputs(pWriter, iTotal, &Input, 1u);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_USERAUTH_INFO_REQUEST
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[0]
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[1]
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[2]
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		(uint32)iCount
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Input = (xbytesview){
			(const unsigned char*)pPrompts[i].Prompt.Data,
			pPrompts[i].Prompt.Size
		};
		if ( (xrtSshWriteString(&Writer, Input) != XSSH_OK) ||
			(xrtSshWriteBool(&Writer, pPrompts[i].Echo) != XSSH_OK) ) {
			return XSSH_ERROR_STATE;
		}
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 预验证完整 challenge 后初始化无分配提示迭代器。 */
xsshcode xrtSshAuthKeyboardChallengeRead(
	xbytesview Payload,
	xsshauthkeyboardchallenge* pChallenge
)
{
	xsshauthkeyboardchallenge Challenge;
	xsshreader Reader;
	xsshreader Prompts;
	xbytesview Value;
	bool bEcho;
	uint8 iMessage;
	uint32 i;
	xsshcode Code;

	if ( (pChallenge == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_USERAUTH_INFO_REQUEST ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Challenge.Name = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Challenge.Instruction = (xstrview){
		(const char*)Value.Data,
		Value.Size
	};
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Challenge.Language = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadU32(&Reader, &Challenge.Count);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtUtf8Valid(Challenge.Name, NULL) ||
		!xrtUtf8Valid(Challenge.Instruction, NULL) ||
		!xrtSshLanguageValid(Challenge.Language) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( (size_t)Challenge.Count >
		(xrtSshReaderRemaining(&Reader) / 5u) ) {
		return XSSH_NEED_MORE;
	}
	Prompts = Reader;
	for ( i = 0u; i < Challenge.Count; ++i ) {
		Code = xrtSshReadString(&Reader, &Value);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( (Value.Size == 0u) || !xrtUtf8Valid(
			(xstrview){ (const char*)Value.Data, Value.Size },
			NULL
		) ) {
			return XSSH_ERROR_PROTOCOL;
		}
		Code = xrtSshReadBool(&Reader, &bEcho);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Challenge.Index = 0u;
	Challenge.Reader = Prompts;
	*pChallenge = Challenge;
	return XSSH_OK;
}



/* 从已验证 challenge 中返回下一项提示。 */
bool xrtSshAuthKeyboardChallengeNext(
	xsshauthkeyboardchallenge* pChallenge,
	xsshauthkeyboardprompt* pPrompt
)
{
	xsshauthkeyboardchallenge Challenge;
	xsshauthkeyboardprompt Prompt;
	xbytesview Value;

	if ( (pChallenge == NULL) || (pPrompt == NULL) ||
		(pChallenge->Index >= pChallenge->Count) ) {
		return false;
	}
	Challenge = *pChallenge;
	if ( (xrtSshReadString(&Challenge.Reader, &Value) != XSSH_OK) ||
		(xrtSshReadBool(&Challenge.Reader, &Prompt.Echo) != XSSH_OK) ||
		(Value.Size == 0u) ) {
		return false;
	}
	Prompt.Prompt = (xstrview){ (const char*)Value.Data, Value.Size };
	if ( !xrtUtf8Valid(Prompt.Prompt, NULL) ) {
		return false;
	}
	++Challenge.Index;
	*pChallenge = Challenge;
	*pPrompt = Prompt;
	return true;
}



/* 写入任意数量的 keyboard-interactive 响应。 */
xsshcode xrtSshAuthKeyboardResponseWrite(
	xsshwriter* pWriter,
	const xstrview* pResponses,
	size_t iCount
)
{
	xbytesview Inputs;
	xbytesview Input;
	xsshwriter Writer;
	size_t iResponsesSize;
	size_t iTotal = 1u + 4u;
	size_t i;
	xsshcode Code;

	if ( (iCount > UINT32_MAX) ||
		(iCount > (SIZE_MAX / sizeof(*pResponses))) ||
		((pResponses == NULL) && (iCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iResponsesSize = iCount * sizeof(*pResponses);
	if ( !xrtMemRangeValid(pResponses, iResponsesSize) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Inputs = (xbytesview){
		(const unsigned char*)pResponses,
		iResponsesSize
	};
	for ( i = 0u; i < iCount; ++i ) {
		Code = xsshKeyboardAddText(pResponses[i], false, &iTotal);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, &Inputs, 1u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Input = (xbytesview){
			(const unsigned char*)pResponses[i].Data,
			pResponses[i].Size
		};
		Code = xrtSshWriterReserveInputs(pWriter, iTotal, &Input, 1u);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_USERAUTH_INFO_RESPONSE
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		(uint32)iCount
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Input = (xbytesview){
			(const unsigned char*)pResponses[i].Data,
			pResponses[i].Size
		};
		if ( xrtSshWriteString(&Writer, Input) != XSSH_OK ) {
			return XSSH_ERROR_STATE;
		}
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 预验证完整 response 后初始化无分配响应迭代器。 */
xsshcode xrtSshAuthKeyboardResponseRead(
	xbytesview Payload,
	xsshauthkeyboardresponses* pResponses
)
{
	xsshauthkeyboardresponses Responses;
	xsshreader Reader;
	xsshreader Items;
	xbytesview Value;
	uint8 iMessage;
	uint32 i;
	xsshcode Code;

	if ( (pResponses == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_USERAUTH_INFO_RESPONSE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadU32(&Reader, &Responses.Count);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (size_t)Responses.Count >
		(xrtSshReaderRemaining(&Reader) / 4u) ) {
		return XSSH_NEED_MORE;
	}
	Items = Reader;
	for ( i = 0u; i < Responses.Count; ++i ) {
		Code = xrtSshReadString(&Reader, &Value);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( !xrtUtf8Valid(
			(xstrview){ (const char*)Value.Data, Value.Size },
			NULL
		) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Responses.Index = 0u;
	Responses.Reader = Items;
	*pResponses = Responses;
	return XSSH_OK;
}



/* 从已验证 response 中返回下一项响应。 */
bool xrtSshAuthKeyboardResponseNext(
	xsshauthkeyboardresponses* pResponses,
	xstrview* pResponse
)
{
	xsshauthkeyboardresponses Responses;
	xstrview Response;
	xbytesview Value;

	if ( (pResponses == NULL) || (pResponse == NULL) ||
		(pResponses->Index >= pResponses->Count) ) {
		return false;
	}
	Responses = *pResponses;
	if ( xrtSshReadString(&Responses.Reader, &Value) != XSSH_OK ) {
		return false;
	}
	Response = (xstrview){ (const char*)Value.Data, Value.Size };
	if ( !xrtUtf8Valid(Response, NULL) ) {
		return false;
	}
	++Responses.Index;
	*pResponses = Responses;
	*pResponse = Response;
	return true;
}

#endif
