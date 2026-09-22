#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_COMPOSE)

#define __XMAIL_COMPOSE_STACK 1024u
#define __XMAIL_COMPOSE_BASE64_INPUT \
	((XMAIL_BASE64_LINE_DEFAULT / 4u) * 3u * 48u)
#define __XMAIL_COMPOSE_BASE64_OUTPUT 4096u



typedef struct __xmailcomposecontext {
	const xmailmessage* Message;
	xmailbuilder Builder;
	xstrview Date;
	xstrview MessageId;
	xstrview Mixed;
	xstrview Alternative;
	xstrview Related;
	xstrview Text;
	xstrview Html;
	str OwnedDate;
	str OwnedMessageId;
	str OwnedMixed;
	str OwnedAlternative;
	str OwnedRelated;
	str OwnedText;
	str OwnedHtml;
	bool HasText;
	bool HasHtml;
	bool HasInline;
	bool HasRootAttachment;
} __xmailcomposecontext;



typedef struct __xmailcomposebuffer {
	str Data;
	size_t Size;
	size_t Capacity;
} __xmailcomposebuffer;



/* 按字节比较两个借用视图。 */
static bool __xrtMailComposeViewEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证借用数组范围且避免 count 乘法回绕。 */
static bool __xrtMailComposeArray(
	const void* pArray,
	size_t iCount,
	size_t iElementSize
)
{
	if ( (iElementSize == 0) || (iCount > (SIZE_MAX / iElementSize)) ||
		 !xrtMemRangeValid(pArray, iCount * iElementSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证地址数组的完整格式化路径。 */
static bool __xrtMailComposeAddresses(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags
)
{
	size_t iSize;

	return __xrtMailComposeArray(
		pAddresses,
		iCount,
		sizeof(*pAddresses)
	) && xrtMailAddressListWrite(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
		NULL,
		0,
		&iSize
	);
}



/* 判断自定义字段是否会覆盖 Compose 管理的报文字段。 */
static bool __xrtMailComposeManagedHeader(xstrview Name)
{
	static const char* const arrNames[] = {
		"date",
		"message-id",
		"from",
		"to",
		"cc",
		"bcc",
		"reply-to",
		"subject",
		"mime-version",
		"content-type",
		"content-transfer-encoding",
		"content-disposition",
		"content-id"
	};

	for ( size_t i = 0; i < (sizeof(arrNames) / sizeof(arrNames[0])); i++ ) {
		if ( __xrtMailAsciiEqualI(Name, __xrtMailView(
			arrNames[i],
			strlen(arrNames[i])
		)) ) {
			return true;
		}
	}
	return false;
}



/* Content-ID 由 Compose 补尖括号，因此输入只允许安全 ASCII 正文。 */
static bool __xrtMailComposeContentId(xstrview ContentId)
{
	for ( size_t i = 0; i < ContentId.Size; i++ ) {
		unsigned char iByte = (unsigned char)ContentId.Data[i];

		if ( (iByte < 33u) || (iByte > 126u) ||
			 (iByte == (unsigned char)'<') ||
			 (iByte == (unsigned char)'>') ) {
			return false;
		}
	}
	return true;
}



/* 检查长度结果是否与消息中的任一借用范围重叠。 */
static bool __xrtMailComposeSizeOverlap(
	const xmailmessage* pMessage,
	const size_t* pSize
)
{
	if ( pSize == NULL ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pSize, sizeof(*pSize), pMessage, sizeof(*pMessage)) ||
		 xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->To,
			pMessage->ToCount * sizeof(*pMessage->To)
		 ) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->Cc,
			pMessage->CcCount * sizeof(*pMessage->Cc)
		 ) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->Bcc,
			pMessage->BccCount * sizeof(*pMessage->Bcc)
		 ) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->Attachments,
			pMessage->AttachmentCount * sizeof(*pMessage->Attachments)
		 ) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->Headers,
			pMessage->HeaderCount * sizeof(*pMessage->Headers)
		 ) ) {
		return true;
	}
	{
		xstrview arrViews[13];

		arrViews[0] = pMessage->From.Name;
		arrViews[1] = pMessage->From.Address;
		arrViews[2] = pMessage->ReplyTo.Name;
		arrViews[3] = pMessage->ReplyTo.Address;
		arrViews[4] = pMessage->Subject;
		arrViews[5] = pMessage->Text;
		arrViews[6] = pMessage->Html;
		arrViews[7] = pMessage->Date;
		arrViews[8] = pMessage->MessageId;
		arrViews[9] = pMessage->MessageIdDomain;
		arrViews[10] = pMessage->MixedBoundary;
		arrViews[11] = pMessage->AlternativeBoundary;
		arrViews[12] = pMessage->RelatedBoundary;

		for ( size_t i = 0; i < (sizeof(arrViews) / sizeof(arrViews[0])); i++ ) {
			if ( xrtMemRangesOverlap(
				pSize,
				sizeof(*pSize),
				arrViews[i].Data,
				arrViews[i].Size
			) ) {
				return true;
			}
		}
	}
	for ( size_t i = 0; i < pMessage->ToCount; i++ ) {
		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->To[i].Name.Data, pMessage->To[i].Name.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->To[i].Address.Data, pMessage->To[i].Address.Size) ) {
			return true;
		}
	}
	for ( size_t i = 0; i < pMessage->CcCount; i++ ) {
		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Cc[i].Name.Data, pMessage->Cc[i].Name.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Cc[i].Address.Data, pMessage->Cc[i].Address.Size) ) {
			return true;
		}
	}
	for ( size_t i = 0; i < pMessage->BccCount; i++ ) {
		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Bcc[i].Name.Data, pMessage->Bcc[i].Name.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Bcc[i].Address.Data, pMessage->Bcc[i].Address.Size) ) {
			return true;
		}
	}
	for ( size_t i = 0; i < pMessage->AttachmentCount; i++ ) {
		const xmailattachment* pAttachment = &pMessage->Attachments[i];

		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pAttachment->FileName.Data, pAttachment->FileName.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pAttachment->MediaType.Data, pAttachment->MediaType.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pAttachment->ContentId.Data, pAttachment->ContentId.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pAttachment->Data.Data, pAttachment->Data.Size) ) {
			return true;
		}
	}
	for ( size_t i = 0; i < pMessage->HeaderCount; i++ ) {
		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Headers[i].Name.Data, pMessage->Headers[i].Name.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Headers[i].Value.Data, pMessage->Headers[i].Value.Size) ) {
			return true;
		}
	}
	return false;
}



/* 在第一次输出前完整验证消息描述和所有借用输入。 */
static bool __xrtMailComposeValid(
	const xmailmessage* pMessage,
	const size_t* pSize
)
{
	xmailmessageidview MessageId;
	xmailmediatypeview MediaType;
	xstrview Local;
	xstrview Domain;
	xtime iDate;
	int iOffset;
	size_t iSize;

	if ( !xrtMemRangeValid(pMessage, sizeof(*pMessage)) ||
		 !xrtMemRangeValid(pSize, pSize != NULL ? sizeof(*pSize) : 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailComposeArray(pMessage->To, pMessage->ToCount,
		sizeof(*pMessage->To)) ||
		 !__xrtMailComposeArray(pMessage->Cc, pMessage->CcCount,
		sizeof(*pMessage->Cc)) ||
		 !__xrtMailComposeArray(pMessage->Bcc, pMessage->BccCount,
		sizeof(*pMessage->Bcc)) ||
		 !__xrtMailComposeArray(pMessage->Attachments,
		pMessage->AttachmentCount, sizeof(*pMessage->Attachments)) ||
		 !__xrtMailComposeArray(pMessage->Headers, pMessage->HeaderCount,
		sizeof(*pMessage->Headers)) ) {
		return false;
	}
	if ( !__xrtMailViewValid(pMessage->Subject) ||
		 !__xrtMailViewValid(pMessage->Text) ||
		 !__xrtMailViewValid(pMessage->Html) ||
		 !__xrtMailViewValid(pMessage->Date) ||
		 !__xrtMailViewValid(pMessage->MessageId) ||
		 !__xrtMailViewValid(pMessage->MessageIdDomain) ||
		 !__xrtMailViewValid(pMessage->MixedBoundary) ||
		 !__xrtMailViewValid(pMessage->AlternativeBoundary) ||
		 !__xrtMailViewValid(pMessage->RelatedBoundary) ||
		 (pMessage->From.Address.Size == 0) ||
		 ((pMessage->ToCount == 0) && (pMessage->CcCount == 0) &&
		  (pMessage->BccCount == 0)) ||
		 ((pMessage->ReplyTo.Address.Size == 0) &&
		  (pMessage->ReplyTo.Name.Size != 0)) ||
		 ((pMessage->WordEncoding != XMAIL_WORD_BASE64) &&
		  (pMessage->WordEncoding != XMAIL_WORD_Q)) ||
		 ((pMessage->AddressFlags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) != 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(pMessage->Subject, NULL) ||
		 !xrtUtf8Valid(pMessage->Text, NULL) ||
		 !xrtUtf8Valid(pMessage->Html, NULL) ||
		 !__xrtMailComposeAddresses(&pMessage->From, 1u,
			pMessage->WordEncoding, pMessage->AddressFlags) ||
		 !__xrtMailComposeAddresses(pMessage->To, pMessage->ToCount,
			pMessage->WordEncoding, pMessage->AddressFlags) ||
		 !__xrtMailComposeAddresses(pMessage->Cc, pMessage->CcCount,
			pMessage->WordEncoding, pMessage->AddressFlags) ||
		 !__xrtMailComposeAddresses(pMessage->Bcc, pMessage->BccCount,
			pMessage->WordEncoding, pMessage->AddressFlags) ||
		 ((pMessage->ReplyTo.Address.Size != 0) &&
		  !__xrtMailComposeAddresses(&pMessage->ReplyTo, 1u,
			pMessage->WordEncoding, pMessage->AddressFlags)) ||
		 !xrtMailWordEncodeWrite(pMessage->Subject,
			pMessage->WordEncoding, NULL, 0, &iSize) ||
		 !xrtMailHeaderWrite(XRT_STR_LITERAL("X"), XRT_STR_LITERAL("x"),
			pMessage->HeaderLineSize, NULL, 0, &iSize) ) {
		return false;
	}
	if ( (pMessage->Date.Size != 0) && !xrtMailDateParse(
		pMessage->Date,
		&iDate,
		&iOffset
	) ) {
		return false;
	}
	if ( (pMessage->MessageId.Size != 0) && !xrtMailMessageIdParse(
		pMessage->MessageId,
		XMAIL_ID_DEFAULT,
		&MessageId
	) ) {
		return false;
	}
	if ( pMessage->MessageId.Size == 0 ) {
		if ( pMessage->MessageIdDomain.Size != 0 ) {
			Domain = pMessage->MessageIdDomain;
		} else if ( !xrtMailAddressValid(
			pMessage->From.Address,
			pMessage->AddressFlags,
			&Local,
			&Domain
		) ) {
			return false;
		}
		if ( !xrtMailMessageIdWrite(Domain, NULL, 0, &iSize) ) {
			return false;
		}
	}
	if ( ((pMessage->MixedBoundary.Size != 0) &&
		 !xrtMailBoundaryValid(pMessage->MixedBoundary)) ||
		 ((pMessage->AlternativeBoundary.Size != 0) &&
		 !xrtMailBoundaryValid(pMessage->AlternativeBoundary)) ||
		 ((pMessage->RelatedBoundary.Size != 0) &&
		 !xrtMailBoundaryValid(pMessage->RelatedBoundary)) ) {
		return false;
	}
	for ( size_t i = 0; i < pMessage->AttachmentCount; i++ ) {
		const xmailattachment* pAttachment = &pMessage->Attachments[i];
		xstrview Type = pAttachment->MediaType.Size != 0 ?
			pAttachment->MediaType : XRT_STR_LITERAL("application/octet-stream");

		if ( !__xrtMailViewValid(pAttachment->FileName) ||
			 !__xrtMailViewValid(pAttachment->MediaType) ||
			 !__xrtMailViewValid(pAttachment->ContentId) ||
			 !xrtMemRangeValid(pAttachment->Data.Data, pAttachment->Data.Size) ||
			 !xrtUtf8Valid(pAttachment->FileName, NULL) ||
			 !xrtMailMediaTypeParse(Type, &MediaType) ||
			 !__xrtMailComposeContentId(pAttachment->ContentId) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		if ( (pAttachment->FileName.Size != 0) && !xrtMailParamWrite(
			XRT_STR_LITERAL("filename"),
			pAttachment->FileName,
			XMAIL_PARAM_ENCODING_AUTO,
			NULL,
			0,
			&iSize
		) ) {
			return false;
		}
	}
	for ( size_t i = 0; i < pMessage->HeaderCount; i++ ) {
		const xmailheaderview* pHeader = &pMessage->Headers[i];

		if ( !__xrtMailViewValid(pHeader->Name) ||
			 !__xrtMailViewValid(pHeader->Value) ||
			 __xrtMailComposeManagedHeader(pHeader->Name) ||
			 !xrtMailHeaderWrite(pHeader->Name, pHeader->Value,
				pMessage->HeaderLineSize, NULL, 0, &iSize) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
	}
	if ( __xrtMailComposeSizeOverlap(pMessage, pSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 释放 Compose 在首次输出前准备的全部 owned 文本。 */
static void __xrtMailComposeFree(__xmailcomposecontext* pContext)
{
	xrtFree(pContext->OwnedDate);
	xrtFree(pContext->OwnedMessageId);
	xrtFree(pContext->OwnedMixed);
	xrtFree(pContext->OwnedAlternative);
	xrtFree(pContext->OwnedRelated);
	xrtFree(pContext->OwnedText);
	xrtFree(pContext->OwnedHtml);
	memset(pContext, 0, sizeof(*pContext));
}



/* 取得调用方 boundary，或在真正需要该层时生成一个。 */
static bool __xrtMailComposeBoundary(
	xstrview Configured,
	bool bNeeded,
	xstrview* pBoundary,
	str* pOwned
)
{
	size_t iSize;

	if ( !bNeeded ) {
		*pBoundary = __xrtMailView(NULL, 0);
		return true;
	}
	if ( Configured.Size != 0 ) {
		*pBoundary = Configured;
		return true;
	}
	*pOwned = xrtMailBoundary(&iSize);
	if ( *pOwned == NULL ) {
		return false;
	}
	*pBoundary = __xrtMailView(*pOwned, iSize);
	return true;
}



/* 生成所有随机值并预编码文本，避免语法错误发生在首个 sink 回调之后。 */
static bool __xrtMailComposePrepare(
	__xmailcomposecontext* pContext,
	const xmailmessage* pMessage,
	xmailwriteproc pWrite,
	ptr pUserData
)
{
	xstrview Local;
	xstrview Domain;
	size_t iSize;

	memset(pContext, 0, sizeof(*pContext));
	pContext->Message = pMessage;
	pContext->HasText = pMessage->Text.Size != 0;
	pContext->HasHtml = pMessage->Html.Size != 0;
	for ( size_t i = 0; i < pMessage->AttachmentCount; i++ ) {
		if ( pMessage->Attachments[i].Inline && pContext->HasHtml ) {
			pContext->HasInline = true;
		} else {
			pContext->HasRootAttachment = true;
		}
	}
	if ( pMessage->Date.Size != 0 ) {
		pContext->Date = pMessage->Date;
	} else {
		pContext->OwnedDate = xrtMailDate(xrtNow(), 0, &iSize);
		if ( pContext->OwnedDate == NULL ) {
			goto fail;
		}
		pContext->Date = __xrtMailView(pContext->OwnedDate, iSize);
	}
	if ( pMessage->MessageId.Size != 0 ) {
		pContext->MessageId = pMessage->MessageId;
	} else {
		if ( pMessage->MessageIdDomain.Size != 0 ) {
			Domain = pMessage->MessageIdDomain;
		} else if ( !xrtMailAddressValid(
			pMessage->From.Address,
			pMessage->AddressFlags,
			&Local,
			&Domain
		) ) {
			goto fail;
		}
		pContext->OwnedMessageId = xrtMailMessageId(Domain, &iSize);
		if ( pContext->OwnedMessageId == NULL ) {
			goto fail;
		}
		pContext->MessageId = __xrtMailView(
			pContext->OwnedMessageId,
			iSize
		);
	}
	if ( !__xrtMailComposeBoundary(
		pMessage->MixedBoundary,
		pContext->HasRootAttachment,
		&pContext->Mixed,
		&pContext->OwnedMixed
	) || !__xrtMailComposeBoundary(
		pMessage->AlternativeBoundary,
		pContext->HasText && pContext->HasHtml,
		&pContext->Alternative,
		&pContext->OwnedAlternative
	) || !__xrtMailComposeBoundary(
		pMessage->RelatedBoundary,
		pContext->HasInline,
		&pContext->Related,
		&pContext->OwnedRelated
	) ) {
		goto fail;
	}
	if ( ((pContext->Mixed.Size != 0) &&
		 __xrtMailComposeViewEqual(pContext->Mixed, pContext->Alternative)) ||
		 ((pContext->Mixed.Size != 0) &&
		 __xrtMailComposeViewEqual(pContext->Mixed, pContext->Related)) ||
		 ((pContext->Alternative.Size != 0) &&
		 __xrtMailComposeViewEqual(pContext->Alternative, pContext->Related)) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_MIME,
			"nested MIME boundaries must be distinct"
		);
		goto fail;
	}
	if ( pContext->HasText ) {
		pContext->OwnedText = xrtMailQp(
			pMessage->Text.Data,
			pMessage->Text.Size,
			0,
			XMAIL_QP_TEXT,
			&iSize
		);
		if ( pContext->OwnedText == NULL ) {
			goto fail;
		}
		pContext->Text = __xrtMailView(pContext->OwnedText, iSize);
	}
	if ( pContext->HasHtml ) {
		pContext->OwnedHtml = xrtMailQp(
			pMessage->Html.Data,
			pMessage->Html.Size,
			0,
			XMAIL_QP_TEXT,
			&iSize
		);
		if ( pContext->OwnedHtml == NULL ) {
			goto fail;
		}
		pContext->Html = __xrtMailView(pContext->OwnedHtml, iSize);
	}
	if ( !xrtMailBuilderInit(&pContext->Builder, pWrite, pUserData) ) {
		goto fail;
	}
	return true;

fail:
	__xrtMailComposeFree(pContext);
	return false;
}



/* 写出带一个 MIME 参数的字段值，常见路径只使用栈缓冲。 */
static bool __xrtMailComposeParamHeader(
	xmailbuilder* pBuilder,
	xstrview Header,
	xstrview Base,
	xstrview ParamName,
	xstrview ParamValue,
	xmailparamencoding Encoding,
	size_t iLineSize
)
{
	char arrStack[__XMAIL_COMPOSE_STACK];
	char* sValue = arrStack;
	size_t iParamSize;
	size_t iValueSize;
	bool bResult;

	if ( ParamValue.Size == 0 ) {
		return xrtMailBuilderHeader(pBuilder, Header, Base, iLineSize);
	}
	if ( !xrtMailParamWrite(
		ParamName,
		ParamValue,
		Encoding,
		NULL,
		0,
		&iParamSize
	) || !__xrtMailSizeAdd(Base.Size, iParamSize, &iValueSize) ) {
		return false;
	}
	if ( iValueSize == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return false;
	}
	if ( iValueSize >= sizeof(arrStack) ) {
		sValue = (char*)xrtMalloc(iValueSize + 1u);
		if ( sValue == NULL ) {
			return false;
		}
	}
	memcpy(sValue, Base.Data, Base.Size);
	bResult = xrtMailParamWrite(
		ParamName,
		ParamValue,
		Encoding,
		sValue + Base.Size,
		iParamSize + 1u,
		&iParamSize
	) && xrtMailBuilderHeader(
		pBuilder,
		Header,
		__xrtMailView(sValue, iValueSize),
		iLineSize
	);
	if ( sValue != arrStack ) {
		xrtFree(sValue);
	}
	return bResult;
}



/* 写出一个 UTF-8 文本 entity。 */
static bool __xrtMailComposeText(
	__xmailcomposecontext* pContext,
	xstrview MediaType,
	xstrview Encoded
)
{
	return __xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Type"),
		MediaType,
		XRT_STR_LITERAL("charset"),
		XRT_STR_LITERAL("UTF-8"),
		XMAIL_PARAM_ENCODING_TOKEN,
		pContext->Message->HeaderLineSize
	) && xrtMailBuilderHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		XRT_STR_LITERAL("quoted-printable"),
		pContext->Message->HeaderLineSize
	) && xrtMailBuilderHeadersEnd(&pContext->Builder) &&
		xrtMailBuilderBody(&pContext->Builder, Encoded.Data, Encoded.Size);
}



/* 按固定小块写出附件 Base64，不创建按附件大小增长的临时结果。 */
static bool __xrtMailComposeBase64(
	xmailbuilder* pBuilder,
	xbytesview Data
)
{
	char arrOutput[__XMAIL_COMPOSE_BASE64_OUTPUT + 1u];

	for ( size_t i = 0; i < Data.Size; ) {
		size_t iChunk = Data.Size - i;
		size_t iOutputSize;

		if ( iChunk > __XMAIL_COMPOSE_BASE64_INPUT ) {
			iChunk = __XMAIL_COMPOSE_BASE64_INPUT;
		}
		if ( !xrtMailBase64Write(
			Data.Data + i,
			iChunk,
			XMAIL_BASE64_LINE_DEFAULT,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) || !xrtMailBuilderBody(pBuilder, arrOutput, iOutputSize) ) {
			return false;
		}
		i += iChunk;
	}
	return true;
}



/* 写出一个内联或普通附件 entity。 */
static bool __xrtMailComposeAttachment(
	__xmailcomposecontext* pContext,
	const xmailattachment* pAttachment
)
{
	xstrview MediaType = pAttachment->MediaType.Size != 0 ?
		pAttachment->MediaType : XRT_STR_LITERAL("application/octet-stream");
	xstrview Disposition = pAttachment->Inline ?
		XRT_STR_LITERAL("inline") : XRT_STR_LITERAL("attachment");
	char arrContentId[__XMAIL_COMPOSE_STACK];
	char* sContentId = arrContentId;
	size_t iContentIdSize;
	bool bResult;

	if ( !__xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Type"),
		MediaType,
		XRT_STR_LITERAL("name"),
		pAttachment->FileName,
		XMAIL_PARAM_ENCODING_AUTO,
		pContext->Message->HeaderLineSize
	) || !xrtMailBuilderHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		XRT_STR_LITERAL("base64"),
		pContext->Message->HeaderLineSize
	) || !__xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Disposition"),
		Disposition,
		XRT_STR_LITERAL("filename"),
		pAttachment->FileName,
		XMAIL_PARAM_ENCODING_AUTO,
		pContext->Message->HeaderLineSize
	) ) {
		return false;
	}
	if ( pAttachment->ContentId.Size != 0 ) {
		if ( !__xrtMailSizeAdd(pAttachment->ContentId.Size, 2u,
			&iContentIdSize) ) {
			return false;
		}
		if ( iContentIdSize == SIZE_MAX ) {
			__xrtMailSetSizeOverflow();
			return false;
		}
		if ( iContentIdSize >= sizeof(arrContentId) ) {
			sContentId = (char*)xrtMalloc(iContentIdSize + 1u);
			if ( sContentId == NULL ) {
				return false;
			}
		}
		sContentId[0] = '<';
		memcpy(sContentId + 1u, pAttachment->ContentId.Data,
			pAttachment->ContentId.Size);
		sContentId[iContentIdSize - 1u] = '>';
		sContentId[iContentIdSize] = 0;
		bResult = xrtMailBuilderHeader(
			&pContext->Builder,
			XRT_STR_LITERAL("Content-ID"),
			__xrtMailView(sContentId, iContentIdSize),
			pContext->Message->HeaderLineSize
		);
		if ( sContentId != arrContentId ) {
			xrtFree(sContentId);
		}
		if ( !bResult ) {
			return false;
		}
	}
	return xrtMailBuilderHeadersEnd(&pContext->Builder) &&
		__xrtMailComposeBase64(&pContext->Builder, pAttachment->Data);
}



/* 写出 HTML 与全部内联资源组成的 related entity。 */
static bool __xrtMailComposeRelated(__xmailcomposecontext* pContext)
{
	if ( !__xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("multipart/related"),
		XRT_STR_LITERAL("boundary"),
		pContext->Related,
		XMAIL_PARAM_ENCODING_QUOTED,
		pContext->Message->HeaderLineSize
	) || !xrtMailBuilderHeadersEnd(&pContext->Builder) ||
		 !xrtMailBuilderPartBegin(
			&pContext->Builder,
			pContext->Related,
			XMAIL_MULTIPART_FIRST
		 ) || !__xrtMailComposeText(
			pContext,
			XRT_STR_LITERAL("text/html"),
			pContext->Html
		 ) ) {
		return false;
	}
	for ( size_t i = 0; i < pContext->Message->AttachmentCount; i++ ) {
		const xmailattachment* pAttachment =
			&pContext->Message->Attachments[i];

		if ( !pAttachment->Inline ) {
			continue;
		}
		if ( !xrtMailBuilderPartBegin(
			&pContext->Builder,
			pContext->Related,
			XMAIL_MULTIPART_NEXT
		) || !__xrtMailComposeAttachment(pContext, pAttachment) ) {
			return false;
		}
	}
	return xrtMailBuilderMultipart(
		&pContext->Builder,
		pContext->Related,
		XMAIL_MULTIPART_CLOSE
	);
}



/* 写出正文核心：plain、html、related 或 alternative。 */
static bool __xrtMailComposeCore(__xmailcomposecontext* pContext)
{
	if ( pContext->HasText && pContext->HasHtml ) {
		if ( !__xrtMailComposeParamHeader(
			&pContext->Builder,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("multipart/alternative"),
			XRT_STR_LITERAL("boundary"),
			pContext->Alternative,
			XMAIL_PARAM_ENCODING_QUOTED,
			pContext->Message->HeaderLineSize
		) || !xrtMailBuilderHeadersEnd(&pContext->Builder) ||
			 !xrtMailBuilderPartBegin(
				&pContext->Builder,
				pContext->Alternative,
				XMAIL_MULTIPART_FIRST
			 ) || !__xrtMailComposeText(
				pContext,
				XRT_STR_LITERAL("text/plain"),
				pContext->Text
			 ) || !xrtMailBuilderPartBegin(
				&pContext->Builder,
				pContext->Alternative,
				XMAIL_MULTIPART_NEXT
			 ) ) {
			return false;
		}
		if ( pContext->HasInline ?
			!__xrtMailComposeRelated(pContext) :
			!__xrtMailComposeText(
				pContext,
				XRT_STR_LITERAL("text/html"),
				pContext->Html
			) ) {
			return false;
		}
		return xrtMailBuilderMultipart(
			&pContext->Builder,
			pContext->Alternative,
			XMAIL_MULTIPART_CLOSE
		);
	}
	if ( pContext->HasHtml ) {
		return pContext->HasInline ? __xrtMailComposeRelated(pContext) :
			__xrtMailComposeText(
				pContext,
				XRT_STR_LITERAL("text/html"),
				pContext->Html
			);
	}
	return __xrtMailComposeText(
		pContext,
		XRT_STR_LITERAL("text/plain"),
		pContext->Text
	);
}



/* 写出可选 mixed 根和全部非 related 附件。 */
static bool __xrtMailComposeRoot(__xmailcomposecontext* pContext)
{
	if ( !pContext->HasRootAttachment ) {
		return __xrtMailComposeCore(pContext);
	}
	if ( !__xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("multipart/mixed"),
		XRT_STR_LITERAL("boundary"),
		pContext->Mixed,
		XMAIL_PARAM_ENCODING_QUOTED,
		pContext->Message->HeaderLineSize
	) || !xrtMailBuilderHeadersEnd(&pContext->Builder) ||
		 !xrtMailBuilderPartBegin(
			&pContext->Builder,
			pContext->Mixed,
			XMAIL_MULTIPART_FIRST
		 ) || !__xrtMailComposeCore(pContext) ) {
		return false;
	}
	for ( size_t i = 0; i < pContext->Message->AttachmentCount; i++ ) {
		const xmailattachment* pAttachment =
			&pContext->Message->Attachments[i];

		if ( pAttachment->Inline && pContext->HasHtml ) {
			continue;
		}
		if ( !xrtMailBuilderPartBegin(
			&pContext->Builder,
			pContext->Mixed,
			XMAIL_MULTIPART_NEXT
		) || !__xrtMailComposeAttachment(pContext, pAttachment) ) {
			return false;
		}
	}
	return xrtMailBuilderMultipart(
		&pContext->Builder,
		pContext->Mixed,
		XMAIL_MULTIPART_CLOSE
	);
}



/* 写出消息级字段后进入 MIME entity 树。 */
static bool __xrtMailComposeMessage(__xmailcomposecontext* pContext)
{
	const xmailmessage* pMessage = pContext->Message;

	if ( !xrtMailBuilderHeader(&pContext->Builder,
		XRT_STR_LITERAL("Date"), pContext->Date,
		pMessage->HeaderLineSize) ||
		 !xrtMailBuilderHeader(&pContext->Builder,
		XRT_STR_LITERAL("Message-ID"), pContext->MessageId,
		pMessage->HeaderLineSize) ||
		 !xrtMailBuilderAddressHeader(&pContext->Builder,
		XRT_STR_LITERAL("From"), &pMessage->From, 1u,
		pMessage->WordEncoding, pMessage->AddressFlags,
		pMessage->HeaderLineSize) ) {
		return false;
	}
	if ( (pMessage->ToCount != 0) && !xrtMailBuilderAddressHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("To"),
		pMessage->To,
		pMessage->ToCount,
		pMessage->WordEncoding,
		pMessage->AddressFlags,
		pMessage->HeaderLineSize
	) ) {
		return false;
	}
	if ( (pMessage->CcCount != 0) && !xrtMailBuilderAddressHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Cc"),
		pMessage->Cc,
		pMessage->CcCount,
		pMessage->WordEncoding,
		pMessage->AddressFlags,
		pMessage->HeaderLineSize
	) ) {
		return false;
	}
	if ( (pMessage->ReplyTo.Address.Size != 0) &&
		 !xrtMailBuilderAddressHeader(
			&pContext->Builder,
			XRT_STR_LITERAL("Reply-To"),
			&pMessage->ReplyTo,
			1u,
			pMessage->WordEncoding,
			pMessage->AddressFlags,
			pMessage->HeaderLineSize
		 ) ) {
		return false;
	}
	if ( !xrtMailBuilderWordHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Subject"),
		pMessage->Subject,
		pMessage->WordEncoding,
		pMessage->HeaderLineSize
	) || !xrtMailBuilderHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("MIME-Version"),
		XRT_STR_LITERAL("1.0"),
		pMessage->HeaderLineSize
	) ) {
		return false;
	}
	for ( size_t i = 0; i < pMessage->HeaderCount; i++ ) {
		if ( !xrtMailBuilderHeader(
			&pContext->Builder,
			pMessage->Headers[i].Name,
			pMessage->Headers[i].Value,
			pMessage->HeaderLineSize
		) ) {
			return false;
		}
	}
	return __xrtMailComposeRoot(pContext) &&
		xrtMailBuilderFinish(&pContext->Builder);
}



/* 扩展 owned 输出缓冲并复制同步片段。 */
static bool __xrtMailComposeBufferWrite(xbytesview Data, ptr pUserData)
{
	__xmailcomposebuffer* pBuffer = (__xmailcomposebuffer*)pUserData;
	size_t iRequired;
	size_t iCapacity;
	str sData;

	if ( (pBuffer->Size == SIZE_MAX) ||
		 (Data.Size > (SIZE_MAX - pBuffer->Size - 1u)) ) {
		__xrtMailSetSizeOverflow();
		return false;
	}
	iRequired = pBuffer->Size + Data.Size + 1u;
	if ( iRequired > pBuffer->Capacity ) {
		iCapacity = pBuffer->Capacity != 0 ? pBuffer->Capacity : 4096u;
		while ( iCapacity < iRequired ) {
			if ( iCapacity > (SIZE_MAX / 2u) ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity *= 2u;
		}
		sData = pBuffer->Data != NULL ?
			(str)xrtRealloc(pBuffer->Data, iCapacity) :
			(str)xrtMalloc(iCapacity);
		if ( sData == NULL ) {
			return false;
		}
		pBuffer->Data = sData;
		pBuffer->Capacity = iCapacity;
	}
	memcpy(pBuffer->Data + pBuffer->Size, Data.Data, Data.Size);
	pBuffer->Size += Data.Size;
	pBuffer->Data[pBuffer->Size] = 0;
	return true;
}



/* 初始化高层消息描述。 */
XRT_API void xrtMailMessageInit(xmailmessage* pMessage)
{
	if ( pMessage == NULL ) {
		return;
	}
	memset(pMessage, 0, sizeof(*pMessage));
	pMessage->WordEncoding = XMAIL_WORD_BASE64;
}



/* 完整验证高层消息描述。 */
XRT_API bool xrtMailMessageValid(const xmailmessage* pMessage)
{
	return __xrtMailComposeValid(pMessage, NULL);
}



/* 流式构建完整 RFC 消息。 */
XRT_API bool xrtMailComposeWrite(
	const xmailmessage* pMessage,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
)
{
	__xmailcomposecontext Context;
	size_t iWritten;

	if ( pWrite == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailComposeValid(pMessage, pWritten) ||
		 !__xrtMailComposePrepare(&Context, pMessage, pWrite, pUserData) ) {
		return false;
	}
	if ( !__xrtMailComposeMessage(&Context) ) {
		__xrtMailComposeFree(&Context);
		return false;
	}
	iWritten = Context.Builder.Written;
	__xrtMailComposeFree(&Context);
	if ( pWritten != NULL ) {
		*pWritten = iWritten;
	}
	return true;
}



/* 构建独立的完整 RFC 消息。 */
XRT_API str xrtMailCompose(
	const xmailmessage* pMessage,
	size_t* pOutputSize
)
{
	__xmailcomposebuffer Buffer;
	size_t iSize;

	if ( !__xrtMailComposeValid(pMessage, pOutputSize) ) {
		return NULL;
	}
	memset(&Buffer, 0, sizeof(Buffer));
	if ( !xrtMailComposeWrite(
		pMessage,
		__xrtMailComposeBufferWrite,
		&Buffer,
		&iSize
	) ) {
		xrtFree(Buffer.Data);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iSize;
	}
	return Buffer.Data;
}

#endif
