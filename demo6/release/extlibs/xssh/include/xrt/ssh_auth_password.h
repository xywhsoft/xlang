#ifndef XRT_SSH_AUTH_PASSWORD_H
#define XRT_SSH_AUTH_PASSWORD_H

#include <xrt/ssh_auth_message.h>



#if defined(XSSH_FEATURE_AUTH_PASSWORD) && \
	!defined(XSSH_FEATURE_AUTH_MESSAGE)
	#error "XSSH_FEATURE_AUTH_PASSWORD requires XSSH_FEATURE_AUTH_MESSAGE"
#endif



#if defined(XSSH_FEATURE_AUTH_PASSWORD)

#define XSSH_MSG_USERAUTH_PASSWD_CHANGEREQ 60u



/* Password 请求借用完整 payload；Password 是当前或旧密码。 */
typedef struct xsshauthpassword {
	xstrview User;
	bool Change;
	xstrview Password;
	xstrview NewPassword;
} xsshauthpassword;



/* Password 更改提示借用完整 payload。 */
typedef struct xsshauthpasswordprompt {
	xstrview Prompt;
	xstrview Language;
} xsshauthpasswordprompt;



XRT_EXTERN_C_BEGIN



/* 使用 ssh-connection 服务写入普通密码认证请求。 */
XRT_API xsshcode xrtSshAuthPasswordWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password
);



/* 使用 ssh-connection 服务写入旧密码与新密码。 */
XRT_API xsshcode xrtSshAuthPasswordChangeWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password,
	xstrview NewPassword
);



/* 严格读取普通或更改密码的 ssh-connection 请求。 */
XRT_API xsshcode xrtSshAuthPasswordRead(
	xbytesview Payload,
	xsshauthpassword* pPassword
);



/* 写入或严格读取服务端密码更改提示。 */
XRT_API xsshcode xrtSshAuthPasswordPromptWrite(
	xsshwriter* pWriter,
	xstrview Prompt,
	xstrview Language
);
XRT_API xsshcode xrtSshAuthPasswordPromptRead(
	xbytesview Payload,
	xsshauthpasswordprompt* pPrompt
);



XRT_EXTERN_C_END

#endif

#endif
