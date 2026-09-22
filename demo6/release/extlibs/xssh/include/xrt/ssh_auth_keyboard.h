#ifndef XRT_SSH_AUTH_KEYBOARD_H
#define XRT_SSH_AUTH_KEYBOARD_H

#include <xrt/ssh_auth_message.h>



#if defined(XSSH_FEATURE_AUTH_KEYBOARD) && \
	!defined(XSSH_FEATURE_AUTH_MESSAGE)
	#error "XSSH_FEATURE_AUTH_KEYBOARD requires XSSH_FEATURE_AUTH_MESSAGE"
#endif



#if defined(XSSH_FEATURE_AUTH_KEYBOARD)

#define XSSH_MSG_USERAUTH_INFO_REQUEST 60u
#define XSSH_MSG_USERAUTH_INFO_RESPONSE 61u



/* Keyboard-interactive 请求借用完整 payload。 */
typedef struct xsshauthkeyboard {
	xstrview User;
	xstrview Language;
	xstrview Submethods;
} xsshauthkeyboard;



/* 单个交互提示借用完整 challenge payload。 */
typedef struct xsshauthkeyboardprompt {
	xstrview Prompt;
	bool Echo;
} xsshauthkeyboardprompt;



/* Challenge 迭代器已在初始化时严格验证全部提示。 */
typedef struct xsshauthkeyboardchallenge {
	xstrview Name;
	xstrview Instruction;
	xstrview Language;
	uint32 Count;
	uint32 Index;
	xsshreader Reader;
} xsshauthkeyboardchallenge;



/* Response 迭代器已在初始化时严格验证全部响应。 */
typedef struct xsshauthkeyboardresponses {
	uint32 Count;
	uint32 Index;
	xsshreader Reader;
} xsshauthkeyboardresponses;



XRT_EXTERN_C_BEGIN



/* 使用空 language tag 写入 ssh-connection keyboard-interactive 请求。 */
XRT_API xsshcode xrtSshAuthKeyboardWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Submethods
);



/* 写入带显式 language tag 的 keyboard-interactive 请求。 */
XRT_API xsshcode xrtSshAuthKeyboardWriteLanguage(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Language,
	xstrview Submethods
);



/* 严格读取 keyboard-interactive 请求。 */
XRT_API xsshcode xrtSshAuthKeyboardRead(
	xbytesview Payload,
	xsshauthkeyboard* pKeyboard
);



/* 写入、验证并迭代服务端交互挑战。 */
XRT_API xsshcode xrtSshAuthKeyboardChallengeWrite(
	xsshwriter* pWriter,
	xstrview Name,
	xstrview Instruction,
	xstrview Language,
	const xsshauthkeyboardprompt* pPrompts,
	size_t iCount
);
XRT_API xsshcode xrtSshAuthKeyboardChallengeRead(
	xbytesview Payload,
	xsshauthkeyboardchallenge* pChallenge
);
XRT_API bool xrtSshAuthKeyboardChallengeNext(
	xsshauthkeyboardchallenge* pChallenge,
	xsshauthkeyboardprompt* pPrompt
);



/* 写入、验证并迭代客户端交互响应。 */
XRT_API xsshcode xrtSshAuthKeyboardResponseWrite(
	xsshwriter* pWriter,
	const xstrview* pResponses,
	size_t iCount
);
XRT_API xsshcode xrtSshAuthKeyboardResponseRead(
	xbytesview Payload,
	xsshauthkeyboardresponses* pResponses
);
XRT_API bool xrtSshAuthKeyboardResponseNext(
	xsshauthkeyboardresponses* pResponses,
	xstrview* pResponse
);



XRT_EXTERN_C_END

#endif

#endif
