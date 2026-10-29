#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_SMTP_SUBMIT)

/* Compose sink 只借用客户端与本次统一等待上下文。 */
typedef struct __xsmtpsubmitsink {
	xsmtpclient* Client;
	xdeadline Deadline;
	xcancel* Cancel;
} __xsmtpsubmitsink;



/* 验证 ESMTP 参数文本不含线路控制字符。 */
static bool __xrtSmtpSubmitParameters(xstrview Parameters)
{
	if ( !__xrtMailViewValid(Parameters) ) {
		return false;
	}
	for ( size_t i = 0; i < Parameters.Size; i++ ) {
		unsigned char iByte = (unsigned char)Parameters.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 验证路径与参数能够装入单条 SMTP 命令。 */
static bool __xrtSmtpSubmitPath(
	xstrview Path,
	xstrview Parameters,
	size_t iPrefix,
	bool bAllowEmpty
)
{
	size_t iArguments;
	size_t iCommand;

	if ( !xrtSmtpPathValid(Path, bAllowEmpty) ||
		 !__xrtSmtpSubmitParameters(Parameters) ||
		 !__xrtMailSizeAdd(iPrefix, Path.Size, &iArguments) ||
		 !__xrtMailSizeAdd(iArguments, 1u, &iArguments) ||
		 ((Parameters.Size != 0) &&
		  (!__xrtMailSizeAdd(iArguments, 1u, &iArguments) ||
		   !__xrtMailSizeAdd(iArguments, Parameters.Size, &iArguments))) ||
		 !__xrtMailSizeAdd(7u, iArguments, &iCommand) ||
		 (iCommand > XSMTP_COMMAND_MAX) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid SMTP submit envelope path or parameters"
		);
		return false;
	}
	return true;
}



/* 失败后恢复 envelope；DATA 已开始时关闭连接以保证半封消息不会提交。 */
static bool __xrtSmtpSubmitRecover(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpclientstate State = xrtSmtpClientState(pClient);
	xerror* pPrimaryError = xrtTakeError();
	xerror* pRecoveryError;

	if ( (State == XSMTP_CLIENT_MAIL) ||
		 (State == XSMTP_CLIENT_RECIPIENT) ) {
		if ( !xrtSmtpClientReset(pClient, iDeadline, pCancel) ) {
			(void)xrtSmtpClientAbort(pClient);
		}
	} else if ( (State == XSMTP_CLIENT_DATA) ||
		 (State == XSMTP_CLIENT_FAILED) ) {
		(void)xrtSmtpClientAbort(pClient);
	}
	pRecoveryError = xrtTakeError();
	if ( pPrimaryError != NULL ) {
		xrtErrorFree(pRecoveryError);
		xrtSetErrorTake(pPrimaryError);
	} else {
		xrtSetErrorTake(pRecoveryError);
	}
	return false;
}



/* 把 Compose 片段直接送入 SMTP 增量 DATA。 */
static bool __xrtSmtpSubmitWrite(xbytesview Data, ptr pUserData)
{
	__xsmtpsubmitsink* pSink = (__xsmtpsubmitsink*)pUserData;

	return xrtSmtpClientDataWrite(
		pSink->Client,
		Data,
		pSink->Deadline,
		pSink->Cancel
	);
}



/* 在 envelope 已建立后流式提交消息内容。 */
static bool __xrtSmtpSubmitData(
	xsmtpclient* pClient,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xsmtpsubmitsink Sink;
	size_t iWritten;

	if ( !xrtSmtpClientDataBegin(pClient, iDeadline, pCancel) ) {
		return __xrtSmtpSubmitRecover(
			pClient,
			iDeadline,
			pCancel
		);
	}
	Sink.Client = pClient;
	Sink.Deadline = iDeadline;
	Sink.Cancel = pCancel;
	if ( !xrtMailComposeWrite(
		pMessage,
		__xrtSmtpSubmitWrite,
		&Sink,
		&iWritten
	) ) {
		return __xrtSmtpSubmitRecover(
			pClient,
			iDeadline,
			pCancel
		);
	}
	(void)iWritten;
	return xrtSmtpClientDataEnd(pClient, iDeadline, pCancel);
}



/* 验证 envelope 本体和收件人数组范围，避免计数字节乘法溢出。 */
static bool __xrtSmtpSubmitEnvelopeValid(const xsmtpenvelope* pEnvelope)
{
	size_t iRecipientBytes;

	if ( !xrtMemRangeValid(pEnvelope, sizeof(*pEnvelope)) ||
		 (pEnvelope->RecipientCount == 0) ||
		 (pEnvelope->RecipientCount >
		  (SIZE_MAX / sizeof(*pEnvelope->Recipients))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iRecipientBytes = pEnvelope->RecipientCount *
		sizeof(*pEnvelope->Recipients);
	if ( !xrtMemRangeValid(pEnvelope->Recipients, iRecipientBytes) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 使用独立 envelope 流式提交消息。 */
XRT_API bool xrtSmtpSubmitEnvelope(
	xsmtpclient* pClient,
	const xsmtpenvelope* pEnvelope,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtSmtpSubmitEnvelopeValid(pEnvelope) ||
		 (xrtSmtpClientState(pClient) != XSMTP_CLIENT_READY) ||
		 !xrtMailMessageValid(pMessage) ||
		 !__xrtSmtpSubmitPath(
			pEnvelope->ReversePath,
			pEnvelope->MailParameters,
			6u,
			true
		 ) ) {
		if ( xrtGetError() == NULL ) {
			__xrtMailSetInvalidArgument();
		}
		return false;
	}
	for ( size_t i = 0; i < pEnvelope->RecipientCount; i++ ) {
		if ( !__xrtSmtpSubmitPath(
			pEnvelope->Recipients[i].Address,
			pEnvelope->Recipients[i].Parameters,
			4u,
			false
		) ) {
			return false;
		}
	}
	if ( !xrtSmtpClientMail(
		pClient,
		pEnvelope->ReversePath,
		pEnvelope->MailParameters,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	for ( size_t i = 0; i < pEnvelope->RecipientCount; i++ ) {
		if ( !xrtSmtpClientRcpt(
			pClient,
			pEnvelope->Recipients[i].Address,
			pEnvelope->Recipients[i].Parameters,
			iDeadline,
			pCancel
		) ) {
			return __xrtSmtpSubmitRecover(
				pClient,
				iDeadline,
				pCancel
			);
		}
	}
	return __xrtSmtpSubmitData(pClient, pMessage, iDeadline, pCancel);
}



/* 发送消息描述中的一个地址数组。 */
static bool __xrtSmtpSubmitAddresses(
	xsmtpclient* pClient,
	const xmailaddress* pAddresses,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !xrtSmtpClientRcpt(
			pClient,
			pAddresses[i].Address,
			XRT_STR_LITERAL(""),
			iDeadline,
			pCancel
		) ) {
			return false;
		}
	}
	return true;
}



/* 从消息 From、To、Cc、Bcc 自动建立 envelope 并提交。 */
XRT_API bool xrtSmtpSubmit(
	xsmtpclient* pClient,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( (xrtSmtpClientState(pClient) != XSMTP_CLIENT_READY) ||
		 !xrtMailMessageValid(pMessage) ) {
		if ( xrtGetError() == NULL ) {
			__xrtMailSetInvalidArgument();
		}
		return false;
	}
	if ( !xrtSmtpClientMail(
		pClient,
		pMessage->From.Address,
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) || !__xrtSmtpSubmitAddresses(
		pClient,
		pMessage->To,
		pMessage->ToCount,
		iDeadline,
		pCancel
	) || !__xrtSmtpSubmitAddresses(
		pClient,
		pMessage->Cc,
		pMessage->CcCount,
		iDeadline,
		pCancel
	) || !__xrtSmtpSubmitAddresses(
		pClient,
		pMessage->Bcc,
		pMessage->BccCount,
		iDeadline,
		pCancel
	) ) {
		return __xrtSmtpSubmitRecover(
			pClient,
			iDeadline,
			pCancel
		);
	}
	return __xrtSmtpSubmitData(pClient, pMessage, iDeadline, pCancel);
}

#endif
