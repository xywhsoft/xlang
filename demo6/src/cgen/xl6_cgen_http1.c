#include "src/cgen/xl6_cgen_http1.h"
#include "src/cgen/xl6_cgen_native_void_abi.h"

#define XL6_HTTP1_SOURCE(...) #__VA_ARGS__



/* HTTP/1 报头、正文分帧和原始封包只依赖协议层与 Buffer。 */
static const char xl6Http1RuntimeSource[] = XL6_HTTP1_SOURCE(
typedef struct xl6_http1_head {
	xhttp1head Head;
	xhttp1limits Limits;
	xhttp1errorinfo Error;
	xhttpfield* Fields;
	bytes Wire;
	xhttp1status Status;
} xl6_http1_head;



typedef struct xl6_http1_body {
	xhttp1body Body;
	xhttp1bodyplan Plan;
	xhttp1errorinfo Error;
	xhttpfield* Trailers;
	bytes TrailerStorage;
	xbytesview Data;
	size_t Consumed;
	xhttp1bodystatus Status;
} xl6_http1_body;



/* 复制一个可能不是零结尾的协议视图。 */
static void* xl6_http1_text(xstrview Text)
{
	return xl6_xrt_string_dup(Text.Data != 0 ? Text.Data : "", Text.Data != 0 ? Text.Size : 0u);
}



/* 创建解析对象并让成功结果持有自己的 Header 字节。 */
void* xl6_xrt_http1_head_create(
	void* pData,
	size_t iSize,
	long long iKind,
	long long iMaxFields,
	long long iMaxHead
)
{
	xl6_http1_head* pValue;
	xbytesview Input;
	xhttp1status Status;
	size_t iFields;
	size_t iHead;
	size_t iParsedBytes;

	if ( ((pData == 0) && (iSize != 0u)) || ((iKind != XHTTP_REQUEST) && (iKind != XHTTP_RESPONSE)) ) {
		return 0;
	}
	iFields = iMaxFields <= 0 ? 100u : (size_t)iMaxFields;
	iHead = iMaxHead <= 0 ? 65536u : (size_t)iMaxHead;
	if ( (iFields > 4096u) || (iHead > (size_t)INT_MAX) ) {
		return 0;
	}
	pValue = (xl6_http1_head*)xrtCalloc(1u, sizeof(*pValue));
	if ( pValue == 0 ) {
		return 0;
	}
	pValue->Fields = (xhttpfield*)xrtCalloc(iFields, sizeof(*pValue->Fields));
	if ( (iFields != 0u) && (pValue->Fields == 0) ) {
		xrtFree(pValue);
		return 0;
	}
	xrtHttp1LimitsInit(&pValue->Limits);
	pValue->Limits.MaxFields = iFields;
	pValue->Limits.MaxHead = iHead;
	xrtHttp1HeadInit(&pValue->Head, pValue->Fields, iFields);
	Input = (xbytesview){ (cbytes)pData, iSize };
	Status = iKind == XHTTP_REQUEST ?
		xrtHttp1RequestParse(Input, &pValue->Head, &pValue->Limits, &pValue->Error) :
		xrtHttp1ResponseParse(Input, &pValue->Head, &pValue->Limits, &pValue->Error);
	pValue->Status = Status;
	if ( Status != XHTTP1_READY ) {
		return pValue;
	}
	iParsedBytes = pValue->Head.Bytes;
	pValue->Wire = (bytes)xrtMalloc(iParsedBytes);
	if ( (iParsedBytes != 0u) && (pValue->Wire == 0) ) {
		xrtFree(pValue->Fields);
		xrtFree(pValue);
		return 0;
	}
	if ( iParsedBytes != 0u ) {
		memcpy(pValue->Wire, pData, iParsedBytes);
	}
	xrtHttp1HeadInit(&pValue->Head, pValue->Fields, iFields);
	Input = (xbytesview){ pValue->Wire, iParsedBytes };
	pValue->Status = iKind == XHTTP_REQUEST ?
		xrtHttp1RequestParse(Input, &pValue->Head, &pValue->Limits, &pValue->Error) :
		xrtHttp1ResponseParse(Input, &pValue->Head, &pValue->Limits, &pValue->Error);
	return pValue;
}



/* 释放报头及其字段描述符。 */
XL6_NATIVE_VOID_RESULT xl6_xrt_http1_head_destroy(void* pHandle)
{
	xl6_http1_head* pValue = (xl6_http1_head*)pHandle;

	if ( pValue != 0 ) {
		xrtFree(pValue->Wire);
		xrtFree(pValue->Fields);
		xrtFree(pValue);
	}
	XL6_NATIVE_VOID_RETURN();
}



/* 暴露原生句柄，供正文和 Upgrade 状态机直接复用。 */
void* xl6_xrt_http1_head_native(void* pHandle)
{
	return pHandle;
}



long long xl6_xrt_http1_head_status(void* pHandle) { return pHandle != 0 ? ((xl6_http1_head*)pHandle)->Status : XHTTP1_ERROR; }
long long xl6_xrt_http1_head_kind(void* pHandle) { return pHandle != 0 ? ((xl6_http1_head*)pHandle)->Head.Kind : 0; }
long long xl6_xrt_http1_head_version(void* pHandle) { return pHandle != 0 ? ((xl6_http1_head*)pHandle)->Head.Version : 0; }
long long xl6_xrt_http1_head_flags(void* pHandle) { return pHandle != 0 ? ((xl6_http1_head*)pHandle)->Head.Flags : 0; }
long long xl6_xrt_http1_head_code(void* pHandle) { return pHandle != 0 ? ((xl6_http1_head*)pHandle)->Head.Status : 0; }
unsigned long long xl6_xrt_http1_head_content_length(void* pHandle) { return pHandle != 0 ? (unsigned long long)((xl6_http1_head*)pHandle)->Head.ContentLength : 0u; }
long long xl6_xrt_http1_head_bytes(void* pHandle) { return pHandle != 0 ? (long long)((xl6_http1_head*)pHandle)->Head.Bytes : 0; }
long long xl6_xrt_http1_head_field_count(void* pHandle) { return pHandle != 0 ? (long long)((xl6_http1_head*)pHandle)->Head.FieldCount : 0; }
long long xl6_xrt_http1_head_error(void* pHandle) { return pHandle != 0 ? ((xl6_http1_head*)pHandle)->Error.Code : 0; }
long long xl6_xrt_http1_head_error_offset(void* pHandle) { return pHandle != 0 ? (long long)((xl6_http1_head*)pHandle)->Error.Offset : 0; }
long long xl6_xrt_http1_head_error_line(void* pHandle) { return pHandle != 0 ? (long long)((xl6_http1_head*)pHandle)->Error.Line : 0; }



void* xl6_xrt_http1_head_method(void* pHandle) { return pHandle != 0 ? xl6_http1_text(((xl6_http1_head*)pHandle)->Head.Method) : xl6_xrt_string_dup("", 0u); }
void* xl6_xrt_http1_head_target(void* pHandle) { return pHandle != 0 ? xl6_http1_text(((xl6_http1_head*)pHandle)->Head.Target) : xl6_xrt_string_dup("", 0u); }
void* xl6_xrt_http1_head_reason(void* pHandle) { return pHandle != 0 ? xl6_http1_text(((xl6_http1_head*)pHandle)->Head.Reason) : xl6_xrt_string_dup("", 0u); }



/* 按线序号复制字段名。 */
void* xl6_xrt_http1_head_field_name(void* pHandle, long long iIndex)
{
	xl6_http1_head* pValue = (xl6_http1_head*)pHandle;

	if ( (pValue == 0) || (iIndex < 0) || ((size_t)iIndex >= pValue->Head.FieldCount) ) {
		return xl6_xrt_string_dup("", 0u);
	}
	return xl6_http1_text(pValue->Head.Fields[iIndex].Name);
}



/* 按线序号复制字段值。 */
void* xl6_xrt_http1_head_field_value(void* pHandle, long long iIndex)
{
	xl6_http1_head* pValue = (xl6_http1_head*)pHandle;

	if ( (pValue == 0) || (iIndex < 0) || ((size_t)iIndex >= pValue->Head.FieldCount) ) {
		return xl6_xrt_string_dup("", 0u);
	}
	return xl6_http1_text(pValue->Head.Fields[iIndex].Value);
}



/* 查找第一个同名字段并复制其值。 */
void* xl6_xrt_http1_head_field(void* pHandle, xlir_string name)
{
	xl6_http1_head* pValue = (xl6_http1_head*)pHandle;
	const xhttpfield* pField;

	if ( pValue == 0 ) {
		return xl6_xrt_string_dup("", 0u);
	}
	if ( name.Data == 0 ) return 0;
	pField = xrtHttp1Field(&pValue->Head, name);
	return pField != 0 ? xl6_http1_text(pField->Value) : xl6_xrt_string_dup("", 0u);
}



/* 校验原始字段块，要求每行完整且不允许折行或提前空行。 */
static bool xl6_http1_raw_fields_valid(const char* sFields)
{
	const unsigned char* p = (const unsigned char*)(sFields != 0 ? sFields : "");

	while ( *p != 0u ) {
		const unsigned char* pLine = p;
		const unsigned char* pColon = 0;

		while ( (p[0] != 0u) && !((p[0] == '\r') && (p[1] == '\n')) ) {
			if ( (p[0] == '\r') || (p[0] == '\n') || (p[0] < 0x20u) || (p[0] == 0x7Fu) ) {
				return false;
			}
			if ( (p[0] == ':') && (pColon == 0) ) {
				pColon = p;
			}
			++p;
		}
		if ( (p[0] == 0u) || (p == pLine) || (pColon == 0) || (pColon == pLine) ) {
			return false;
		}
		p += 2;
	}
	return true;
}



/* 校验请求行 token 与 target 的最低线缆约束。 */
static bool xl6_http1_request_line_valid(const char* sMethod, const char* sTarget)
{
	const unsigned char* p;

	if ( (sMethod == 0) || (sMethod[0] == 0) || (sTarget == 0) || (sTarget[0] == 0) ) {
		return false;
	}
	for ( p = (const unsigned char*)sMethod; *p != 0u; ++p ) {
		if ( (*p <= 0x20u) || (*p >= 0x7Fu) || strchr("()<>@,;:\\\"/[]?={}\t", *p) != 0 ) {
			return false;
		}
	}
	for ( p = (const unsigned char*)sTarget; *p != 0u; ++p ) {
		if ( (*p <= 0x20u) || (*p == 0x7Fu) ) {
			return false;
		}
	}
	return true;
}



/* 追加一个零结尾文本视图。 */
static bool xl6_http1_buffer_text(xbuffer* pBuffer, const char* sText)
{
	const char* sValue = sText != 0 ? sText : "";

	return xrtBufferAppend(pBuffer, (xbytesview){ (cbytes)sValue, strlen(sValue) });
}



/* 一次分配生成可直接与正文组成 sendVec 的请求 Header。 */
void* xl6_xrt_http1_request_head(xlir_string method, xlir_string target, xlir_string fields)
{
	xbuffer* pOutput;
	const char* sMethod = xlir_string_cstr(method);
	const char* sTarget = xlir_string_cstr(target);
	const char* sFields = xlir_string_cstr(fields);

	if ( sMethod == 0 || sTarget == 0 || sFields == 0 ||
	     !xl6_http1_request_line_valid(sMethod, sTarget) || !xl6_http1_raw_fields_valid(sFields) ) {
		return 0;
	}
	pOutput = xrtBufferCreate();
	if ( pOutput == 0 ) {
		return 0;
	}
	if ( !xl6_http1_buffer_text(pOutput, sMethod) ||
		 !xl6_http1_buffer_text(pOutput, " ") ||
		 !xl6_http1_buffer_text(pOutput, sTarget) ||
		 !xl6_http1_buffer_text(pOutput, " HTTP/1.1\r\n") ||
		 !xl6_http1_buffer_text(pOutput, sFields) ||
		 !xl6_http1_buffer_text(pOutput, "\r\n") ) {
		xrtBufferDestroy(pOutput);
		return 0;
	}
	return pOutput;
}



/* 一次分配生成可直接与正文组成 sendVec 的响应 Header。 */
void* xl6_xrt_http1_response_head(int64_t iStatus, xlir_string reason, xlir_string fields)
{
	xbuffer* pOutput;
	char sCode[16];
	const unsigned char* p;
	const char* sReason = xlir_string_cstr(reason);
	const char* sFields = xlir_string_cstr(fields);

	if ( sReason == 0 || sFields == 0 || (iStatus < 100) || (iStatus > 999) || !xl6_http1_raw_fields_valid(sFields) ) {
		return 0;
	}
	for ( p = (const unsigned char*)(sReason != 0 ? sReason : ""); *p != 0u; ++p ) {
		if ( (*p == '\r') || (*p == '\n') || (*p == 0x7Fu) ) {
			return 0;
		}
	}
	(void)snprintf(sCode, sizeof(sCode), "%lld", (long long)iStatus);
	pOutput = xrtBufferCreate();
	if ( pOutput == 0 ) {
		return 0;
	}
	if ( !xl6_http1_buffer_text(pOutput, "HTTP/1.1 ") ||
		 !xl6_http1_buffer_text(pOutput, sCode) ||
		 (((sReason != 0) && (sReason[0] != 0)) &&
		  (!xl6_http1_buffer_text(pOutput, " ") || !xl6_http1_buffer_text(pOutput, sReason))) ||
		 !xl6_http1_buffer_text(pOutput, "\r\n") ||
		 !xl6_http1_buffer_text(pOutput, sFields) ||
		 !xl6_http1_buffer_text(pOutput, "\r\n") ) {
		xrtBufferDestroy(pOutput);
		return 0;
	}
	return pOutput;
}



/* 生成 chunk-size 行，正文可继续以 sendVec 零拷贝发送。 */
void* xl6_xrt_http1_chunk_head(uint64_t iSize, xlir_string extensions)
{
	xbuffer* pOutput;
	size_t iBytes = 0u;

	if ( extensions.Data == 0 || !xrtHttp1ChunkLineWrite(iSize, extensions, 0, 0u, &iBytes) ) {
		return 0;
	}
	pOutput = xrtBufferCreate();
	if ( (pOutput == 0) || !xrtBufferResize(pOutput, iBytes) ||
	     !xrtHttp1ChunkLineWrite(iSize, extensions, pOutput->Data, pOutput->Capacity, &pOutput->Size) ) {
		xrtBufferDestroy(pOutput);
		return 0;
	}
	return pOutput;
}



/* 生成一个完整 chunk，作为非向量发送的便利入口。 */
void* xl6_xrt_http1_chunk(void* pData, size_t iSize)
{
	xbuffer* pOutput;
	size_t iBytes = 0u;
	xbytesview Data = { (cbytes)pData, iSize };

	if ( ((pData == 0) && (iSize != 0u)) || !xrtHttp1ChunkWrite(Data, 0, 0u, &iBytes) ) {
		return 0;
	}
	pOutput = xrtBufferCreate();
	if ( (pOutput == 0) || !xrtBufferResize(pOutput, iBytes) ||
		 !xrtHttp1ChunkWrite(Data, pOutput->Data, pOutput->Capacity, &pOutput->Size) ) {
		xrtBufferDestroy(pOutput);
		return 0;
	}
	return pOutput;
}



/* 生成 last-chunk 与可选原始 trailer 字段块。 */
void* xl6_xrt_http1_chunk_end(xlir_string trailers)
{
	xbuffer* pOutput;
	const char* sTrailers = xlir_string_cstr(trailers);

	if ( sTrailers == 0 || !xl6_http1_raw_fields_valid(sTrailers) ) {
		return 0;
	}
	pOutput = xrtBufferCreate();
	if ( (pOutput == 0) || !xl6_http1_buffer_text(pOutput, "0\r\n") ||
		 !xl6_http1_buffer_text(pOutput, sTrailers) || !xl6_http1_buffer_text(pOutput, "\r\n") ) {
		xrtBufferDestroy(pOutput);
		return 0;
	}
	return pOutput;
}



/* 从已解析 Header 建立无固定接收缓冲的正文状态机。 */
void* xl6_xrt_http1_body_create(void* pHeadHandle, xlir_string requestMethod, long long iMaxTrailers, long long iMaxBody)
{
	xl6_http1_head* pHead = (xl6_http1_head*)pHeadHandle;
	xl6_http1_body* pValue;
	xhttp1bodylimits Limits;
	size_t iTrailers;
	bool bPlan;
	const char* sRequestMethod = xlir_string_cstr(requestMethod);

	if ( (pHead == 0) || (pHead->Status != XHTTP1_READY) || sRequestMethod == 0 ) {
		return 0;
	}
	iTrailers = iMaxTrailers < 0 ? 32u : (size_t)iMaxTrailers;
	if ( iTrailers > 4096u ) {
		return 0;
	}
	pValue = (xl6_http1_body*)xrtCalloc(1u, sizeof(*pValue));
	if ( pValue == 0 ) {
		return 0;
	}
	pValue->Trailers = (xhttpfield*)xrtCalloc(iTrailers, sizeof(*pValue->Trailers));
	if ( (iTrailers != 0u) && (pValue->Trailers == 0) ) {
		xrtFree(pValue);
		return 0;
	}
	if ( pHead->Head.Kind == XHTTP_REQUEST ) {
		bPlan = xrtHttp1RequestBodyPlan(&pHead->Head, &pValue->Plan);
	} else {
		xstrview Method = { sRequestMethod != 0 ? sRequestMethod : "", sRequestMethod != 0 ? strlen(sRequestMethod) : 0u };
		bPlan = xrtHttp1ResponseBodyPlan(&pHead->Head, Method, &pValue->Plan);
	}
	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxBody = iMaxBody < 0 ? UINT64_MAX : (uint64_t)iMaxBody;
	Limits.MaxTrailers = iTrailers;
	if ( !bPlan || !xrtHttp1BodyInit(&pValue->Body, &pValue->Plan, pValue->Trailers, iTrailers, &Limits) ) {
		xrtFree(pValue->Trailers);
		xrtFree(pValue);
		return 0;
	}
	return pValue;
}



/* Trailer 完成时把借用视图收口为对象自有内存。 */
static bool xl6_http1_body_own_trailers(xl6_http1_body* pValue)
{
	size_t i;
	size_t iBytes = 0u;
	bytes pWrite;

	if ( (pValue == 0) || (pValue->TrailerStorage != 0) || (pValue->Body.TrailerCount == 0u) ) {
		return true;
	}
	for ( i = 0u; i < pValue->Body.TrailerCount; ++i ) {
		if ( (SIZE_MAX - iBytes) < (pValue->Trailers[i].Name.Size + pValue->Trailers[i].Value.Size) ) {
			return false;
		}
		iBytes += pValue->Trailers[i].Name.Size + pValue->Trailers[i].Value.Size;
	}
	pValue->TrailerStorage = (bytes)xrtMalloc(iBytes);
	if ( (iBytes != 0u) && (pValue->TrailerStorage == 0) ) {
		return false;
	}
	pWrite = pValue->TrailerStorage;
	for ( i = 0u; i < pValue->Body.TrailerCount; ++i ) {
		memcpy(pWrite, pValue->Trailers[i].Name.Data, pValue->Trailers[i].Name.Size);
		pValue->Trailers[i].Name.Data = (const char*)pWrite;
		pWrite += pValue->Trailers[i].Name.Size;
		memcpy(pWrite, pValue->Trailers[i].Value.Data, pValue->Trailers[i].Value.Size);
		pValue->Trailers[i].Value.Data = (const char*)pWrite;
		pWrite += pValue->Trailers[i].Value.Size;
	}
	return true;
}



/* 推进正文状态并保留本次借用数据视图。 */
long long xl6_xrt_http1_body_read(void* pHandle, void* pData, size_t iSize, int bEnd)
{
	xl6_http1_body* pValue = (xl6_http1_body*)pHandle;

	if ( (pValue == 0) || ((pData == 0) && (iSize != 0u)) ) {
		return XHTTP1_BODY_ERROR;
	}
	pValue->Consumed = 0u;
	pValue->Data = (xbytesview){ 0 };
	memset(&pValue->Error, 0, sizeof(pValue->Error));
	pValue->Status = xrtHttp1BodyRead(
		&pValue->Body,
		(xbytesview){ (cbytes)pData, iSize },
		bEnd != 0,
		&pValue->Consumed,
		&pValue->Data,
		&pValue->Error
	);
	if ( (pValue->Status == XHTTP1_BODY_DONE) && !xl6_http1_body_own_trailers(pValue) ) {
		pValue->Status = XHTTP1_BODY_ERROR;
	}
	return pValue->Status;
}



/* 释放正文状态。 */
XL6_NATIVE_VOID_RESULT xl6_xrt_http1_body_destroy(void* pHandle)
{
	xl6_http1_body* pValue = (xl6_http1_body*)pHandle;

	if ( pValue != 0 ) {
		xrtFree(pValue->TrailerStorage);
		xrtFree(pValue->Trailers);
		xrtFree(pValue);
	}
	XL6_NATIVE_VOID_RETURN();
}



void* xl6_xrt_http1_body_native(void* pHandle) { return pHandle; }
long long xl6_xrt_http1_body_status(void* pHandle) { return pHandle != 0 ? ((xl6_http1_body*)pHandle)->Status : XHTTP1_BODY_ERROR; }
long long xl6_xrt_http1_body_mode(void* pHandle) { return pHandle != 0 ? ((xl6_http1_body*)pHandle)->Plan.Mode : XHTTP1_BODY_NONE; }
unsigned long long xl6_xrt_http1_body_length(void* pHandle) { return pHandle != 0 ? (unsigned long long)((xl6_http1_body*)pHandle)->Plan.Length : 0u; }
unsigned long long xl6_xrt_http1_body_received(void* pHandle) { return pHandle != 0 ? (unsigned long long)((xl6_http1_body*)pHandle)->Body.Received : 0u; }
unsigned long long xl6_xrt_http1_body_wire_bytes(void* pHandle) { return pHandle != 0 ? (unsigned long long)((xl6_http1_body*)pHandle)->Body.WireBytes : 0u; }
long long xl6_xrt_http1_body_consumed(void* pHandle) { return pHandle != 0 ? (long long)((xl6_http1_body*)pHandle)->Consumed : 0; }
void* xl6_xrt_http1_body_data(void* pHandle) { return pHandle != 0 ? (void*)((xl6_http1_body*)pHandle)->Data.Data : 0; }
long long xl6_xrt_http1_body_data_size(void* pHandle) { return pHandle != 0 ? (long long)((xl6_http1_body*)pHandle)->Data.Size : 0; }
int xl6_xrt_http1_body_done(void* pHandle) { return pHandle != 0 && xrtHttp1BodyDone(&((xl6_http1_body*)pHandle)->Body); }
long long xl6_xrt_http1_body_error(void* pHandle) { return pHandle != 0 ? ((xl6_http1_body*)pHandle)->Error.Code : 0; }
long long xl6_xrt_http1_body_error_offset(void* pHandle) { return pHandle != 0 ? (long long)((xl6_http1_body*)pHandle)->Error.Offset : 0; }
long long xl6_xrt_http1_body_trailer_count(void* pHandle) { return pHandle != 0 ? (long long)((xl6_http1_body*)pHandle)->Body.TrailerCount : 0; }



/* 按需复制本次正文片段。 */
void* xl6_xrt_http1_body_data_copy(void* pHandle)
{
	xl6_http1_body* pValue = (xl6_http1_body*)pHandle;

	return pValue != 0 ? xl6_xrt_buffer_from_view_bytes(pValue->Data.Data, pValue->Data.Size) : xrtBufferCreate();
}



void* xl6_xrt_http1_body_trailer_name(void* pHandle, long long iIndex)
{
	xl6_http1_body* pValue = (xl6_http1_body*)pHandle;

	return (pValue != 0) && (iIndex >= 0) && ((size_t)iIndex < pValue->Body.TrailerCount) ?
		xl6_http1_text(pValue->Trailers[iIndex].Name) : xl6_xrt_string_dup("", 0u);
}



void* xl6_xrt_http1_body_trailer_value(void* pHandle, long long iIndex)
{
	xl6_http1_body* pValue = (xl6_http1_body*)pHandle;

	return (pValue != 0) && (iIndex >= 0) && ((size_t)iIndex < pValue->Body.TrailerCount) ?
		xl6_http1_text(pValue->Trailers[iIndex].Value) : xl6_xrt_string_dup("", 0u);
}
);



/* 输出 HTTP/1 报头、正文和快速封包桥。 */
void xl6CgenEmitHttp1Runtime(XL6_Output* out)
{
	if ( out != NULL ) {
		xl6CgenEmitNativeVoidAbiBegin(out);
		xl6OutputPuts(
			"static char* xlir_http1_string_dup(const char* value, size_t size)\n"
			"{ const char* source = value != NULL ? value : \"\"; return size == 0 ? xrtStrDup(source) : xrtStrDupN(source, size); }\n"
			"#define xl6_xrt_string_dup xlir_http1_string_dup\n",
			out);
		xl6OutputPuts(xl6Http1RuntimeSource, out);
		xl6CgenEmitNativeVoidAbiEnd(out);
		xl6OutputPuts("\n#undef xl6_xrt_string_dup", out);
		xl6OutputPuts("\n\n", out);
	}
}



#undef XL6_HTTP1_SOURCE
