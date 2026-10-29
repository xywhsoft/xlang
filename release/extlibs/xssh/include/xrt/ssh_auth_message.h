#ifndef XRT_SSH_AUTH_MESSAGE_H
#define XRT_SSH_AUTH_MESSAGE_H

#include <xrt/charset.h>
#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_AUTH_MESSAGE) && \
	(!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_UNICODE))
	#error "XSSH_FEATURE_AUTH_MESSAGE requires XSSH_FEATURE_WIRE and XRT_FEATURE_UNICODE"
#endif



#if defined(XSSH_FEATURE_AUTH_MESSAGE)

#define XSSH_MSG_USERAUTH_REQUEST 50u
#define XSSH_MSG_USERAUTH_FAILURE 51u
#if !defined(XSSH_MSG_USERAUTH_SUCCESS)
	#define XSSH_MSG_USERAUTH_SUCCESS 52u
#endif
#define XSSH_MSG_USERAUTH_BANNER 53u

#define XSSH_SERVICE_USERAUTH "ssh-userauth"
#define XSSH_SERVICE_CONNECTION "ssh-connection"

#define XSSH_AUTH_METHOD_NONE "none"
#define XSSH_AUTH_METHOD_PASSWORD "password"
#define XSSH_AUTH_METHOD_PUBLICKEY "publickey"
#define XSSH_AUTH_METHOD_HOSTBASED "hostbased"
#define XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE "keyboard-interactive"



/* 通用认证请求借用完整 payload，并保留方法专用原始字段。 */
typedef struct xsshauthrequest {
	xstrview User;
	xstrview Service;
	xstrview Method;
	xbytesview Fields;
} xsshauthrequest;



/* 认证失败消息借用完整 payload。 */
typedef struct xsshauthfailure {
	xstrview Methods;
	bool PartialSuccess;
} xsshauthfailure;



/* 认证横幅消息借用完整 payload。 */
typedef struct xsshauthbanner {
	xstrview Message;
	xstrview Language;
} xsshauthbanner;



XRT_EXTERN_C_BEGIN



/* 计算通用 USERAUTH_REQUEST 总长度，不访问方法字段内容。 */
XRT_API xsshcode xrtSshAuthRequestSize(
	xstrview User,
	xstrview Service,
	xstrview Method,
	size_t iFieldsSize,
	size_t* pSize
);



/* 写入或读取可扩展的通用 USERAUTH_REQUEST。 */
XRT_API xsshcode xrtSshAuthRequestWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Service,
	xstrview Method,
	xbytesview Fields
);
XRT_API xsshcode xrtSshAuthRequestRead(
	xbytesview Payload,
	xsshauthrequest* pRequest
);



/* 使用 ssh-connection 服务写入或读取 none 探测。 */
XRT_API xsshcode xrtSshAuthNoneWrite(
	xsshwriter* pWriter,
	xstrview User
);
XRT_API xsshcode xrtSshAuthNoneRead(
	xbytesview Payload,
	xstrview* pUser
);



/* 写入或严格读取认证失败及可继续方法。 */
XRT_API xsshcode xrtSshAuthFailureWrite(
	xsshwriter* pWriter,
	xstrview Methods,
	bool bPartialSuccess
);
XRT_API xsshcode xrtSshAuthFailureRead(
	xbytesview Payload,
	xsshauthfailure* pFailure
);



/* 写入或严格读取无字段认证成功消息。 */
XRT_API xsshcode xrtSshAuthSuccessWrite(xsshwriter* pWriter);
XRT_API xsshcode xrtSshAuthSuccessRead(xbytesview Payload);



/* 写入或严格读取 UTF-8 认证横幅。 */
XRT_API xsshcode xrtSshAuthBannerWrite(
	xsshwriter* pWriter,
	xstrview Message,
	xstrview Language
);
XRT_API xsshcode xrtSshAuthBannerRead(
	xbytesview Payload,
	xsshauthbanner* pBanner
);



XRT_EXTERN_C_END

#endif

#endif
