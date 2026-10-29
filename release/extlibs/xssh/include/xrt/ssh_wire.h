#ifndef XRT_SSH_WIRE_H
#define XRT_SSH_WIRE_H

#include <xrt/memory.h>



#if defined(XSSH_FEATURE_SSH) && \
	(!defined(XSSH_FEATURE_PACKET_CODEC_RANDOM) || \
	 !defined(XSSH_FEATURE_KEXINIT_RANDOM) || \
	 !defined(XSSH_FEATURE_KEX_ECDH) || \
	 !defined(XSSH_FEATURE_KEX_SHA256) || \
	 !defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM) || \
	 !defined(XSSH_FEATURE_HOSTKEY_ED25519) || \
	 !defined(XSSH_FEATURE_KEY_TEXT) || \
	 !defined(XSSH_FEATURE_KNOWN_HOST) || \
	 !defined(XSSH_FEATURE_KNOWN_HOST_HASH) || \
	 !defined(XSSH_FEATURE_KNOWN_HOST_DB) || \
	 !defined(XSSH_FEATURE_FINGERPRINT) || \
	 !defined(XSSH_FEATURE_PRIVATE_KEY) || \
	 !defined(XSSH_FEATURE_PRIVATE_KEY_PEM) || \
	 !defined(XSSH_FEATURE_PRIVATE_KEY_ED25519) || \
	 !defined(XSSH_FEATURE_TRANSPORT_MESSAGE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_REKEY) || \
	 !defined(XSSH_FEATURE_TRANSPORT_STATE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_CORE) || \
	 !defined(XSSH_FEATURE_KEX_SESSION) || \
	 !defined(XSSH_FEATURE_KEX_SESSION_RANDOM) || \
	 !defined(XSSH_FEATURE_KEX_EXCHANGE) || \
	 !defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) || \
	 !defined(XSSH_FEATURE_TRANSPORT_TCP) || \
	 !defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM) || \
	 !defined(XSSH_FEATURE_AUTH_PASSWORD) || \
	 !defined(XSSH_FEATURE_AUTH_PUBLICKEY) || \
	 !defined(XSSH_FEATURE_AUTH_KEYBOARD) || \
	 !defined(XSSH_FEATURE_AUTH_HOSTBASED) || \
	 !defined(XSSH_FEATURE_AUTH_GUARD) || \
	 !defined(XSSH_FEATURE_AUTH_SESSION) || \
	 !defined(XSSH_FEATURE_CONNECTION_MESSAGE) || \
	 !defined(XSSH_FEATURE_CHANNEL_MESSAGE) || \
	 !defined(XSSH_FEATURE_CHANNEL_WINDOW) || \
	 !defined(XSSH_FEATURE_CHANNEL_REQUEST) || \
	 !defined(XSSH_FEATURE_CHANNEL_PTY) || \
	 !defined(XSSH_FEATURE_CHANNEL_STATE) || \
	 !defined(XSSH_FEATURE_CHANNEL_CORE) || \
	 !defined(XSSH_FEATURE_CHANNEL_IO) || \
	 !defined(XSSH_FEATURE_FORWARD_MESSAGE) || \
	 !defined(XSSH_FEATURE_REPLY_QUEUE) || \
	 !defined(XSSH_FEATURE_CONNECTION_SESSION) || \
	 !defined(XSSH_FEATURE_CHANNELS) || \
	 !defined(XSSH_FEATURE_SESSION_CORE) || \
	 !defined(XSSH_FEATURE_SESSION_CORE_RANDOM) || \
	 !defined(XSSH_FEATURE_SESSION_TCP) || \
	 !defined(XSSH_FEATURE_SESSION_READER) || \
	 !defined(XSSH_FEATURE_SESSION_STREAM) || \
	 !defined(XSSH_FEATURE_SESSION_TCP_RANDOM) || \
	 !defined(XSSH_FEATURE_CLIENT_CORE))
	#error "XSSH_FEATURE_SSH requires the complete xssh dependency closure"
#endif



#if defined(XSSH_FEATURE_WIRE)

/* SSH identification 整行上限，包含 CRLF 或兼容性的 LF。 */
#define XSSH_IDENTIFICATION_MAX 255u



/* 无分配协议原语使用稳定结果码；NEED_MORE 不是错误。 */
typedef enum xsshcode {
	XSSH_ERROR_TIMEOUT = -9,
	XSSH_ERROR_STATE = -8,
	XSSH_ERROR_AUTHENTICATION = -7,
	XSSH_ERROR_CALLBACK = -6,
	XSSH_ERROR_PROTOCOL = -5,
	XSSH_ERROR_UNSUPPORTED = -4,
	XSSH_ERROR_OVERFLOW = -3,
	XSSH_ERROR_SPACE = -2,
	XSSH_ERROR_ARGUMENT = -1,
	XSSH_OK = 0,
	XSSH_NEED_MORE = 1
} xsshcode;



/* Reader 借用完整输入，失败的读取不会推进 Position。 */
typedef struct xsshreader {
	xbytesview Source;
	size_t Position;
} xsshreader;



/* Writer 借用调用方缓冲，失败的写入不会推进 Size。 */
typedef struct xsshwriter {
	bytes Data;
	size_t Capacity;
	size_t Size;
} xsshwriter;



XRT_EXTERN_C_BEGIN



/* 初始化借用输入的 SSH reader；空输入允许 Data 为 NULL。 */
XRT_API bool xrtSshReaderInit(xsshreader* pReader, xbytesview Source);



/* 初始化借用输出缓冲的 SSH writer；零容量允许 Data 为 NULL。 */
XRT_API bool xrtSshWriterInit(xsshwriter* pWriter, void* pData, size_t iCapacity);



/* 返回 reader 尚未消费的字节数；无效状态返回零。 */
XRT_API size_t xrtSshReaderRemaining(const xsshreader* pReader);



/* 返回 writer 尚可写入的字节数；无效状态返回零。 */
XRT_API size_t xrtSshWriterRemaining(const xsshwriter* pWriter);



/* 校验 writer 并确认后续写入可一次完成，不改变 writer 状态。 */
XRT_API xsshcode xrtSshWriterReserve(
	const xsshwriter* pWriter,
	size_t iSize
);



/* 校验整段输出与多个借用输入不重叠，不改变 writer 状态。 */
XRT_API xsshcode xrtSshWriterReserveInputs(
	const xsshwriter* pWriter,
	size_t iSize,
	const xbytesview* pInputs,
	size_t iInputCount
);



/* 读取一个 SSH byte，输入不足返回 XSSH_NEED_MORE。 */
XRT_API xsshcode xrtSshReadByte(xsshreader* pReader, uint8* pValue);



/* 读取一个 SSH boolean；零为 false，任意非零值为 true。 */
XRT_API xsshcode xrtSshReadBool(xsshreader* pReader, bool* pValue);



/* 读取一个网络字节序 uint32。 */
XRT_API xsshcode xrtSshReadU32(xsshreader* pReader, uint32* pValue);



/* 读取一个网络字节序 uint64。 */
XRT_API xsshcode xrtSshReadU64(xsshreader* pReader, uint64* pValue);



/* 读取 uint32 长度前缀的 SSH string，并返回借用视图。 */
XRT_API xsshcode xrtSshReadString(xsshreader* pReader, xbytesview* pValue);



/* 读取指定数量的原始字节，并返回借用视图。 */
XRT_API xsshcode xrtSshReadBytes(
	xsshreader* pReader,
	size_t iSize,
	xbytesview* pValue
);



/* 写入一个 SSH byte。 */
XRT_API xsshcode xrtSshWriteByte(xsshwriter* pWriter, uint8 iValue);



/* 写入规范的单字节 SSH boolean。 */
XRT_API xsshcode xrtSshWriteBool(xsshwriter* pWriter, bool bValue);



/* 以网络字节序写入 uint32。 */
XRT_API xsshcode xrtSshWriteU32(xsshwriter* pWriter, uint32 iValue);



/* 以网络字节序写入 uint64。 */
XRT_API xsshcode xrtSshWriteU64(xsshwriter* pWriter, uint64 iValue);



/* 写入 uint32 长度前缀的 SSH string。 */
XRT_API xsshcode xrtSshWriteString(xsshwriter* pWriter, xbytesview Value);



/* 不添加长度前缀，直接写入原始字节。 */
XRT_API xsshcode xrtSshWriteBytes(xsshwriter* pWriter, xbytesview Value);



/* 校验并写入一个 SSH name-list string。 */
XRT_API xsshcode xrtSshWriteNameList(xsshwriter* pWriter, xstrview List);



/* 校验一个非空、可打印 US-ASCII 且不含逗号的 SSH 名称。 */
XRT_API bool xrtSshNameValid(xstrview Name);



/* 校验允许为空且不含控制字符的 ASCII language tag。 */
XRT_API bool xrtSshLanguageValid(xstrview Language);



/* 校验 SSH name-list 的非空项与可打印 US-ASCII 约束。 */
XRT_API bool xrtSshNameListValid(xstrview List);



/* 判断有效 name-list 是否包含一个完整名称。 */
XRT_API bool xrtSshNameListContains(xstrview List, xstrview Name);



/* 判断有效 name-list 是否存在完全相同的重复项。 */
XRT_API bool xrtSshNameListHasDuplicate(xstrview List);



/* 按 Preferred 的顺序返回 Available 中首个匹配名称的借用视图。 */
XRT_API xsshcode xrtSshNameListFirstMatch(
	xstrview Preferred,
	xstrview Available,
	xstrview* pMatch
);



/* 读取规范的非负 SSH mpint，返回原始二进制补码字节视图。 */
XRT_API xsshcode xrtSshReadMpint(xsshreader* pReader, xbytesview* pValue);



/* 将大端无符号 magnitude 规范化并写成非负 SSH mpint。 */
XRT_API xsshcode xrtSshWriteMpint(xsshwriter* pWriter, xbytesview Magnitude);



/* 读取规范的有符号 SSH mpint，并报告其符号。 */
XRT_API xsshcode xrtSshReadSignedMpint(
	xsshreader* pReader,
	xbytesview* pValue,
	bool* pNegative
);



/* 将大端 magnitude 和符号规范化并写成 SSH mpint。 */
XRT_API xsshcode xrtSshWriteSignedMpint(
	xsshwriter* pWriter,
	xbytesview Magnitude,
	bool bNegative
);



/* 从增量输入读取 SSH-2.0/SSH-1.99 identification，可跳过服务端前置行。 */
XRT_API xsshcode xrtSshBannerRead(
	xstrview Data,
	xstrview* pBanner,
	size_t* pConsumed
);



/* 严格校验并写入本端 SSH-2.0 identification 与 CRLF。 */
XRT_API xsshcode xrtSshBannerWrite(
	xsshwriter* pWriter,
	xstrview Banner
);



XRT_EXTERN_C_END

#endif

#endif
