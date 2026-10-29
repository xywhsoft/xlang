#include <limits.h>
#include <string.h>



#define XSSH_XLANG_DOMAIN "xlang.extlib.xssh"



typedef struct xssh_native_writer {
	xbuffer* Buffer;
	xsshwriter Writer;
} xssh_native_writer;



typedef struct xssh_native_reader {
	xbuffer* Source;
	xsshreader Reader;
	xbytesview Last;
	uint64 Scalar;
	bool Negative;
} xssh_native_reader;



typedef struct xssh_native_banner {
	xbuffer* Source;
	xstrview Banner;
	size_t Consumed;
	xsshcode Code;
} xssh_native_banner;



typedef struct xssh_native_key_line {
	xbuffer* Source;
	xsshopensshkeyline Line;
} xssh_native_key_line;



typedef struct xssh_native_known_host {
	xbuffer* Source;
	xsshknownhostline Line;
} xssh_native_known_host;



/* 返回稳定结果码名称，便于语言层保留 NEED_MORE 与错误的区别。 */
const char* xssh_code_name(int iCode)
{
	switch ( (xsshcode)iCode ) {
		case XSSH_ERROR_TIMEOUT: return "timeout";
		case XSSH_ERROR_STATE: return "state";
		case XSSH_ERROR_AUTHENTICATION: return "authentication";
		case XSSH_ERROR_CALLBACK: return "callback";
		case XSSH_ERROR_PROTOCOL: return "protocol";
		case XSSH_ERROR_UNSUPPORTED: return "unsupported";
		case XSSH_ERROR_OVERFLOW: return "overflow";
		case XSSH_ERROR_SPACE: return "space";
		case XSSH_ERROR_ARGUMENT: return "argument";
		case XSSH_OK: return "ok";
		case XSSH_NEED_MORE: return "need_more";
		default: return "unknown";
	}
}



/* 把协议结果码映射为 XRT 结构化错误类别。 */
static xerrkind xsshNativeErrorKind(xsshcode Code)
{
	switch ( Code ) {
		case XSSH_ERROR_TIMEOUT: return XERR_TIMEOUT;
		case XSSH_ERROR_AUTHENTICATION: return XERR_PERMISSION;
		case XSSH_ERROR_PROTOCOL: return XERR_PROTOCOL;
		case XSSH_ERROR_UNSUPPORTED: return XERR_UNSUPPORTED;
		case XSSH_ERROR_OVERFLOW:
		case XSSH_ERROR_SPACE: return XERR_RANGE;
		case XSSH_ERROR_ARGUMENT: return XERR_ARGUMENT;
		case XSSH_ERROR_CALLBACK:
		case XSSH_ERROR_STATE:
		default: return XERR_STATE;
	}
}



/* 发布一次失败；已有的内存或系统错误优先保留。 */
static xsshcode xsshNativePublish(xsshcode Code, cstr sOperation)
{
	if ( (Code < XSSH_OK) && (xrtGetError() == NULL) ) {
		xrtSetErrorInfo(
			xsshNativeErrorKind(Code),
			XSSH_XLANG_DOMAIN,
			(int32)Code,
			sOperation
		);
	}
	return Code;
}



/* 返回当前线程的结构化错误文本。 */
const char* xssh_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	return (sMessage != NULL) && (sMessage[0] != 0) ?
		sMessage : "xssh operation failed";
}



/* 复制借用字节视图并交给语言 Buffer。 */
static xbuffer* xsshNativeCopyBytes(xbytesview Data)
{
	return xrtBufferFrom(Data);
}



/* 复制借用文本视图并交给语言 Buffer。 */
static xbuffer* xsshNativeCopyText(xstrview Text)
{
	xbytesview Data;

	Data.Data = (const unsigned char*)Text.Data;
	Data.Size = Text.Size;
	return xsshNativeCopyBytes(Data);
}



/* 创建自动扩容的 SSH wire writer。 */
void* xssh_writer_create_native(void)
{
	xssh_native_writer* pNative;

	xrtClearError();
	pNative = (xssh_native_writer*)xrtCalloc(1u, sizeof(*pNative));
	if ( pNative == NULL ) {
		return NULL;
	}
	pNative->Buffer = xrtBufferCreate();
	if ( (pNative->Buffer == NULL) ||
		 !xrtSshWriterInit(&pNative->Writer, NULL, 0u) ) {
		xrtBufferDestroy(pNative->Buffer);
		xrtFree(pNative);
		return NULL;
	}
	return pNative;
}



/* 释放 writer 及其动态缓冲。 */
void xssh_writer_destroy_native(void* pHandle)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;

	if ( pNative == NULL ) {
		return;
	}
	xrtBufferDestroy(pNative->Buffer);
	xrtFree(pNative);
}



/* 为一次原子写入准备空间并重建借用 writer。 */
static xsshcode xsshNativeWriterPrepare(
	xssh_native_writer* pNative,
	size_t iExtra
)
{
	size_t iRequired;

	xrtClearError();
	if ( (pNative == NULL) || (pNative->Buffer == NULL) ) {
		return xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh writer is null");
	}
	if ( iExtra > (SIZE_MAX - pNative->Buffer->Size) ) {
		return xsshNativePublish(XSSH_ERROR_OVERFLOW, "xssh writer size overflow");
	}
	iRequired = pNative->Buffer->Size + iExtra;
	if ( !xrtBufferReserve(pNative->Buffer, iRequired) ||
		 !xrtSshWriterInit(
			&pNative->Writer,
			pNative->Buffer->Data,
			pNative->Buffer->Capacity
		) ) {
		return xsshNativePublish(XSSH_ERROR_STATE, "xssh writer reserve failed");
	}
	pNative->Writer.Size = pNative->Buffer->Size;
	return XSSH_OK;
}



/* 提交一次成功写入，并统一发布失败。 */
static int xsshNativeWriterCommit(
	xssh_native_writer* pNative,
	xsshcode Code,
	cstr sOperation
)
{
	if ( Code == XSSH_OK ) {
		pNative->Buffer->Size = pNative->Writer.Size;
	} else {
		xsshNativePublish(Code, sOperation);
	}
	return (int)Code;
}



/* 清空 writer，但保留已分配容量。 */
bool xssh_writer_clear_native(void* pHandle)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;

	if ( (pNative == NULL) || (pNative->Buffer == NULL) ) {
		return false;
	}
	xrtBufferClear(pNative->Buffer);
	return xrtSshWriterInit(
		&pNative->Writer,
		pNative->Buffer->Data,
		pNative->Buffer->Capacity
	);
}



/* 返回 writer 当前字节数。 */
int64 xssh_writer_size_native(void* pHandle)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;

	if ( (pNative == NULL) || (pNative->Buffer == NULL) ||
		 (pNative->Buffer->Size > (size_t)INT64_MAX) ) {
		return -1;
	}
	return (int64)pNative->Buffer->Size;
}



/* 返回 writer 内容的独立快照。 */
void* xssh_writer_snapshot_native(void* pHandle)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;

	if ( (pNative == NULL) || (pNative->Buffer == NULL) ) {
		return NULL;
	}
	return xsshNativeCopyBytes(xrtBufferView(pNative->Buffer));
}



/* 公开底层借用 writer；扩容后地址仍由包装对象重新发布。 */
void* xssh_writer_handle_native(void* pHandle)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;

	return pNative != NULL ? &pNative->Writer : NULL;
}



/* 写入 byte。 */
int xssh_writer_byte_native(void* pHandle, uint32 iValue)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code = xsshNativeWriterPrepare(pNative, 1u);

	if ( Code == XSSH_OK ) {
		Code = iValue <= UINT8_MAX ?
			xrtSshWriteByte(&pNative->Writer, (uint8)iValue) :
			XSSH_ERROR_ARGUMENT;
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh byte write failed");
}



/* 写入 boolean。 */
int xssh_writer_bool_native(void* pHandle, bool bValue)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code = xsshNativeWriterPrepare(pNative, 1u);

	if ( Code == XSSH_OK ) {
		Code = xrtSshWriteBool(&pNative->Writer, bValue);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh boolean write failed");
}



/* 写入网络字节序 uint32。 */
int xssh_writer_u32_native(void* pHandle, uint32 iValue)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code = xsshNativeWriterPrepare(pNative, 4u);

	if ( Code == XSSH_OK ) {
		Code = xrtSshWriteU32(&pNative->Writer, iValue);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh uint32 write failed");
}



/* 写入网络字节序 uint64。 */
int xssh_writer_u64_native(void* pHandle, uint64 iValue)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code = xsshNativeWriterPrepare(pNative, 8u);

	if ( Code == XSSH_OK ) {
		Code = xrtSshWriteU64(&pNative->Writer, iValue);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh uint64 write failed");
}



/* 写入带 uint32 长度前缀的 SSH string。 */
int xssh_writer_string_native(
	void* pHandle,
	const void* pData,
	size_t iSize
)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code;

	Code = iSize <= (SIZE_MAX - 4u) ?
		xsshNativeWriterPrepare(pNative, iSize + 4u) : XSSH_ERROR_OVERFLOW;
	if ( Code == XSSH_OK ) {
		Code = xrtSshWriteString(
			&pNative->Writer,
			(xbytesview){ (const unsigned char*)pData, iSize }
		);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh string write failed");
}



/* 为 XLang string ABI 提供独立符号，语义与字节 string 完全一致。 */
int xssh_writer_string_text_native(
	void* pHandle,
	const char* sData,
	size_t iSize
)
{
	return xssh_writer_string_native(pHandle, sData, iSize);
}



/* 不添加长度前缀，直接写入原始字节。 */
int xssh_writer_raw_native(
	void* pHandle,
	const void* pData,
	size_t iSize
)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code = xsshNativeWriterPrepare(pNative, iSize);

	if ( Code == XSSH_OK ) {
		Code = xrtSshWriteBytes(
			&pNative->Writer,
			(xbytesview){ (const unsigned char*)pData, iSize }
		);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh raw write failed");
}



/* 为 XLang string ABI 提供独立原始写入符号。 */
int xssh_writer_raw_text_native(
	void* pHandle,
	const char* sData,
	size_t iSize
)
{
	return xssh_writer_raw_native(pHandle, sData, iSize);
}



/* 校验并写入 name-list。 */
int xssh_writer_name_list_native(
	void* pHandle,
	const char* sList,
	size_t iSize
)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code;

	Code = iSize <= (SIZE_MAX - 4u) ?
		xsshNativeWriterPrepare(pNative, iSize + 4u) : XSSH_ERROR_OVERFLOW;
	if ( Code == XSSH_OK ) {
		Code = xrtSshWriteNameList(
			&pNative->Writer,
			(xstrview){ sList, iSize }
		);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh name-list write failed");
}



/* 规范化并写入非负 mpint。 */
int xssh_writer_mpint_native(
	void* pHandle,
	const void* pData,
	size_t iSize
)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code;

	Code = iSize <= (SIZE_MAX - 5u) ?
		xsshNativeWriterPrepare(pNative, iSize + 5u) : XSSH_ERROR_OVERFLOW;
	if ( Code == XSSH_OK ) {
		Code = xrtSshWriteMpint(
			&pNative->Writer,
			(xbytesview){ (const unsigned char*)pData, iSize }
		);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh mpint write failed");
}



/* 规范化并写入有符号 mpint。 */
int xssh_writer_signed_mpint_native(
	void* pHandle,
	const void* pData,
	size_t iSize,
	bool bNegative
)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code;

	Code = iSize <= (SIZE_MAX - 5u) ?
		xsshNativeWriterPrepare(pNative, iSize + 5u) : XSSH_ERROR_OVERFLOW;
	if ( Code == XSSH_OK ) {
		Code = xrtSshWriteSignedMpint(
			&pNative->Writer,
			(xbytesview){ (const unsigned char*)pData, iSize },
			bNegative
		);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh signed mpint write failed");
}



/* 写入严格 SSH identification。 */
int xssh_writer_banner_native(
	void* pHandle,
	const char* sBanner,
	size_t iSize
)
{
	xssh_native_writer* pNative = (xssh_native_writer*)pHandle;
	xsshcode Code;

	Code = iSize <= (SIZE_MAX - 2u) ?
		xsshNativeWriterPrepare(pNative, iSize + 2u) : XSSH_ERROR_OVERFLOW;
	if ( Code == XSSH_OK ) {
		Code = xrtSshBannerWrite(
			&pNative->Writer,
			(xstrview){ sBanner, iSize }
		);
	}
	return xsshNativeWriterCommit(pNative, Code, "xssh banner write failed");
}



/* 创建持有输入副本的 SSH wire reader。 */
void* xssh_reader_create_native(const void* pData, size_t iSize)
{
	xssh_native_reader* pNative;

	xrtClearError();
	pNative = (xssh_native_reader*)xrtCalloc(1u, sizeof(*pNative));
	if ( pNative == NULL ) {
		return NULL;
	}
	pNative->Source = xrtBufferFrom(
		(xbytesview){ (const unsigned char*)pData, iSize }
	);
	if ( (pNative->Source == NULL) || !xrtSshReaderInit(
		&pNative->Reader,
		xrtBufferView(pNative->Source)
	) ) {
		xrtBufferDestroy(pNative->Source);
		xrtFree(pNative);
		return NULL;
	}
	return pNative;
}



/* 为 XLang string ABI 提供独立 reader 创建符号。 */
void* xssh_reader_create_text_native(const char* sData, size_t iSize)
{
	return xssh_reader_create_native(sData, iSize);
}



/* 接管已经创建的 reader 句柄。 */
void* xssh_reader_adopt_native(void* pHandle)
{
	return pHandle;
}



/* 释放 reader 及其输入副本。 */
void xssh_reader_destroy_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;

	if ( pNative == NULL ) {
		return;
	}
	xrtBufferDestroy(pNative->Source);
	xrtFree(pNative);
}



/* 返回 reader 剩余字节数。 */
int64 xssh_reader_remaining_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	size_t iRemaining;

	if ( pNative == NULL ) {
		return -1;
	}
	iRemaining = xrtSshReaderRemaining(&pNative->Reader);
	return iRemaining <= (size_t)INT64_MAX ? (int64)iRemaining : -1;
}



/* 公开底层 reader，所有权仍属于包装对象。 */
void* xssh_reader_handle_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;

	return pNative != NULL ? &pNative->Reader : NULL;
}



/* 读取 byte 并保存标量结果。 */
int xssh_reader_byte_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	uint8 iValue;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh reader is null");
	}
	Code = xrtSshReadByte(&pNative->Reader, &iValue);
	if ( Code == XSSH_OK ) {
		pNative->Scalar = iValue;
	}
	return (int)xsshNativePublish(Code, "xssh byte read failed");
}



/* 读取 boolean 并保存标量结果。 */
int xssh_reader_bool_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	bool bValue;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh reader is null");
	}
	Code = xrtSshReadBool(&pNative->Reader, &bValue);
	if ( Code == XSSH_OK ) {
		pNative->Scalar = bValue ? 1u : 0u;
	}
	return (int)xsshNativePublish(Code, "xssh boolean read failed");
}



/* 读取 uint32 并保存标量结果。 */
int xssh_reader_u32_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	uint32 iValue;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh reader is null");
	}
	Code = xrtSshReadU32(&pNative->Reader, &iValue);
	if ( Code == XSSH_OK ) {
		pNative->Scalar = iValue;
	}
	return (int)xsshNativePublish(Code, "xssh uint32 read failed");
}



/* 读取 uint64 并保存标量结果。 */
int xssh_reader_u64_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	uint64 iValue;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh reader is null");
	}
	Code = xrtSshReadU64(&pNative->Reader, &iValue);
	if ( Code == XSSH_OK ) {
		pNative->Scalar = iValue;
	}
	return (int)xsshNativePublish(Code, "xssh uint64 read failed");
}



/* 读取 SSH string 并保存借用视图。 */
int xssh_reader_string_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	xbytesview Value;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh reader is null");
	}
	Code = xrtSshReadString(&pNative->Reader, &Value);
	if ( Code == XSSH_OK ) {
		pNative->Last = Value;
	}
	return (int)xsshNativePublish(Code, "xssh string read failed");
}



/* 读取指定数量原始字节并保存借用视图。 */
int xssh_reader_raw_native(void* pHandle, uint64 iSize)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	xbytesview Value;
	xsshcode Code;

	xrtClearError();
	if ( (pNative == NULL) || (iSize > (uint64)SIZE_MAX) ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh raw size is invalid");
	}
	Code = xrtSshReadBytes(&pNative->Reader, (size_t)iSize, &Value);
	if ( Code == XSSH_OK ) {
		pNative->Last = Value;
	}
	return (int)xsshNativePublish(Code, "xssh raw read failed");
}



/* 读取规范非负 mpint 并保存借用视图。 */
int xssh_reader_mpint_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	xbytesview Value;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh reader is null");
	}
	Code = xrtSshReadMpint(&pNative->Reader, &Value);
	if ( Code == XSSH_OK ) {
		pNative->Last = Value;
		pNative->Negative = false;
	}
	return (int)xsshNativePublish(Code, "xssh mpint read failed");
}



/* 读取规范有符号 mpint 并保存视图与符号。 */
int xssh_reader_signed_mpint_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;
	xbytesview Value;
	bool bNegative;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "xssh reader is null");
	}
	Code = xrtSshReadSignedMpint(&pNative->Reader, &Value, &bNegative);
	if ( Code == XSSH_OK ) {
		pNative->Last = Value;
		pNative->Negative = bNegative;
	}
	return (int)xsshNativePublish(Code, "xssh signed mpint read failed");
}



/* 返回最近一次标量读取结果。 */
uint64 xssh_reader_scalar_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;

	return pNative != NULL ? pNative->Scalar : 0u;
}



/* 返回最近一次 mpint 的符号。 */
bool xssh_reader_negative_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;

	return (pNative != NULL) && pNative->Negative;
}



/* 返回最近一次视图读取结果的独立副本。 */
void* xssh_reader_last_native(void* pHandle)
{
	xssh_native_reader* pNative = (xssh_native_reader*)pHandle;

	return pNative != NULL ? xsshNativeCopyBytes(pNative->Last) : NULL;
}



/* 校验 SSH 名称。 */
bool xssh_name_valid_native(const char* sName, size_t iSize)
{
	return xrtSshNameValid((xstrview){ sName, iSize });
}



/* 校验 SSH language tag。 */
bool xssh_language_valid_native(const char* sLanguage, size_t iSize)
{
	return xrtSshLanguageValid((xstrview){ sLanguage, iSize });
}



/* 校验 SSH name-list。 */
bool xssh_name_list_valid_native(const char* sList, size_t iSize)
{
	return xrtSshNameListValid((xstrview){ sList, iSize });
}



/* 判断 name-list 是否包含完整名称。 */
bool xssh_name_list_contains_native(
	const char* sList,
	size_t iListSize,
	const char* sName,
	size_t iNameSize
)
{
	return xrtSshNameListContains(
		(xstrview){ sList, iListSize },
		(xstrview){ sName, iNameSize }
	);
}



/* 判断 name-list 是否包含重复项。 */
bool xssh_name_list_duplicate_native(const char* sList, size_t iSize)
{
	return xrtSshNameListHasDuplicate((xstrview){ sList, iSize });
}



/* 按本地首选顺序返回首个协商结果；无匹配返回空 Buffer。 */
void* xssh_name_list_first_native(
	const char* sPreferred,
	size_t iPreferredSize,
	const char* sAvailable,
	size_t iAvailableSize
)
{
	xstrview Match;
	xsshcode Code;

	xrtClearError();
	Code = xrtSshNameListFirstMatch(
		(xstrview){ sPreferred, iPreferredSize },
		(xstrview){ sAvailable, iAvailableSize },
		&Match
	);
	if ( Code == XSSH_ERROR_UNSUPPORTED ) {
		return xsshNativeCopyBytes((xbytesview){ NULL, 0u });
	}
	if ( Code != XSSH_OK ) {
		xsshNativePublish(Code, "xssh name-list negotiation failed");
		return NULL;
	}
	return xsshNativeCopyText(Match);
}



/* 创建保留完整状态的增量 banner 解析结果。 */
void* xssh_banner_create_native(const char* sData, size_t iSize)
{
	xssh_native_banner* pNative;
	xstrview Data;

	xrtClearError();
	pNative = (xssh_native_banner*)xrtCalloc(1u, sizeof(*pNative));
	if ( pNative == NULL ) {
		return NULL;
	}
	pNative->Source = xrtBufferFrom(
		(xbytesview){ (const unsigned char*)sData, iSize }
	);
	if ( pNative->Source == NULL ) {
		xrtFree(pNative);
		return NULL;
	}
	Data.Data = (const char*)pNative->Source->Data;
	Data.Size = pNative->Source->Size;
	pNative->Code = xrtSshBannerRead(
		Data,
		&pNative->Banner,
		&pNative->Consumed
	);
	return pNative;
}



/* 释放 banner 解析结果。 */
void xssh_banner_destroy_native(void* pHandle)
{
	xssh_native_banner* pNative = (xssh_native_banner*)pHandle;

	if ( pNative == NULL ) {
		return;
	}
	xrtBufferDestroy(pNative->Source);
	xrtFree(pNative);
}



/* 返回 banner 解析状态。 */
int xssh_banner_status_native(void* pHandle)
{
	xssh_native_banner* pNative = (xssh_native_banner*)pHandle;

	return pNative != NULL ? (int)pNative->Code : (int)XSSH_ERROR_ARGUMENT;
}



/* 返回完整 banner 的独立副本。 */
void* xssh_banner_text_native(void* pHandle)
{
	xssh_native_banner* pNative = (xssh_native_banner*)pHandle;

	if ( (pNative == NULL) || (pNative->Code != XSSH_OK) ) {
		return NULL;
	}
	return xsshNativeCopyText(pNative->Banner);
}



/* 返回 banner 解析已消费字节数。 */
int64 xssh_banner_consumed_native(void* pHandle)
{
	xssh_native_banner* pNative = (xssh_native_banner*)pHandle;

	if ( (pNative == NULL) || (pNative->Code != XSSH_OK) ||
		 (pNative->Consumed > (size_t)INT64_MAX) ) {
		return -1;
	}
	return (int64)pNative->Consumed;
}



/* 解析并持有 OpenSSH 公钥文本行。 */
void* xssh_key_line_create_native(const char* sLine, size_t iSize)
{
	xssh_native_key_line* pNative;
	xstrview Line;
	xsshcode Code;

	xrtClearError();
	pNative = (xssh_native_key_line*)xrtCalloc(1u, sizeof(*pNative));
	if ( pNative == NULL ) {
		return NULL;
	}
	pNative->Source = xrtBufferFrom(
		(xbytesview){ (const unsigned char*)sLine, iSize }
	);
	if ( pNative->Source == NULL ) {
		xrtFree(pNative);
		return NULL;
	}
	Line.Data = (const char*)pNative->Source->Data;
	Line.Size = pNative->Source->Size;
	Code = xrtSshPublicKeyLineRead(Line, &pNative->Line);
	if ( Code != XSSH_OK ) {
		xrtBufferDestroy(pNative->Source);
		xrtFree(pNative);
		xsshNativePublish(Code, "OpenSSH public-key line parse failed");
		return NULL;
	}
	return pNative;
}



/* 释放 OpenSSH 公钥文本行。 */
void xssh_key_line_destroy_native(void* pHandle)
{
	xssh_native_key_line* pNative = (xssh_native_key_line*)pHandle;

	if ( pNative == NULL ) {
		return;
	}
	xrtBufferDestroy(pNative->Source);
	xrtFree(pNative);
}



/* 返回公钥行 options。 */
void* xssh_key_line_options_native(void* pHandle)
{
	xssh_native_key_line* pNative = (xssh_native_key_line*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Options) : NULL;
}



/* 返回公钥行算法。 */
void* xssh_key_line_algorithm_native(void* pHandle)
{
	xssh_native_key_line* pNative = (xssh_native_key_line*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Algorithm) : NULL;
}



/* 返回公钥行 Base64。 */
void* xssh_key_line_base64_native(void* pHandle)
{
	xssh_native_key_line* pNative = (xssh_native_key_line*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Base64) : NULL;
}



/* 返回公钥行注释。 */
void* xssh_key_line_comment_native(void* pHandle)
{
	xssh_native_key_line* pNative = (xssh_native_key_line*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Comment) : NULL;
}



/* 解码完整公钥 blob。 */
void* xssh_key_line_decode_native(void* pHandle)
{
	xssh_native_key_line* pNative = (xssh_native_key_line*)pHandle;
	xsshpublickey PublicKey;
	xbuffer* pBlob;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		xsshNativePublish(XSSH_ERROR_ARGUMENT, "OpenSSH public-key line is null");
		return NULL;
	}
	pBlob = xrtBufferCreate();
	if ( (pBlob == NULL) || !xrtBufferResize(pBlob, pNative->Line.BlobSize) ) {
		xrtBufferDestroy(pBlob);
		return NULL;
	}
	Code = xrtSshPublicKeyLineDecode(
		&pNative->Line,
		pBlob->Data,
		pBlob->Size,
		&PublicKey
	);
	if ( Code != XSSH_OK ) {
		xrtBufferDestroy(pBlob);
		xsshNativePublish(Code, "OpenSSH public-key line decode failed");
		return NULL;
	}
	return pBlob;
}



/* 比较公钥行与完整原始公钥 blob。 */
int xssh_key_line_match_native(
	void* pHandle,
	const void* pBlob,
	size_t iSize
)
{
	xssh_native_key_line* pNative = (xssh_native_key_line*)pHandle;
	bool bMatch = false;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(
			XSSH_ERROR_ARGUMENT,
			"OpenSSH public-key line is null"
		);
	}
	Code = xrtSshPublicKeyLineMatch(
		&pNative->Line,
		(xbytesview){ (const unsigned char*)pBlob, iSize },
		&bMatch
	);
	if ( Code != XSSH_OK ) {
		return (int)xsshNativePublish(Code, "OpenSSH public-key match failed");
	}
	return bMatch ? 1 : 0;
}



/* 返回完整 host-key blob 的 OpenSSH SHA-256 指纹。 */
void* xssh_fingerprint_native(const void* pBlob, size_t iSize)
{
	xbytesview Blob = { (const unsigned char*)pBlob, iSize };
	xbuffer* pResult;
	size_t iOutputSize;
	xsshcode Code;

	xrtClearError();
	Code = xrtSshHostKeyFingerprintSha256(Blob, NULL, 0u, &iOutputSize);
	if ( Code != XSSH_OK ) {
		xsshNativePublish(Code, "SSH host-key fingerprint query failed");
		return NULL;
	}
	pResult = xrtBufferCreate();
	if ( (pResult == NULL) || (iOutputSize == SIZE_MAX) ||
		 !xrtBufferResize(pResult, iOutputSize + 1u) ) {
		xrtBufferDestroy(pResult);
		return NULL;
	}
	Code = xrtSshHostKeyFingerprintSha256(
		Blob,
		(char*)pResult->Data,
		pResult->Size,
		&iOutputSize
	);
	if ( (Code != XSSH_OK) || !xrtBufferResize(pResult, iOutputSize) ) {
		xrtBufferDestroy(pResult);
		xsshNativePublish(Code, "SSH host-key fingerprint failed");
		return NULL;
	}
	return pResult;
}



/* 解析并持有 known_hosts 行。 */
void* xssh_known_host_create_native(const char* sLine, size_t iSize)
{
	xssh_native_known_host* pNative;
	xstrview Line;
	xsshcode Code;

	xrtClearError();
	pNative = (xssh_native_known_host*)xrtCalloc(1u, sizeof(*pNative));
	if ( pNative == NULL ) {
		return NULL;
	}
	pNative->Source = xrtBufferFrom(
		(xbytesview){ (const unsigned char*)sLine, iSize }
	);
	if ( pNative->Source == NULL ) {
		xrtFree(pNative);
		return NULL;
	}
	Line.Data = (const char*)pNative->Source->Data;
	Line.Size = pNative->Source->Size;
	Code = xrtSshKnownHostLineRead(Line, &pNative->Line);
	if ( Code != XSSH_OK ) {
		xrtBufferDestroy(pNative->Source);
		xrtFree(pNative);
		xsshNativePublish(Code, "known_hosts line parse failed");
		return NULL;
	}
	return pNative;
}



/* 释放 known_hosts 行。 */
void xssh_known_host_destroy_native(void* pHandle)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;

	if ( pNative == NULL ) {
		return;
	}
	xrtBufferDestroy(pNative->Source);
	xrtFree(pNative);
}



/* 返回 known_hosts marker。 */
void* xssh_known_host_marker_native(void* pHandle)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Marker) : NULL;
}



/* 返回 known_hosts marker 类型。 */
int xssh_known_host_marker_kind_native(void* pHandle)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;

	return pNative != NULL ? (int)pNative->Line.MarkerKind : -1;
}



/* 返回 known_hosts host patterns。 */
void* xssh_known_host_hosts_native(void* pHandle)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Hosts) : NULL;
}



/* 返回 known_hosts 公钥算法。 */
void* xssh_known_host_algorithm_native(void* pHandle)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Algorithm) : NULL;
}



/* 返回 known_hosts Base64。 */
void* xssh_known_host_base64_native(void* pHandle)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Base64) : NULL;
}



/* 返回 known_hosts 注释。 */
void* xssh_known_host_comment_native(void* pHandle)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;

	return pNative != NULL ? xsshNativeCopyText(pNative->Line.Comment) : NULL;
}



/* 返回 known_hosts 行是否使用 OpenSSH hashed-host。 */
bool xssh_known_host_hashed_native(void* pHandle)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;

	return (pNative != NULL) && pNative->Line.Hashed;
}



/* 对 known_hosts 行执行明文 host pattern 匹配。 */
int xssh_known_host_match_native(
	void* pHandle,
	const char* sHost,
	size_t iHostSize,
	uint32 iPort
)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;
	xsshknownhostmatch Match;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "known_hosts line is null");
	}
	Code = xrtSshKnownHostLineMatch(
		&pNative->Line,
		(xstrview){ sHost, iHostSize },
		iPort,
		&Match
	);
	if ( Code != XSSH_OK ) {
		return (int)xsshNativePublish(Code, "known_hosts pattern match failed");
	}
	return (int)Match;
}



/* 对 hashed known_hosts 行执行常量时间匹配。 */
int xssh_known_host_hash_match_native(
	void* pHandle,
	const char* sHost,
	size_t iHostSize,
	uint32 iPort
)
{
	xssh_native_known_host* pNative = (xssh_native_known_host*)pHandle;
	bool bMatch;
	xsshcode Code;

	xrtClearError();
	if ( pNative == NULL ) {
		return (int)xsshNativePublish(XSSH_ERROR_ARGUMENT, "known_hosts line is null");
	}
	Code = xrtSshKnownHostLineHashMatch(
		&pNative->Line,
		(xstrview){ sHost, iHostSize },
		iPort,
		&bMatch
	);
	if ( Code != XSSH_OK ) {
		return (int)xsshNativePublish(Code, "known_hosts hash match failed");
	}
	return bMatch ? 1 : 0;
}



/* 直接匹配一组明文 host patterns。 */
int xssh_known_host_patterns_native(
	const char* sPatterns,
	size_t iPatternsSize,
	const char* sHost,
	size_t iHostSize,
	uint32 iPort
)
{
	xsshknownhostmatch Match;
	xsshcode Code;

	xrtClearError();
	Code = xrtSshKnownHostPatternsMatch(
		(xstrview){ sPatterns, iPatternsSize },
		(xstrview){ sHost, iHostSize },
		iPort,
		&Match
	);
	if ( Code != XSSH_OK ) {
		return (int)xsshNativePublish(Code, "known_hosts pattern match failed");
	}
	return (int)Match;
}



/* 生成 OpenSSH hashed-host token。 */
void* xssh_known_host_hash_write_native(
	const char* sHost,
	size_t iHostSize,
	uint32 iPort,
	const void* pSalt,
	size_t iSaltSize
)
{
	xstrview Host = { sHost, iHostSize };
	xbytesview Salt = { (const unsigned char*)pSalt, iSaltSize };
	xbuffer* pResult;
	size_t iOutputSize;
	xsshcode Code;

	xrtClearError();
	Code = xrtSshKnownHostHashWrite(
		Host,
		iPort,
		Salt,
		NULL,
		0u,
		&iOutputSize
	);
	if ( Code != XSSH_OK ) {
		xsshNativePublish(Code, "known_hosts hash query failed");
		return NULL;
	}
	pResult = xrtBufferCreate();
	if ( (pResult == NULL) || (iOutputSize == SIZE_MAX) ||
		 !xrtBufferResize(pResult, iOutputSize + 1u) ) {
		xrtBufferDestroy(pResult);
		return NULL;
	}
	Code = xrtSshKnownHostHashWrite(
		Host,
		iPort,
		Salt,
		(char*)pResult->Data,
		pResult->Size,
		&iOutputSize
	);
	if ( (Code != XSSH_OK) || !xrtBufferResize(pResult, iOutputSize) ) {
		xrtBufferDestroy(pResult);
		xsshNativePublish(Code, "known_hosts hash write failed");
		return NULL;
	}
	return pResult;
}



/* 常量时间匹配完整 OpenSSH hashed-host token。 */
int xssh_known_host_hash_token_match_native(
	const char* sHashedHost,
	size_t iHashedHostSize,
	const char* sHost,
	size_t iHostSize,
	uint32 iPort
)
{
	bool bMatch;
	xsshcode Code;

	xrtClearError();
	Code = xrtSshKnownHostHashMatch(
		(xstrview){ sHashedHost, iHashedHostSize },
		(xstrview){ sHost, iHostSize },
		iPort,
		&bMatch
	);
	if ( Code != XSSH_OK ) {
		return (int)xsshNativePublish(Code, "known_hosts hash token match failed");
	}
	return bMatch ? 1 : 0;
}
