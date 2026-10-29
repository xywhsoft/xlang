#define XMAIL_MODULE_XMAIL

#include "include/xmail.h"
#include "../../include/xl_extlib_abi.h"

#include "src/native/mail/mail_core.c"
#include "src/native/mail/mail_codec.c"
#include "src/native/mail/mail_header.c"
#include "src/native/mail/mail_word.c"
#include "src/native/mail/mail_address.c"
#include "src/native/mail/mail_date.c"
#include "src/native/mail/mail_id.c"
#include "src/native/mail/mail_param.c"
#include "src/native/mail/mail_multipart.c"
#include "src/native/mail/mail_message.c"
#include "src/native/mail/mail_tree.c"
#include "src/native/mail/mail_build.c"
#include "src/native/mail/mail_compose.c"
#include "src/native/transport/mail_wire.c"
#include "src/native/transport/mail_net.c"
#include "src/native/transport/mail_net_tls.c"
#include "src/native/transport/mail_net_deflate.c"
#include "src/native/transport/mail_auth.c"
#include "src/native/smtp/smtp.c"
#include "src/native/smtp/smtp_client.c"
#include "src/native/smtp/smtp_auth.c"
#include "src/native/smtp/smtp_submit.c"
#include "src/native/pop3/pop3.c"
#include "src/native/pop3/pop3_client.c"
#include "src/native/pop3/pop3_auth.c"
#include "src/native/pop3/pop3_message.c"
#include "src/native/imap/imap.c"
#include "src/native/imap/imap_data.c"
#include "src/native/imap/imap_client.c"
#include "src/native/imap/imap_auth.c"
#include "src/native/imap/imap_command.c"
#include "src/native/imap/imap_body.c"
#include "src/native/imap/imap_message.c"
#include "src/native/imap/imap_append.c"
#include "src/native/imap/imap_compress.c"

#include <limits.h>
#include <string.h>



#define XMAIL_XLANG_DOMAIN "xlang.extlib.xmail"



typedef struct xmail_native_message {
	xmailmessage Message;
	xmailaddress* To;
	xmailaddress* Cc;
	xmailaddress* Bcc;
	xmailattachment* Attachments;
	xmailheaderview* Headers;
} xmail_native_message;



/* 设置稳定的语言边界错误。 */
static bool xmail_native_fail(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, XMAIL_XLANG_DOMAIN, 0, sMessage);
	return false;
}



/* 返回当前线程的结构化错误文本。 */
const char* xmail_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	return (sMessage != NULL) && (sMessage[0] != 0) ?
		sMessage : "xmail operation failed";
}



/* 读取对象字段，不存在时返回 NULL。 */
static const xvalue* xmail_native_field(const xvalue* pObject, cstr sName)
{
	if ( (pObject == NULL) || (xrtValueType(pObject) != XVALUE_OBJECT) ) {
		return NULL;
	}
	return xrtValueObjectGet(pObject, xrtStrView(sName));
}



/* 读取可选文本字段。 */
static bool xmail_native_text(
	const xvalue* pObject,
	cstr sName,
	xstrview* pText
)
{
	const xvalue* pValue;

	if ( pText == NULL ) {
		return xmail_native_fail(XERR_ARGUMENT, "text output is null");
	}
	pValue = xmail_native_field(pObject, sName);
	if ( (pValue == NULL) || (xrtValueType(pValue) == XVALUE_NULL) ) {
		pText->Data = NULL;
		pText->Size = 0;
		return true;
	}
	if ( !xrtValueGetString(pValue, pText) ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail text field has an invalid type");
	}
	return true;
}



/* 读取可选布尔字段。 */
static bool xmail_native_bool(
	const xvalue* pObject,
	cstr sName,
	bool bDefault,
	bool* pResult
)
{
	const xvalue* pValue = xmail_native_field(pObject, sName);

	if ( pResult == NULL ) {
		return xmail_native_fail(XERR_ARGUMENT, "boolean output is null");
	}
	if ( (pValue == NULL) || (xrtValueType(pValue) == XVALUE_NULL) ) {
		*pResult = bDefault;
		return true;
	}
	if ( !xrtValueGetBool(pValue, pResult) ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail boolean field has an invalid type");
	}
	return true;
}



/* 读取可选非负 size_t 字段。 */
static bool xmail_native_size(
	const xvalue* pObject,
	cstr sName,
	size_t iDefault,
	size_t* pResult
)
{
	const xvalue* pValue = xmail_native_field(pObject, sName);
	int64 iValue;

	if ( pResult == NULL ) {
		return xmail_native_fail(XERR_ARGUMENT, "size output is null");
	}
	if ( (pValue == NULL) || (xrtValueType(pValue) == XVALUE_NULL) ) {
		*pResult = iDefault;
		return true;
	}
	if ( !xrtValueGetInt(pValue, &iValue) || (iValue < 0) ||
		 ((uint64)iValue > (uint64)SIZE_MAX) ) {
		return xmail_native_fail(XERR_RANGE, "mail size field is out of range");
	}
	*pResult = (size_t)iValue;
	return true;
}



/* 读取可选 uint32 字段。 */
static bool xmail_native_uint32(
	const xvalue* pObject,
	cstr sName,
	uint32 iDefault,
	uint32* pResult
)
{
	const xvalue* pValue = xmail_native_field(pObject, sName);
	int64 iValue;

	if ( pResult == NULL ) {
		return xmail_native_fail(XERR_ARGUMENT, "uint32 output is null");
	}
	if ( (pValue == NULL) || (xrtValueType(pValue) == XVALUE_NULL) ) {
		*pResult = iDefault;
		return true;
	}
	if ( !xrtValueGetInt(pValue, &iValue) || (iValue < 0) ||
		 ((uint64)iValue > (uint64)UINT32_MAX) ) {
		return xmail_native_fail(XERR_RANGE, "mail uint32 field is out of range");
	}
	*pResult = (uint32)iValue;
	return true;
}



/* 映射一个地址对象。 */
static bool xmail_native_address(
	const xvalue* pValue,
	xmailaddress* pAddress
)
{
	if ( pAddress == NULL ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail address output is null");
	}
	if ( pValue == NULL ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail message requires an address");
	}
	if ( xrtValueType(pValue) != XVALUE_OBJECT ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail address must be an object");
	}
	memset(pAddress, 0, sizeof(*pAddress));
	if ( !xmail_native_text(pValue, "name", &pAddress->Name) ||
		 !xmail_native_text(pValue, "address", &pAddress->Address) ) {
		return false;
	}
	if ( pAddress->Address.Size == 0 ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail address is empty");
	}
	return true;
}



/* 映射地址数组并由结果结构拥有临时数组。 */
static bool xmail_native_addresses(
	const xvalue* pValue,
	xmailaddress** ppAddresses,
	size_t* pCount
)
{
	xmailaddress* pAddresses;
	size_t iCount;
	size_t i;

	*ppAddresses = NULL;
	*pCount = 0;
	if ( (pValue == NULL) || (xrtValueType(pValue) == XVALUE_NULL) ) {
		return true;
	}
	if ( xrtValueType(pValue) != XVALUE_ARRAY ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail address list must be an array");
	}
	iCount = xrtValueCount(pValue);
	if ( iCount == 0 ) {
		return true;
	}
	pAddresses = (xmailaddress*)xrtCalloc(iCount, sizeof(*pAddresses));
	if ( pAddresses == NULL ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xmail_native_address(xrtValueArrayGet(pValue, i), &pAddresses[i]) ) {
			xrtFree(pAddresses);
			return false;
		}
	}
	*ppAddresses = pAddresses;
	*pCount = iCount;
	return true;
}



/* 映射附件数组，正文可以是 string 或 bytes。 */
static bool xmail_native_attachments(
	const xvalue* pValue,
	xmailattachment** ppAttachments,
	size_t* pCount
)
{
	xmailattachment* pAttachments;
	size_t iCount;
	size_t i;

	*ppAttachments = NULL;
	*pCount = 0;
	if ( (pValue == NULL) || (xrtValueType(pValue) == XVALUE_NULL) ) {
		return true;
	}
	if ( xrtValueType(pValue) != XVALUE_ARRAY ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail attachment list must be an array");
	}
	iCount = xrtValueCount(pValue);
	if ( iCount == 0 ) {
		return true;
	}
	pAttachments = (xmailattachment*)xrtCalloc(iCount, sizeof(*pAttachments));
	if ( pAttachments == NULL ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		const xvalue* pItem = xrtValueArrayGet(pValue, i);
		const xvalue* pData;
		xstrview Text;

		if ( (pItem == NULL) || (xrtValueType(pItem) != XVALUE_OBJECT) ||
			 !xmail_native_text(pItem, "fileName", &pAttachments[i].FileName) ||
			 !xmail_native_text(pItem, "mediaType", &pAttachments[i].MediaType) ||
			 !xmail_native_text(pItem, "contentId", &pAttachments[i].ContentId) ||
			 !xmail_native_bool(pItem, "inline", false, &pAttachments[i].Inline) ) {
			xrtFree(pAttachments);
			return false;
		}
		pData = xmail_native_field(pItem, "data");
		if ( (pData == NULL) || (xrtValueType(pData) == XVALUE_NULL) ) {
			pAttachments[i].Data.Data = NULL;
			pAttachments[i].Data.Size = 0;
		} else if ( xrtValueGetBytes(pData, &pAttachments[i].Data) ) {
			continue;
		} else if ( xrtValueGetString(pData, &Text) ) {
			pAttachments[i].Data.Data = (cbytes)Text.Data;
			pAttachments[i].Data.Size = Text.Size;
		} else {
			xrtFree(pAttachments);
			return xmail_native_fail(XERR_ARGUMENT,
				"mail attachment data must be string or bytes");
		}
	}
	*ppAttachments = pAttachments;
	*pCount = iCount;
	return true;
}



/* 映射附加字段数组。 */
static bool xmail_native_headers(
	const xvalue* pValue,
	xmailheaderview** ppHeaders,
	size_t* pCount
)
{
	xmailheaderview* pHeaders;
	size_t iCount;
	size_t i;

	*ppHeaders = NULL;
	*pCount = 0;
	if ( (pValue == NULL) || (xrtValueType(pValue) == XVALUE_NULL) ) {
		return true;
	}
	if ( xrtValueType(pValue) != XVALUE_ARRAY ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail header list must be an array");
	}
	iCount = xrtValueCount(pValue);
	if ( iCount == 0 ) {
		return true;
	}
	pHeaders = (xmailheaderview*)xrtCalloc(iCount, sizeof(*pHeaders));
	if ( pHeaders == NULL ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		const xvalue* pItem = xrtValueArrayGet(pValue, i);

		if ( (pItem == NULL) || (xrtValueType(pItem) != XVALUE_OBJECT) ||
			 !xmail_native_text(pItem, "name", &pHeaders[i].Name) ||
			 !xmail_native_text(pItem, "value", &pHeaders[i].Value) ) {
			xrtFree(pHeaders);
			return false;
		}
	}
	*ppHeaders = pHeaders;
	*pCount = iCount;
	return true;
}



/* 释放消息映射使用的临时数组。 */
static void xmail_native_message_free(xmail_native_message* pNative)
{
	if ( pNative == NULL ) {
		return;
	}
	xrtFree(pNative->To);
	xrtFree(pNative->Cc);
	xrtFree(pNative->Bcc);
	xrtFree(pNative->Attachments);
	xrtFree(pNative->Headers);
	memset(pNative, 0, sizeof(*pNative));
}



/* 把 xlang 动态对象映射为借用型 xmailmessage。 */
static bool xmail_native_message_map(
	const xvalue* pValue,
	xmail_native_message* pNative
)
{
	const xvalue* pFrom;
	const xvalue* pReplyTo;
	uint32 iWordEncoding;

	if ( (pValue == NULL) || (pNative == NULL) ||
		 (xrtValueType(pValue) != XVALUE_OBJECT) ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail message must be an object");
	}
	memset(pNative, 0, sizeof(*pNative));
	xrtMailMessageInit(&pNative->Message);
	pFrom = xmail_native_field(pValue, "from");
	if ( !xmail_native_address(pFrom, &pNative->Message.From) ) {
		goto failed;
	}
	pReplyTo = xmail_native_field(pValue, "replyTo");
	if ( (pReplyTo != NULL) && (xrtValueType(pReplyTo) != XVALUE_NULL) &&
		 !xmail_native_address(pReplyTo, &pNative->Message.ReplyTo) ) {
		goto failed;
	}
	if ( !xmail_native_addresses(xmail_native_field(pValue, "to"),
			&pNative->To, &pNative->Message.ToCount) ||
		 !xmail_native_addresses(xmail_native_field(pValue, "cc"),
			&pNative->Cc, &pNative->Message.CcCount) ||
		 !xmail_native_addresses(xmail_native_field(pValue, "bcc"),
			&pNative->Bcc, &pNative->Message.BccCount) ||
		 !xmail_native_attachments(xmail_native_field(pValue, "attachments"),
			&pNative->Attachments, &pNative->Message.AttachmentCount) ||
		 !xmail_native_headers(xmail_native_field(pValue, "headers"),
			&pNative->Headers, &pNative->Message.HeaderCount) ) {
		goto failed;
	}
	pNative->Message.To = pNative->To;
	pNative->Message.Cc = pNative->Cc;
	pNative->Message.Bcc = pNative->Bcc;
	pNative->Message.Attachments = pNative->Attachments;
	pNative->Message.Headers = pNative->Headers;
	if ( !xmail_native_text(pValue, "subject", &pNative->Message.Subject) ||
		 !xmail_native_text(pValue, "text", &pNative->Message.Text) ||
		 !xmail_native_text(pValue, "html", &pNative->Message.Html) ||
		 !xmail_native_text(pValue, "date", &pNative->Message.Date) ||
		 !xmail_native_text(pValue, "messageId", &pNative->Message.MessageId) ||
		 !xmail_native_text(pValue, "messageIdDomain", &pNative->Message.MessageIdDomain) ||
		 !xmail_native_text(pValue, "mixedBoundary", &pNative->Message.MixedBoundary) ||
		 !xmail_native_text(pValue, "alternativeBoundary", &pNative->Message.AlternativeBoundary) ||
		 !xmail_native_text(pValue, "relatedBoundary", &pNative->Message.RelatedBoundary) ||
		 !xmail_native_uint32(pValue, "addressFlags",
			pNative->Message.AddressFlags, &pNative->Message.AddressFlags) ||
		 !xmail_native_size(pValue, "headerLineSize",
			pNative->Message.HeaderLineSize, &pNative->Message.HeaderLineSize) ||
		 !xmail_native_uint32(pValue, "wordEncoding",
			(uint32)pNative->Message.WordEncoding, &iWordEncoding) ) {
		goto failed;
	}
	pNative->Message.WordEncoding = (xmailwordencoding)iWordEncoding;
	return true;

failed:
	xmail_native_message_free(pNative);
	return false;
}



/* 接管由 xrtFree 释放的文本并交给语言 Buffer。 */
static xbuffer* xmail_native_take_text(str sText, size_t iSize)
{
	bytes pData = (bytes)sText;
	xbuffer* pBuffer;

	if ( sText == NULL ) {
		return NULL;
	}
	pBuffer = xrtBufferCreateTake(&pData, iSize, iSize + 1u);
	if ( pBuffer == NULL ) {
		xrtFree(sText);
	}
	return pBuffer;
}



/* 验证高层消息对象。 */
bool xmail_message_valid_native(xvalue* pValue)
{
	xmail_native_message Native;
	bool bValid;

	xrtClearError();
	if ( !xmail_native_message_map(pValue, &Native) ) {
		return false;
	}
	bValid = xrtMailMessageValid(&Native.Message);
	xmail_native_message_free(&Native);
	return bValid;
}



/* 构建完整 RFC 消息并把结果交给语言 Buffer。 */
void* xmail_compose_native(xvalue* pValue)
{
	xmail_native_message Native;
	str sOutput;
	size_t iOutputSize;

	xrtClearError();
	if ( !xmail_native_message_map(pValue, &Native) ) {
		return NULL;
	}
	sOutput = xrtMailCompose(&Native.Message, &iOutputSize);
	xmail_native_message_free(&Native);
	return xmail_native_take_text(sOutput, iOutputSize);
}



/* 把一个借用视图复制到动态值。 */
static xvalue* xmail_native_view_value(xstrview Text)
{
	return xrtValueString(Text);
}



/* 设置对象字段并在失败时释放待接管值。 */
static bool xmail_native_set(xvalue* pObject, cstr sName, xvalue* pValue)
{
	return (pValue != NULL) &&
		xrtValueObjectSetNew(pObject, xrtStrView(sName), pValue);
}



/* 把 MIME part 递归转换为独立动态值。 */
static xvalue* xmail_native_part_value(const xmailpart* pPart)
{
	xvalue* pObject;
	xvalue* pChildren;
	size_t i;

	if ( pPart == NULL ) {
		return xrtValueNull();
	}
	pObject = xrtValueObject();
	pChildren = xrtValueArray();
	if ( (pObject == NULL) || (pChildren == NULL) ) {
		xrtValueRelease(pObject);
		xrtValueRelease(pChildren);
		return NULL;
	}
	for ( i = 0; i < pPart->ChildCount; i++ ) {
		xvalue* pChild = xmail_native_part_value(&pPart->Children[i]);

		if ( (pChild == NULL) || !xrtValueArrayAppendNew(pChildren, pChild) ) {
			xrtValueRelease(pChildren);
			xrtValueRelease(pObject);
			return NULL;
		}
	}
	if ( !xmail_native_set(pObject, "mediaType",
			xmail_native_view_value(pPart->ContentType.Type)) ||
		 !xmail_native_set(pObject, "mediaSubtype",
			xmail_native_view_value(pPart->ContentType.Subtype)) ||
		 !xmail_native_set(pObject, "disposition",
			xmail_native_view_value(pPart->Disposition.Type)) ||
		 !xmail_native_set(pObject, "fileName",
			xmail_native_view_value(pPart->FileName)) ||
		 !xmail_native_set(pObject, "contentId",
			xmail_native_view_value(pPart->ContentId)) ||
		 !xmail_native_set(pObject, "headers",
			xmail_native_view_value(pPart->Message.Headers)) ||
		 !xmail_native_set(pObject, "body",
			xmail_native_view_value(pPart->Message.Body)) ||
		 !xmail_native_set(pObject, "data", xrtValueBytes(pPart->Data)) ||
		 !xmail_native_set(pObject, "transfer", xrtValueInt((int64)pPart->Transfer)) ||
		 !xmail_native_set(pObject, "attachment", xrtValueBool(pPart->Attachment)) ||
		 !xmail_native_set(pObject, "inline", xrtValueBool(pPart->Inline)) ||
		 !xmail_native_set(pObject, "decoded", xrtValueBool(pPart->Decoded)) ||
		 !xmail_native_set(pObject, "embedded", xrtValueBool(pPart->Embedded)) ) {
		xrtValueRelease(pChildren);
		xrtValueRelease(pObject);
		return NULL;
	}
	if ( !xmail_native_set(pObject, "children", pChildren) ) {
		xrtValueRelease(pObject);
		return NULL;
	}
	return pObject;
}



/* 把可选 uint64 预算无损映射为 size_t。 */
static bool xmail_native_limit(uint64 iValue, size_t* pResult)
{
	if ( (pResult == NULL) || (iValue > (uint64)SIZE_MAX) ) {
		return xmail_native_fail(XERR_RANGE, "mail parse limit exceeds native size range");
	}
	*pResult = (size_t)iValue;
	return true;
}



/* 解析完整 RFC/MIME 消息并返回独立动态树。 */
xvalue* xmail_parse_native(
	const char* sSource,
	size_t iSourceSize,
	uint64 iMaxDepth,
	uint64 iMaxParts,
	uint64 iMaxSourceBytes,
	uint64 iMaxDecodedBytes
)
{
	xmailtreelimits Limits;
	xmailtree Tree;
	xvalue* pResult;
	xvalue* pRoot;

	xrtClearError();
	xrtMailTreeLimitsInit(&Limits);
	if ( ((iMaxDepth != 0) && !xmail_native_limit(iMaxDepth, &Limits.MaxDepth)) ||
		 ((iMaxParts != 0) && !xmail_native_limit(iMaxParts, &Limits.MaxParts)) ||
		 ((iMaxSourceBytes != 0) &&
			!xmail_native_limit(iMaxSourceBytes, &Limits.MaxSourceBytes)) ||
		 ((iMaxDecodedBytes != 0) &&
			!xmail_native_limit(iMaxDecodedBytes, &Limits.MaxDecodedBytes)) ) {
		return NULL;
	}
	memset(&Tree, 0, sizeof(Tree));
	if ( !xrtMailTreeParse(
			(xstrview){ sSource, iSourceSize },
			&Limits,
			&Tree
	) ) {
		return NULL;
	}
	pRoot = xmail_native_part_value(Tree.Root);
	pResult = xrtValueObject();
	if ( (pRoot == NULL) || (pResult == NULL) ||
		 (Tree.PartCount > (size_t)INT64_MAX) ||
		 (Tree.DecodedBytes > (size_t)INT64_MAX) ||
		 !xmail_native_set(pResult, "partCount",
			xrtValueInt((int64)Tree.PartCount)) ||
		 !xmail_native_set(pResult, "decodedBytes",
			xrtValueInt((int64)Tree.DecodedBytes)) ) {
		xrtValueRelease(pRoot);
		xrtValueRelease(pResult);
		xrtMailTreeFree(&Tree);
		if ( xrtGetError() == NULL ) {
			xmail_native_fail(XERR_RANGE, "mail parse result exceeds xlang integer range");
		}
		return NULL;
	}
	if ( !xmail_native_set(pResult, "root", pRoot) ) {
		xrtValueRelease(pResult);
		xrtMailTreeFree(&Tree);
		return NULL;
	}
	xrtMailTreeFree(&Tree);
	return pResult;
}



#include "src/xlang/xmail_native_protocol.c"
#include "src/xlang/xmail_native_smtp.c"
#include "src/xlang/xmail_native_pop3.c"
#include "src/xlang/xmail_native_imap.c"
