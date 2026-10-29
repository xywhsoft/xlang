#include "xl_clock.h"
/* 邮件协议桥共享的网络所有权、deadline 和 TLS 配置适配。 */



typedef struct xmail_native_protocol {
	xvalue* EngineOwner;
} xmail_native_protocol;



typedef struct xmail_native_tls { unsigned reserved; } xmail_native_tls;

/* 把语言毫秒超时转换为 XRT 绝对 deadline；负值表示无限等待。 */
static bool xmail_native_deadline(int64 iTimeoutMilliseconds, XL6_DeadlineV1* pDeadline)
{
	uint64 iTimeout;

	if ( pDeadline == NULL ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail deadline output is null");
	}
	if ( iTimeoutMilliseconds < 0 ) {
		*pDeadline = XL6_DEADLINE_NEVER;
		return true;
	}
	iTimeout = (uint64)iTimeoutMilliseconds;
	*pDeadline = xl6DeadlineAfterMillisecondsV1(iTimeout);
	return true;
}



/* 构造一个不拥有输入内存的文本视图。 */
static xstrview xmail_native_view(cstr sText, size_t iSize)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = iSize;
	return Text;
}



/* 复制借用文本为语言 Buffer 句柄。 */
static xbuffer* xmail_native_copy_text(xstrview Text)
{
	xbytesview Data;

	Data.Data = (cbytes)(Text.Data != NULL ? Text.Data : "");
	Data.Size = Text.Size;
	return xrtBufferFrom(Data);
}



/* 验证语言字符串适合作为要求零结尾的网络主机名。 */
static bool xmail_native_host(cstr sHost, size_t iHostSize)
{
	if ( (sHost == NULL) || (iHostSize == 0) ||
		(iHostSize > XMAIL_NET_HOST_MAX) || (strlen(sHost) != iHostSize) ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail host is empty or contains a null byte");
	}
	return true;
}



/* Engine is protected by the resident call scope during synchronous Open. */
static xnetresolver* xmail_native_protocol_begin(xnetengine* pEngine)
{
	xnetresolver* pResolver;

	if ( pEngine == NULL ) {
		xmail_native_fail(XERR_ARGUMENT, "mail client requires a network engine");
		return NULL;
	}
	pResolver = xrtNetResolverCreate(NULL);
	return pResolver;
}



/* Release the actual managed creator; transports own their native edges. */
static void xmail_native_protocol_release(xmail_native_protocol* pProtocol)
{

	if(pProtocol){xrtValueRelease(pProtocol->EngineOwner);pProtocol->EngineOwner=NULL;}
}



/* 在清理临时资源后恢复原始结构化错误。 */
static void xmail_native_restore_error(xerror* pError)
{
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 为 TLS/STARTTLS 配置创建独立共享 verifier。 */
static bool xmail_native_tls_open(
	void* pTlsConfig,
	xmailnetconfig* pNet,
	xmail_native_tls* pTls
)
{
	if ( (pNet == NULL) || (pTls == NULL) ) {
		return xmail_native_fail(XERR_ARGUMENT, "mail TLS output is null");
	}
	memset(pTls, 0, sizeof(*pTls));
	if ( pNet->Security == XMAIL_SECURITY_PLAIN ) {
		return true;
	}
	if ( pTlsConfig == NULL ) {
		return xmail_native_fail(XERR_ARGUMENT, "secure mail client requires TlsClientContext");
	}
	pNet->Tls = *(const xtlsclientconfig*)pTlsConfig;

	return true;
}



/* 释放一次 Open 调用持有的 TLS 临时引用。 */
static void xmail_native_tls_close(xmail_native_tls* pTls)
{
	if ( pTls == NULL ) {
		return;
	}
	memset(pTls, 0, sizeof(*pTls));
}

/* Frozen physical slots, never flatten retained native transport children. */
static bool xmail_native_protocol_trace(const xmail_native_protocol* protocol,xrtownershipvisitor visit,void* context)
{
    return !protocol->EngineOwner||visit(xrtValueOwnership(protocol->EngineOwner),context);
}
static bool xmail_native_transport_trace(const __xmailtransport* transport,xrtownershipvisitor visit,void* context)
{
    return (!transport->Tcp||visit(xrtNetStreamOwnership(transport->Tcp),context))&&
        (!transport->Tls||visit(xrtTlsStreamOwnership(transport->Tls),context))&&
        (!transport->DeflateInput||visit(xrtNetBytesOwnership(transport->DeflateInput),context));
}
