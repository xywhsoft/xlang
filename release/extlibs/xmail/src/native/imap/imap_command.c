#include <xrt/imap_command.h>

#include "../internal/xrt_imap_client.h"
#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_IMAP_COMMAND)

/* 设置命令层稳定错误。 */
static bool __xrtImapCommandError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 验证必须存在的单行命令参数。 */
static bool __xrtImapCommandRequired(xstrview Text, cstr sMessage)
{
	if ( !__xrtMailViewValid(Text) || (Text.Size == 0) ) {
		return __xrtImapCommandError(XERR_ARGUMENT, sMessage);
	}
	return true;
}



/* 验证序号或 UID 集合并设置明确错误。 */
static bool __xrtImapCommandSet(xstrview Set)
{
	if ( !xrtImapSequenceSetValid(Set) ) {
		return __xrtImapCommandError(
			XERR_ARGUMENT,
			"invalid IMAP sequence set"
		);
	}
	return true;
}



/* 验证命令可在认证态或选中态执行。 */
static bool __xrtImapCommandAuthenticated(ximapclient* pClient)
{
	ximapclientstate State;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	State = xrtImapClientState(pClient);
	if ( (State != XIMAP_CLIENT_AUTHENTICATED) &&
		(State != XIMAP_CLIENT_SELECTED) ) {
		return __xrtImapCommandError(
			XERR_STATE,
			"IMAP command requires an authenticated session"
		);
	}
	return true;
}



/* 验证命令只能在选中邮箱上执行。 */
static bool __xrtImapCommandSelected(ximapclient* pClient)
{
	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( xrtImapClientState(pClient) != XIMAP_CLIENT_SELECTED ) {
		return __xrtImapCommandError(
			XERR_STATE,
			"IMAP command requires a selected mailbox"
		);
	}
	return true;
}



/* 以客户端零拼接路径开始多参数命令。 */
static bool __xrtImapCommandBegin(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pParts,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return xrtImapClientBeginParts(
		pClient,
		Command,
		pParts,
		iCount,
		iDeadline,
		pCancel
	);
}



/* 把邮箱名转义为 quoted string。 */
static str __xrtImapCommandMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	size_t* pSize
)
{
	size_t iRequired;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( !xrtImapQuoteWrite(Mailbox, NULL, 0, &iRequired) ) {
		return NULL;
	}
	if ( iRequired > xrtImapClientCommandLimit(pClient) ) {
		(void)__xrtImapCommandError(
			XERR_RANGE,
			"IMAP mailbox name exceeds the command line limit"
		);
		return NULL;
	}
	return xrtImapQuote(Mailbox, pSize);
}



/* 开始带一个 quoted mailbox 参数的命令。 */
static bool __xrtImapCommandBeginMailbox(
	ximapclient* pClient,
	xstrview Command,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Part;
	str sMailbox;
	bool bSuccess;

	sMailbox = __xrtImapCommandMailbox(pClient, Mailbox, &Part.Size);
	if ( sMailbox == NULL ) {
		return false;
	}
	Part.Data = sMailbox;
	bSuccess = __xrtImapCommandBegin(
		pClient,
		Command,
		&Part,
		1u,
		iDeadline,
		pCancel
	);
	xrtFree(sMailbox);
	return bSuccess;
}



/* 把一个纯数字响应码参数转换为 uint64。 */
static bool __xrtImapCommandNumber(xstrview Text, uint64* pValue)
{
	uint64 iValue = 0;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint64 iDigit;

		if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
			return false;
		}
		iDigit = (uint64)(Text.Data[i] - '0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
	}
	*pValue = iValue;
	return true;
}



/* 合并 SELECT/EXAMINE 的方括号响应码。 */
static xmailnext __xrtImapCommandMailboxCode(
	xstrview Text,
	ximapmailboxinfo* pInfo
)
{
	ximapcodeview Code;
	xmailnext Next = xrtImapCodeParse(Text, &Code);
	uint64 iValue;

	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( __xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("READ-ONLY")) ||
		__xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("READ-WRITE")) ) {
		if ( Code.Arguments.Size != 0 ) {
			(void)__xrtImapCommandError(
				XERR_PROTOCOL,
				"invalid IMAP mailbox access response code"
			);
			return XMAIL_NEXT_ERROR;
		}
		pInfo->ReadOnly = __xrtMailAsciiEqualI(
			Code.Name,
			XRT_STR_LITERAL("READ-ONLY")
		);
		pInfo->Present |= XIMAP_MAILBOX_ACCESS;
		return XMAIL_NEXT_ITEM;
	}
	if ( !__xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("UNSEEN")) &&
		!__xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("UIDVALIDITY")) &&
		!__xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("UIDNEXT")) &&
		!__xrtMailAsciiEqualI(
			Code.Name,
			XRT_STR_LITERAL("HIGHESTMODSEQ")
		) ) {
		return XMAIL_NEXT_END;
	}
	if ( !__xrtImapCommandNumber(Code.Arguments, &iValue) ) {
		(void)__xrtImapCommandError(
			XERR_PROTOCOL,
			"invalid numeric IMAP mailbox response code"
		);
		return XMAIL_NEXT_ERROR;
	}
	if ( __xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("UNSEEN")) ) {
		pInfo->Unseen = iValue;
		pInfo->Present |= XIMAP_MAILBOX_UNSEEN;
	} else if ( __xrtMailAsciiEqualI(
		Code.Name,
		XRT_STR_LITERAL("UIDVALIDITY")
	) ) {
		pInfo->UidValidity = iValue;
		pInfo->Present |= XIMAP_MAILBOX_UID_VALIDITY;
	} else if ( __xrtMailAsciiEqualI(
		Code.Name,
		XRT_STR_LITERAL("UIDNEXT")
	) ) {
		pInfo->UidNext = iValue;
		pInfo->Present |= XIMAP_MAILBOX_UID_NEXT;
	} else if ( __xrtMailAsciiEqualI(
		Code.Name,
		XRT_STR_LITERAL("HIGHESTMODSEQ")
	) ) {
		pInfo->HighestModSeq = iValue;
		pInfo->Present |= XIMAP_MAILBOX_HIGHEST_MODSEQ;
	} else {
		return XMAIL_NEXT_END;
	}
	return XMAIL_NEXT_ITEM;
}



/* 消费一个不允许 literal 的命令并返回最终状态。 */
static bool __xrtImapCommandFinish(
	ximapclient* pClient,
	ximapmailboxinfo* pInfo,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return false;
			}
			if ( (pInfo != NULL) &&
				(xrtImapMailboxInfoUpdate(&Final, pInfo) ==
				 XMAIL_NEXT_ERROR) ) {
				return __xrtImapClientProtocolFail(
					pClient,
					"invalid IMAP mailbox response code"
				);
			}
			if ( Final.Status == XIMAP_STATUS_OK ) {
				return true;
			}
			return __xrtImapCommandError(
				Final.Status == XIMAP_STATUS_NO ?
					XERR_PERMISSION : XERR_PROTOCOL,
				"IMAP command was rejected"
			);
		}
		if ( Event.HasLiteral ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"unexpected literal in an IMAP control command"
			);
		}
		if ( (pInfo != NULL) &&
			(Event.Kind == XIMAP_EVENT_RESPONSE) &&
			(xrtImapMailboxInfoUpdate(&Event.Response, pInfo) ==
			 XMAIL_NEXT_ERROR) ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"invalid IMAP mailbox status response"
			);
		}
	}
}



/* 执行无参数并且没有 literal 结果的命令。 */
static bool __xrtImapCommandSimple(
	ximapclient* pClient,
	xstrview Command,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return xrtImapClientBegin(
		pClient,
		Command,
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) && __xrtImapCommandFinish(
		pClient,
		NULL,
		iDeadline,
		pCancel
	);
}



/* 执行带一个 quoted mailbox 的简单命令。 */
static bool __xrtImapCommandMailboxSimple(
	ximapclient* pClient,
	xstrview Command,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandAuthenticated(pClient) &&
		__xrtImapCommandBeginMailbox(
			pClient,
			Command,
			Mailbox,
			iDeadline,
			pCancel
		) && __xrtImapCommandFinish(
			pClient,
			NULL,
			iDeadline,
			pCancel
		);
}



/* 初始化空邮箱摘要。 */
XRT_API void xrtImapMailboxInfoInit(ximapmailboxinfo* pInfo)
{
	if ( !xrtMemRangeValid(pInfo, sizeof(*pInfo)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
}



/* 合并 SELECT、EXAMINE 或未请求邮箱状态响应。 */
XRT_API xmailnext xrtImapMailboxInfoUpdate(
	const ximapresponseview* pResponse,
	ximapmailboxinfo* pInfo
)
{
	ximapnumberview Number;
	xmailnext Next;

	if ( !xrtMemRangeValid(pResponse, sizeof(*pResponse)) ||
		!xrtMemRangeValid(pInfo, sizeof(*pInfo)) ||
		xrtMemRangesOverlap(
			pResponse,
			sizeof(*pResponse),
			pInfo,
			sizeof(*pInfo)
		) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (pResponse->Kind == XIMAP_RESPONSE_TAGGED) ||
		((pResponse->Kind == XIMAP_RESPONSE_UNTAGGED) &&
		 (pResponse->Status == XIMAP_STATUS_OK)) ) {
		return __xrtImapCommandMailboxCode(pResponse->Text, pInfo);
	}
	if ( (pResponse->Kind != XIMAP_RESPONSE_UNTAGGED) ||
		(pResponse->Status != XIMAP_STATUS_NONE) ) {
		return XMAIL_NEXT_END;
	}
	Next = xrtImapNumberParse(pResponse->Text, &Number);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( __xrtMailAsciiEqualI(Number.Name, XRT_STR_LITERAL("EXISTS")) ) {
		pInfo->Exists = Number.Number;
		pInfo->Present |= XIMAP_MAILBOX_EXISTS;
		return XMAIL_NEXT_ITEM;
	}
	if ( __xrtMailAsciiEqualI(Number.Name, XRT_STR_LITERAL("RECENT")) ) {
		pInfo->Recent = Number.Number;
		pInfo->Present |= XIMAP_MAILBOX_RECENT;
		return XMAIL_NEXT_ITEM;
	}
	if ( __xrtMailAsciiEqualI(Number.Name, XRT_STR_LITERAL("EXPUNGE")) ) {
		if ( ((pInfo->Present & XIMAP_MAILBOX_EXISTS) != 0) &&
			(pInfo->Exists != 0) ) {
			pInfo->Exists--;
		}
		return XMAIL_NEXT_ITEM;
	}
	return XMAIL_NEXT_END;
}



/* 执行 NOOP。 */
XRT_API bool xrtImapClientNoop(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandSimple(
		pClient,
		XRT_STR_LITERAL("NOOP"),
		iDeadline,
		pCancel
	);
}



/* 执行 SELECT 或 EXAMINE 并提交选中态。 */
static bool __xrtImapCommandSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	bool bReadOnly,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	ximapmailboxinfo Local;
	bool bWasSelected;

	if ( !__xrtImapCommandAuthenticated(pClient) ) {
		return false;
	}
	bWasSelected = xrtImapClientState(pClient) == XIMAP_CLIENT_SELECTED;
	if ( pInfo == NULL ) {
		pInfo = &Local;
	} else if ( !xrtMemRangeValid(pInfo, sizeof(*pInfo)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	xrtImapMailboxInfoInit(pInfo);
	if ( !__xrtImapCommandBeginMailbox(
		pClient,
		bReadOnly ? XRT_STR_LITERAL("EXAMINE") :
			XRT_STR_LITERAL("SELECT"),
		Mailbox,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !__xrtImapCommandFinish(
		pClient,
		pInfo,
		iDeadline,
		pCancel
	) ) {
		if ( bWasSelected &&
			(xrtImapClientState(pClient) == XIMAP_CLIENT_SELECTED) ) {
			(void)__xrtImapClientStateCommit(
				pClient,
				XIMAP_CLIENT_AUTHENTICATED
			);
		}
		return false;
	}
	if ( bReadOnly ) {
		pInfo->ReadOnly = true;
		pInfo->Present |= XIMAP_MAILBOX_ACCESS;
	}
	return __xrtImapClientStateCommit(pClient, XIMAP_CLIENT_SELECTED);
}



/* 选择可写邮箱。 */
XRT_API bool xrtImapClientSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandSelect(
		pClient,
		Mailbox,
		pInfo,
		false,
		iDeadline,
		pCancel
	);
}



/* 以只读方式选择邮箱。 */
XRT_API bool xrtImapClientExamine(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandSelect(
		pClient,
		Mailbox,
		pInfo,
		true,
		iDeadline,
		pCancel
	);
}



/* 对当前选中邮箱执行 CHECK。 */
XRT_API bool xrtImapClientCheck(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandSelected(pClient) && __xrtImapCommandSimple(
		pClient,
		XRT_STR_LITERAL("CHECK"),
		iDeadline,
		pCancel
	);
}



/* 执行 UNSELECT 并返回认证态。 */
XRT_API bool xrtImapClientUnselect(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	uint64 iCapabilities;

	if ( !__xrtImapCommandSelected(pClient) ) {
		return false;
	}
	iCapabilities = xrtImapClientCapabilities(pClient);
	if ( (iCapabilities &
		(XIMAP_CAP_UNSELECT | XIMAP_CAP_IMAP4REV2)) == 0 ) {
		return __xrtImapCommandError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise UNSELECT"
		);
	}
	return __xrtImapCommandSimple(
		pClient,
		XRT_STR_LITERAL("UNSELECT"),
		iDeadline,
		pCancel
	) && __xrtImapClientStateCommit(
		pClient,
		XIMAP_CLIENT_AUTHENTICATED
	);
}



/* 执行 CLOSE 并返回认证态。 */
XRT_API bool xrtImapClientCloseMailbox(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandSelected(pClient) && __xrtImapCommandSimple(
		pClient,
		XRT_STR_LITERAL("CLOSE"),
		iDeadline,
		pCancel
	) && __xrtImapClientStateCommit(
		pClient,
		XIMAP_CLIENT_AUTHENTICATED
	);
}



/* 创建邮箱。 */
XRT_API bool xrtImapClientCreateMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandMailboxSimple(
		pClient,
		XRT_STR_LITERAL("CREATE"),
		Mailbox,
		iDeadline,
		pCancel
	);
}



/* 删除邮箱。 */
XRT_API bool xrtImapClientDeleteMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandMailboxSimple(
		pClient,
		XRT_STR_LITERAL("DELETE"),
		Mailbox,
		iDeadline,
		pCancel
	);
}



/* 重命名邮箱。 */
XRT_API bool xrtImapClientRenameMailbox(
	ximapclient* pClient,
	xstrview Source,
	xstrview Target,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	str sSource;
	str sTarget;
	xstrview Parts[2];
	bool bSuccess;

	if ( !__xrtImapCommandAuthenticated(pClient) ) {
		return false;
	}
	sSource = __xrtImapCommandMailbox(pClient, Source, &Parts[0].Size);
	if ( sSource == NULL ) {
		return false;
	}
	sTarget = __xrtImapCommandMailbox(pClient, Target, &Parts[1].Size);
	if ( sTarget == NULL ) {
		xrtFree(sSource);
		return false;
	}
	Parts[0].Data = sSource;
	Parts[1].Data = sTarget;
	bSuccess = __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("RENAME"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
	xrtFree(sTarget);
	xrtFree(sSource);
	return bSuccess && __xrtImapCommandFinish(
		pClient,
		NULL,
		iDeadline,
		pCancel
	);
}



/* 订阅邮箱。 */
XRT_API bool xrtImapClientSubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandMailboxSimple(
		pClient,
		XRT_STR_LITERAL("SUBSCRIBE"),
		Mailbox,
		iDeadline,
		pCancel
	);
}



/* 取消订阅邮箱。 */
XRT_API bool xrtImapClientUnsubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandMailboxSimple(
		pClient,
		XRT_STR_LITERAL("UNSUBSCRIBE"),
		Mailbox,
		iDeadline,
		pCancel
	);
}



/* 开始 LIST 并保留流式响应。 */
XRT_API bool xrtImapClientBeginList(
	ximapclient* pClient,
	xstrview Reference,
	xstrview Pattern,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	str sReference;
	str sPattern;
	xstrview Parts[2];
	bool bSuccess;

	if ( !__xrtImapCommandAuthenticated(pClient) ) {
		return false;
	}
	sReference = __xrtImapCommandMailbox(
		pClient,
		Reference,
		&Parts[0].Size
	);
	if ( sReference == NULL ) {
		return false;
	}
	sPattern = __xrtImapCommandMailbox(pClient, Pattern, &Parts[1].Size);
	if ( sPattern == NULL ) {
		xrtFree(sReference);
		return false;
	}
	Parts[0].Data = sReference;
	Parts[1].Data = sPattern;
	bSuccess = __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("LIST"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
	xrtFree(sPattern);
	xrtFree(sReference);
	return bSuccess;
}



/* 开始 STATUS；空 Items 使用常见状态集合。 */
XRT_API bool xrtImapClientBeginStatus(
	ximapclient* pClient,
	xstrview Mailbox,
	xstrview Items,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	str sMailbox;
	xstrview Parts[2];
	bool bSuccess;

	if ( !__xrtImapCommandAuthenticated(pClient) ) {
		return false;
	}
	if ( !__xrtMailViewValid(Items) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	sMailbox = __xrtImapCommandMailbox(pClient, Mailbox, &Parts[0].Size);
	if ( sMailbox == NULL ) {
		return false;
	}
	Parts[0].Data = sMailbox;
	Parts[1] = Items.Size != 0 ? Items : XRT_STR_LITERAL(
		"(MESSAGES UNSEEN UIDNEXT UIDVALIDITY)"
	);
	bSuccess = __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("STATUS"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
	xrtFree(sMailbox);
	return bSuccess;
}



/* 开始 SEARCH 或 UID SEARCH。 */
XRT_API bool xrtImapClientBeginSearch(
	ximapclient* pClient,
	xstrview Criteria,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Parts[2];

	if ( !__xrtImapCommandSelected(pClient) ||
		!__xrtImapCommandRequired(
			Criteria,
			"IMAP SEARCH criteria are missing"
		) ) {
		return false;
	}
	if ( !bUid ) {
		return __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("SEARCH"),
			&Criteria,
			1u,
			iDeadline,
			pCancel
		);
	}
	Parts[0] = XRT_STR_LITERAL("SEARCH");
	Parts[1] = Criteria;
	return __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("UID"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
}



/* 开始 FETCH 或 UID FETCH。 */
XRT_API bool xrtImapClientBeginFetch(
	ximapclient* pClient,
	xstrview Set,
	xstrview Items,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Parts[3];

	if ( !__xrtImapCommandSelected(pClient) || !__xrtImapCommandSet(Set) ||
		!__xrtImapCommandRequired(Items, "IMAP FETCH items are missing") ) {
		return false;
	}
	if ( !bUid ) {
		Parts[0] = Set;
		Parts[1] = Items;
		return __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("FETCH"),
			Parts,
			2u,
			iDeadline,
			pCancel
		);
	}
	Parts[0] = XRT_STR_LITERAL("FETCH");
	Parts[1] = Set;
	Parts[2] = Items;
	return __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("UID"),
		Parts,
		3u,
		iDeadline,
		pCancel
	);
}



/* 返回 STORE 模式的协议 atom。 */
static xstrview __xrtImapCommandStoreMode(ximapstoremode Mode)
{
	switch ( Mode ) {
		case XIMAP_STORE_SET:
			return XRT_STR_LITERAL("FLAGS");
		case XIMAP_STORE_SET_SILENT:
			return XRT_STR_LITERAL("FLAGS.SILENT");
		case XIMAP_STORE_ADD:
			return XRT_STR_LITERAL("+FLAGS");
		case XIMAP_STORE_ADD_SILENT:
			return XRT_STR_LITERAL("+FLAGS.SILENT");
		case XIMAP_STORE_REMOVE:
			return XRT_STR_LITERAL("-FLAGS");
		case XIMAP_STORE_REMOVE_SILENT:
			return XRT_STR_LITERAL("-FLAGS.SILENT");
		default:
			return __xrtMailView(NULL, 0);
	}
}



/* 开始 STORE 或 UID STORE。 */
XRT_API bool xrtImapClientBeginStore(
	ximapclient* pClient,
	xstrview Set,
	ximapstoremode Mode,
	xstrview Flags,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Operation = __xrtImapCommandStoreMode(Mode);
	xstrview Parts[4];

	if ( !__xrtImapCommandSelected(pClient) || !__xrtImapCommandSet(Set) ||
		!__xrtImapCommandRequired(
			Operation,
			"invalid IMAP STORE mode"
		) || !__xrtImapCommandRequired(
			Flags,
			"IMAP STORE flags are missing"
		) ) {
		return false;
	}
	if ( !bUid ) {
		Parts[0] = Set;
		Parts[1] = Operation;
		Parts[2] = Flags;
		return __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("STORE"),
			Parts,
			3u,
			iDeadline,
			pCancel
		);
	}
	Parts[0] = XRT_STR_LITERAL("STORE");
	Parts[1] = Set;
	Parts[2] = Operation;
	Parts[3] = Flags;
	return __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("UID"),
		Parts,
		4u,
		iDeadline,
		pCancel
	);
}



/* 开始 COPY 或 MOVE 的共享实现。 */
static bool __xrtImapCommandBeginCopyMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	bool bMove,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	str sMailbox;
	xstrview Parts[3];
	bool bSuccess;

	if ( !__xrtImapCommandSelected(pClient) || !__xrtImapCommandSet(Set) ) {
		return false;
	}
	if ( bMove && ((xrtImapClientCapabilities(pClient) &
		XIMAP_CAP_MOVE) == 0) ) {
		return __xrtImapCommandError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise MOVE"
		);
	}
	sMailbox = __xrtImapCommandMailbox(pClient, Mailbox, &Parts[2].Size);
	if ( sMailbox == NULL ) {
		return false;
	}
	if ( !bUid ) {
		Parts[0] = Set;
		Parts[1].Data = sMailbox;
		Parts[1].Size = Parts[2].Size;
		bSuccess = __xrtImapCommandBegin(
			pClient,
			bMove ? XRT_STR_LITERAL("MOVE") : XRT_STR_LITERAL("COPY"),
			Parts,
			2u,
			iDeadline,
			pCancel
		);
	} else {
		Parts[0] = bMove ? XRT_STR_LITERAL("MOVE") :
			XRT_STR_LITERAL("COPY");
		Parts[1] = Set;
		Parts[2].Data = sMailbox;
		bSuccess = __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("UID"),
			Parts,
			3u,
			iDeadline,
			pCancel
		);
	}
	xrtFree(sMailbox);
	return bSuccess;
}



/* 开始 COPY。 */
XRT_API bool xrtImapClientBeginCopy(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandBeginCopyMove(
		pClient,
		Set,
		Mailbox,
		bUid,
		false,
		iDeadline,
		pCancel
	);
}



/* 开始 MOVE。 */
XRT_API bool xrtImapClientBeginMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandBeginCopyMove(
		pClient,
		Set,
		Mailbox,
		bUid,
		true,
		iDeadline,
		pCancel
	);
}



/* 开始 EXPUNGE 或 UID EXPUNGE。 */
XRT_API bool xrtImapClientBeginExpunge(
	ximapclient* pClient,
	xstrview UidSet,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Parts[2];

	if ( !__xrtImapCommandSelected(pClient) ) {
		return false;
	}
	if ( !__xrtMailViewValid(UidSet) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( UidSet.Size == 0 ) {
		return __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("EXPUNGE"),
			NULL,
			0,
			iDeadline,
			pCancel
		);
	}
	if ( !xrtImapSequenceSetValid(UidSet) ) {
		return __xrtImapCommandError(
			XERR_ARGUMENT,
			"invalid IMAP UID set"
		);
	}
	if ( (xrtImapClientCapabilities(pClient) & XIMAP_CAP_UIDPLUS) == 0 ) {
		return __xrtImapCommandError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise UIDPLUS"
		);
	}
	Parts[0] = XRT_STR_LITERAL("EXPUNGE");
	Parts[1] = UidSet;
	return __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("UID"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
}



/* 开始 IDLE，continuation 和未请求事件由调用方读取。 */
XRT_API bool xrtImapClientBeginIdle(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapCommandSelected(pClient) ) {
		return false;
	}
	if ( (xrtImapClientCapabilities(pClient) & XIMAP_CAP_IDLE) == 0 ) {
		return __xrtImapCommandError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise IDLE"
		);
	}
	return xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("IDLE"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) && __xrtImapClientIdleStart(pClient);
}



/* 向活动 IDLE 命令发送 DONE。 */
XRT_API bool xrtImapClientEndIdle(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapCommandSelected(pClient) ) {
		return false;
	}
	return __xrtImapClientIdleEnd(pClient) && xrtImapClientContinue(
		pClient,
		XRT_STR_LITERAL("DONE"),
		iDeadline,
		pCancel
	);
}

#endif
