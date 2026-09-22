#ifndef XRT_SSH_CHANNEL_PTY_H
#define XRT_SSH_CHANNEL_PTY_H

#include <xrt/ssh_channel_request.h>



#if defined(XSSH_FEATURE_CHANNEL_PTY) && \
	!defined(XSSH_FEATURE_CHANNEL_REQUEST)
	#error "XSSH_FEATURE_CHANNEL_PTY requires XSSH_FEATURE_CHANNEL_REQUEST"
#endif



#if defined(XSSH_FEATURE_CHANNEL_PTY)

#define XSSH_CHANNEL_REQUEST_PTY "pty-req"

#define XSSH_TTY_OP_END 0u
#define XSSH_TTY_OP_VINTR 1u
#define XSSH_TTY_OP_VQUIT 2u
#define XSSH_TTY_OP_VERASE 3u
#define XSSH_TTY_OP_VKILL 4u
#define XSSH_TTY_OP_VEOF 5u
#define XSSH_TTY_OP_VEOL 6u
#define XSSH_TTY_OP_VEOL2 7u
#define XSSH_TTY_OP_VSTART 8u
#define XSSH_TTY_OP_VSTOP 9u
#define XSSH_TTY_OP_VSUSP 10u
#define XSSH_TTY_OP_VDSUSP 11u
#define XSSH_TTY_OP_VREPRINT 12u
#define XSSH_TTY_OP_VWERASE 13u
#define XSSH_TTY_OP_VLNEXT 14u
#define XSSH_TTY_OP_VFLUSH 15u
#define XSSH_TTY_OP_VSWTCH 16u
#define XSSH_TTY_OP_VSTATUS 17u
#define XSSH_TTY_OP_VDISCARD 18u
#define XSSH_TTY_OP_IGNPAR 30u
#define XSSH_TTY_OP_PARMRK 31u
#define XSSH_TTY_OP_INPCK 32u
#define XSSH_TTY_OP_ISTRIP 33u
#define XSSH_TTY_OP_INLCR 34u
#define XSSH_TTY_OP_IGNCR 35u
#define XSSH_TTY_OP_ICRNL 36u
#define XSSH_TTY_OP_IUCLC 37u
#define XSSH_TTY_OP_IXON 38u
#define XSSH_TTY_OP_IXANY 39u
#define XSSH_TTY_OP_IXOFF 40u
#define XSSH_TTY_OP_IMAXBEL 41u
#define XSSH_TTY_OP_ISIG 50u
#define XSSH_TTY_OP_ICANON 51u
#define XSSH_TTY_OP_XCASE 52u
#define XSSH_TTY_OP_ECHO 53u
#define XSSH_TTY_OP_ECHOE 54u
#define XSSH_TTY_OP_ECHOK 55u
#define XSSH_TTY_OP_ECHONL 56u
#define XSSH_TTY_OP_NOFLSH 57u
#define XSSH_TTY_OP_TOSTOP 58u
#define XSSH_TTY_OP_IEXTEN 59u
#define XSSH_TTY_OP_ECHOCTL 60u
#define XSSH_TTY_OP_ECHOKE 61u
#define XSSH_TTY_OP_PENDIN 62u
#define XSSH_TTY_OP_OPOST 70u
#define XSSH_TTY_OP_OLCUC 71u
#define XSSH_TTY_OP_ONLCR 72u
#define XSSH_TTY_OP_OCRNL 73u
#define XSSH_TTY_OP_ONOCR 74u
#define XSSH_TTY_OP_ONLRET 75u
#define XSSH_TTY_OP_CS7 90u
#define XSSH_TTY_OP_CS8 91u
#define XSSH_TTY_OP_PARENB 92u
#define XSSH_TTY_OP_PARODD 93u
#define XSSH_TTY_OP_ISPEED 128u
#define XSSH_TTY_OP_OSPEED 129u
#define XSSH_TTY_OP_UNSUPPORTED_MIN 160u



/* 单个 terminal mode 保留 opcode 和 uint32 参数。 */
typedef struct xsshterminalmode {
	uint8 Opcode;
	uint32 Value;
} xsshterminalmode;



/* 已完整验证的 terminal mode 借用迭代器，没有固定数量上限。 */
typedef struct xsshterminalmodes {
	xsshreader Reader;
	size_t Count;
	size_t Index;
	bool Unsupported;
} xsshterminalmodes;



/* PTY request 借用终端名称和原始 mode stream。 */
typedef struct xsshchannelpty {
	xbytesview Terminal;
	uint32 Columns;
	uint32 Rows;
	uint32 PixelWidth;
	uint32 PixelHeight;
	xbytesview Modes;
} xsshchannelpty;



XRT_EXTERN_C_BEGIN



/* 向调用方缓冲追加一个 opcode/value terminal mode。 */
XRT_API xsshcode xrtSshTerminalModeWrite(
	xsshwriter* pWriter,
	uint8 iOpcode,
	uint32 iValue
);



/* 向 terminal mode stream 追加 TTY_OP_END。 */
XRT_API xsshcode xrtSshTerminalModeEnd(xsshwriter* pWriter);



/* 完整验证并初始化无固定数量上限的 terminal mode 迭代器。 */
XRT_API xsshcode xrtSshTerminalModesRead(
	xbytesview Modes,
	xsshterminalmodes* pModes
);



/* 返回下一项已验证 terminal mode；迭代结束返回 false。 */
XRT_API bool xrtSshTerminalModesNext(
	xsshterminalmodes* pModes,
	xsshterminalmode* pMode
);



/* 写入或严格读取 PTY request。 */
XRT_API xsshcode xrtSshChannelPtyWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Terminal,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight,
	xbytesview Modes
);
XRT_API xsshcode xrtSshChannelPtyRead(
	const xsshchannelrequest* pRequest,
	xsshchannelpty* pPty
);



XRT_EXTERN_C_END

#endif

#endif
