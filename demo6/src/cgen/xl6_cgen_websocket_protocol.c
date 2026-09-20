/* WebSocket 帧头与 Close 载荷保持无消息聚合的协议快速路径。 */
static const char xl6WebSocketFrameSource[] = XL6_WEBSOCKET_SOURCE(
typedef struct xl6_ws_frame {
	xwsframe Frame;
	xwsframeconfig Config;
	xwsframeerrorinfo Error;
	xwsframestatus Status;
} xl6_ws_frame;



typedef struct xl6_ws_close {
	uint16 Code;
	char* Reason;
	size_t ReasonSize;
} xl6_ws_close;



/* 从线路字节严格解析一个帧头，READY 不要求负载已经到达。 */
void* xl6_xrt_ws_frame_parse(
	void* pData,
	size_t iSize,
	int64_t iMaskPolicy,
	int64_t iMaxPayload,
	int64_t iAllowedRsv
)
{
	xl6_ws_frame* pValue;

	if ( ((pData == 0) && (iSize != 0u)) ||
		 (iMaskPolicy < XWS_MASK_ANY) ||
		 (iMaskPolicy > XWS_MASK_FORBIDDEN) ||
		 (iAllowedRsv < 0) ||
		 (((uint64_t)iAllowedRsv & ~(uint64_t)(XWS_FRAME_RSV1 | XWS_FRAME_RSV2 | XWS_FRAME_RSV3)) != 0u) ) {
		return 0;
	}
	pValue = (xl6_ws_frame*)xrtCalloc(1u, sizeof(*pValue));
	if ( pValue == 0 ) {
		return 0;
	}
	xrtWsFrameConfigInit(&pValue->Config);
	pValue->Config.Mask = (xwsmaskpolicy)iMaskPolicy;
	pValue->Config.AllowedRsv = (uint16)iAllowedRsv;
	if ( iMaxPayload >= 0 ) {
		pValue->Config.MaxPayload = (uint64_t)iMaxPayload;
	}
	pValue->Status = xrtWsFrameParse(
		(xbytesview){ (cbytes)pData, iSize },
		&pValue->Frame,
		&pValue->Config,
		&pValue->Error
	);
	return pValue;
}



/* 构造一个可直接写出帧头的描述符。 */
void* xl6_xrt_ws_frame_create(
	int64_t iOpcode,
	uint64_t iPayloadSize,
	bool bFinal,
	bool bMasked,
	uint32_t iMask,
	int64_t iRsv
)
{
	xl6_ws_frame* pValue;

	if ( (iOpcode < 0) || (iOpcode > 15) ||
		 (iPayloadSize > XWS_FRAME_PAYLOAD_MAX) ||
		 (iRsv < 0) ||
		 (((uint64_t)iRsv & ~(uint64_t)(XWS_FRAME_RSV1 | XWS_FRAME_RSV2 | XWS_FRAME_RSV3)) != 0u) ) {
		return 0;
	}
	pValue = (xl6_ws_frame*)xrtCalloc(1u, sizeof(*pValue));
	if ( pValue == 0 ) {
		return 0;
	}
	xrtWsFrameConfigInit(&pValue->Config);
	xrtWsFrameInit(&pValue->Frame);
	pValue->Frame.Opcode = (uint8)iOpcode;
	pValue->Frame.PayloadSize = iPayloadSize;
	pValue->Frame.Flags = (uint32)iRsv;
	if ( bFinal ) {
		pValue->Frame.Flags |= XWS_FRAME_FIN;
	}
	if ( bMasked ) {
		pValue->Frame.Flags |= XWS_FRAME_MASKED;
		pValue->Frame.Mask[0] = (uint8)(iMask >> 24u);
		pValue->Frame.Mask[1] = (uint8)(iMask >> 16u);
		pValue->Frame.Mask[2] = (uint8)(iMask >> 8u);
		pValue->Frame.Mask[3] = (uint8)iMask;
	}
	pValue->Config.AllowedRsv = (uint16)iRsv;
	pValue->Status = XWS_FRAME_READY;
	return pValue;
}



/* 释放帧描述符。 */
void* xl6_xrt_ws_frame_wrap(void* pHandle) { return pHandle; }



XL6_NATIVE_VOID_RESULT xl6_xrt_ws_frame_destroy(void* pHandle)
{
	xrtFree(pHandle);
	XL6_NATIVE_VOID_RETURN();
}



void* xl6_xrt_ws_frame_native(void* pHandle) { return pHandle; }
long long xl6_xrt_ws_frame_status(void* pHandle) { return pHandle != 0 ? ((xl6_ws_frame*)pHandle)->Status : XWS_FRAME_ERROR; }
long long xl6_xrt_ws_frame_opcode(void* pHandle) { return pHandle != 0 ? ((xl6_ws_frame*)pHandle)->Frame.Opcode : 0; }
long long xl6_xrt_ws_frame_flags(void* pHandle) { return pHandle != 0 ? ((xl6_ws_frame*)pHandle)->Frame.Flags : 0; }
unsigned long long xl6_xrt_ws_frame_payload(void* pHandle) { return pHandle != 0 ? (unsigned long long)((xl6_ws_frame*)pHandle)->Frame.PayloadSize : 0u; }
long long xl6_xrt_ws_frame_head(void* pHandle) { return pHandle != 0 ? (long long)((xl6_ws_frame*)pHandle)->Frame.HeadSize : 0; }
long long xl6_xrt_ws_frame_error(void* pHandle) { return pHandle != 0 ? ((xl6_ws_frame*)pHandle)->Error.Code : 0; }
long long xl6_xrt_ws_frame_error_offset(void* pHandle) { return pHandle != 0 ? (long long)((xl6_ws_frame*)pHandle)->Error.Offset : 0; }



/* 按网络字节序返回四字节掩码。 */
uint32_t xl6_xrt_ws_frame_mask(void* pHandle)
{
	xl6_ws_frame* pValue = (xl6_ws_frame*)pHandle;

	if ( pValue == 0 ) {
		return 0u;
	}
	return ((uint32_t)pValue->Frame.Mask[0] << 24u) |
		((uint32_t)pValue->Frame.Mask[1] << 16u) |
		((uint32_t)pValue->Frame.Mask[2] << 8u) |
		(uint32_t)pValue->Frame.Mask[3];
}



/* 一次分配写出规范帧头。 */
void* xl6_xrt_ws_frame_write(void* pHandle)
{
	xl6_ws_frame* pValue = (xl6_ws_frame*)pHandle;
	xbuffer* pOutput;
	size_t iSize = 0u;

	if ( (pValue == 0) || (pValue->Status != XWS_FRAME_READY) ||
		 !xrtWsFrameWrite(&pValue->Frame, &pValue->Config, 0, 0u, &iSize) ) {
		return 0;
	}
	pOutput = xrtBufferCreate();
	if ( (pOutput == 0) || !xrtBufferResize(pOutput, iSize) ||
		 !xrtWsFrameWrite(&pValue->Frame, &pValue->Config, pOutput->Data, pOutput->Capacity, &pOutput->Size) ) {
		xrtBufferDestroy(pOutput);
		return 0;
	}
	return pOutput;
}



/* 原地应用或移除线路掩码。 */
bool xl6_xrt_ws_mask(void* pBuffer, uint32_t iMask, uint64_t iOffset)
{
	xbuffer* pValue = (xbuffer*)pBuffer;
	uint8 Mask[XWS_MASK_SIZE];

	if ( pValue == 0 ) {
		return false;
	}
	Mask[0] = (uint8)(iMask >> 24u);
	Mask[1] = (uint8)(iMask >> 16u);
	Mask[2] = (uint8)(iMask >> 8u);
	Mask[3] = (uint8)iMask;
	return xrtWsMask(pValue->Data, pValue->Size, Mask, iOffset);
}



/* 创建并立即验证一个待写出的 Close 载荷。 */
void* xl6_xrt_ws_close_create(int64_t iCode, const char* sReason)
{
	xl6_ws_close* pValue;
	xstrview Reason;
	size_t iSize = 0u;

	if ( (iCode < 0) || (iCode > UINT16_MAX) ) {
		return 0;
	}
	Reason = (xstrview){ sReason != 0 ? sReason : "", sReason != 0 ? strlen(sReason) : 0u };
	if ( !xrtWsCloseWrite((uint16)iCode, Reason, 0, 0u, &iSize) ) {
		return 0;
	}
	pValue = (xl6_ws_close*)xrtCalloc(1u, sizeof(*pValue));
	if ( pValue == 0 ) {
		return 0;
	}
	pValue->Reason = xl6_xrt_string_dup(Reason.Data, Reason.Size);
	if ( pValue->Reason == 0 ) {
		xrtFree(pValue);
		return 0;
	}
	pValue->Code = (uint16)iCode;
	pValue->ReasonSize = Reason.Size;
	return pValue;
}



/* 解析完整 Close 载荷并复制原因文本。 */
void* xl6_xrt_ws_close_parse(void* pData, size_t iSize)
{
	xl6_ws_close* pValue;
	xwsclose Close;

	if ( ((pData == 0) && (iSize != 0u)) ||
		 !xrtWsCloseParse((xbytesview){ (cbytes)pData, iSize }, &Close) ) {
		return 0;
	}
	pValue = (xl6_ws_close*)xrtCalloc(1u, sizeof(*pValue));
	if ( pValue == 0 ) {
		return 0;
	}
	pValue->Reason = xl6_xrt_string_dup(Close.Reason.Data != 0 ? Close.Reason.Data : "", Close.Reason.Size);
	if ( pValue->Reason == 0 ) {
		xrtFree(pValue);
		return 0;
	}
	pValue->Code = Close.Code;
	pValue->ReasonSize = Close.Reason.Size;
	return pValue;
}



/* 释放 Close 快照。 */
void* xl6_xrt_ws_close_wrap(void* pHandle) { return pHandle; }



XL6_NATIVE_VOID_RESULT xl6_xrt_ws_close_destroy(void* pHandle)
{
	xl6_ws_close* pValue = (xl6_ws_close*)pHandle;

	if ( pValue != 0 ) {
		xrtFree(pValue->Reason);
		xrtFree(pValue);
	}
	XL6_NATIVE_VOID_RETURN();
}



long long xl6_xrt_ws_close_code(void* pHandle) { return pHandle != 0 ? ((xl6_ws_close*)pHandle)->Code : 0; }
void* xl6_xrt_ws_close_reason(void* pHandle) { xl6_ws_close* pValue = (xl6_ws_close*)pHandle; return pValue != 0 ? xl6_xrt_string_dup(pValue->Reason, pValue->ReasonSize) : xl6_xrt_string_dup("", 0u); }



/* 写出完整 Close 载荷。 */
void* xl6_xrt_ws_close_write(void* pHandle)
{
	xl6_ws_close* pValue = (xl6_ws_close*)pHandle;
	xbuffer* pOutput;
	size_t iSize = 0u;
	xstrview Reason;

	if ( pValue == 0 ) {
		return 0;
	}
	Reason = (xstrview){ pValue->Reason, pValue->ReasonSize };
	if ( !xrtWsCloseWrite(pValue->Code, Reason, 0, 0u, &iSize) ) {
		return 0;
	}
	pOutput = xrtBufferCreate();
	if ( (pOutput == 0) || !xrtBufferResize(pOutput, iSize) ||
		 !xrtWsCloseWrite(pValue->Code, Reason, pOutput->Data, pOutput->Capacity, &pOutput->Size) ) {
		xrtBufferDestroy(pOutput);
		return 0;
	}
	return pOutput;
}
);



/* RFC 6455 握手工具保持无对象、无隐藏网络行为。 */
static const char xl6WebSocketHandshakeSource[] = XL6_WEBSOCKET_SOURCE(
/* 生成一次客户端 Sec-WebSocket-Key。 */
void* xl6_xrt_ws_key_generate(void)
{
	char sKey[XWS_KEY_CAPACITY];

	return xrtWsKeyGenerate(sKey, sizeof(sKey)) ? xl6_xrt_string_dup(sKey, XWS_KEY_SIZE) : 0;
}



/* 计算 Sec-WebSocket-Accept。 */
void* xl6_xrt_ws_accept(const char* sKey)
{
	char sAccept[XWS_ACCEPT_CAPACITY];
	xstrview Key = { sKey != 0 ? sKey : "", sKey != 0 ? strlen(sKey) : 0u };

	return xrtWsAccept(Key, sAccept, sizeof(sAccept)) ? xl6_xrt_string_dup(sAccept, XWS_ACCEPT_SIZE) : 0;
}



bool xl6_xrt_ws_key_valid(const char* sKey) { xstrview Key = { sKey != 0 ? sKey : "", sKey != 0 ? strlen(sKey) : 0u }; return xrtWsKeyValid(Key); }
bool xl6_xrt_ws_accept_valid(const char* sKey, const char* sAccept) { xstrview Key = { sKey != 0 ? sKey : "", sKey != 0 ? strlen(sKey) : 0u }; xstrview Accept = { sAccept != 0 ? sAccept : "", sAccept != 0 ? strlen(sAccept) : 0u }; return xrtWsAcceptValid(Key, Accept); }
bool xl6_xrt_ws_protocols_valid(const char* sProtocols) { xstrview Value = { sProtocols != 0 ? sProtocols : "", sProtocols != 0 ? strlen(sProtocols) : 0u }; return xrtWsProtocolsValid(Value); }
bool xl6_xrt_ws_protocols_has(const char* sProtocols, const char* sProtocol) { xstrview Values = { sProtocols != 0 ? sProtocols : "", sProtocols != 0 ? strlen(sProtocols) : 0u }; xstrview Value = { sProtocol != 0 ? sProtocol : "", sProtocol != 0 ? strlen(sProtocol) : 0u }; return xrtWsProtocolsHas(Values, Value); }



/* 按客户端偏好返回首个双方支持的子协议。 */
void* xl6_xrt_ws_protocol_select(const char* sClient, const char* sServer)
{
	xstrview Client = { sClient != 0 ? sClient : "", sClient != 0 ? strlen(sClient) : 0u };
	xstrview Server = { sServer != 0 ? sServer : "", sServer != 0 ? strlen(sServer) : 0u };
	xstrview Selected;

	return xrtWsProtocolSelect(Client, Server, &Selected) ?
		xl6_xrt_string_dup(Selected.Data != 0 ? Selected.Data : "", Selected.Size) : 0;
}
);



/* HTTP/1.1 Upgrade 快照只保存建立 WebSocket Stream 所需的协商事实。 */
static const char xl6WebSocketUpgradeSource[] = XL6_WEBSOCKET_SOURCE(
typedef struct xl6_ws_upgrade {
	xwsupgrade Value;
	xwsupgradeclientconfig Client;
	char* Protocols;
	char* Selected;
	char* Fields;
	char Key[XWS_KEY_CAPACITY];
	size_t FieldSize;
	int Role;
	bool Ready;
} xl6_ws_upgrade;



/* 复制零结尾配置文本。 */
static char* xl6_ws_text_dup(const char* sText)
{
	const char* sValue = sText != 0 ? sText : "";

	return xl6_xrt_string_dup(sValue, strlen(sValue));
}



/* 把 XRT 字段数组直接展平为可交给 Http1 快速封包器的字段块。 */
static char* xl6_ws_fields(const xhttpfield* pFields, size_t iCount, size_t* pSize)
{
	char* sOutput;
	char* pWrite;
	size_t i;
	size_t iSize = 0u;

	if ( pSize == 0 ) {
		return 0;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( (SIZE_MAX - iSize) < (pFields[i].Name.Size + pFields[i].Value.Size + 4u) ) {
			return 0;
		}
		iSize += pFields[i].Name.Size + pFields[i].Value.Size + 4u;
	}
	sOutput = (char*)xrtMalloc(iSize + 1u);
	if ( sOutput == 0 ) {
		return 0;
	}
	pWrite = sOutput;
	for ( i = 0u; i < iCount; ++i ) {
		memcpy(pWrite, pFields[i].Name.Data, pFields[i].Name.Size);
		pWrite += pFields[i].Name.Size;
		memcpy(pWrite, ": ", 2u);
		pWrite += 2u;
		memcpy(pWrite, pFields[i].Value.Data, pFields[i].Value.Size);
		pWrite += pFields[i].Value.Size;
		memcpy(pWrite, "\r\n", 2u);
		pWrite += 2u;
	}
	sOutput[iSize] = 0;
	*pSize = iSize;
	return sOutput;
}



/* 服务端默认接受一份最小合规 permessage-deflate offer。 */
static bool xl6_ws_deflate_accept(const xwsdeflate* pOffer, xwsdeflate* pResponse, void* pData)
{
	(void)pData;
	return xrtWsDeflateAccept(pOffer, pResponse);
}



/* 深复制协商结果中借用的子协议视图。 */
static bool xl6_ws_upgrade_own_protocol(xl6_ws_upgrade* pValue)
{
	xrtFree(pValue->Selected);
	pValue->Selected = xl6_xrt_string_dup(
		pValue->Value.Protocol.Data != 0 ? pValue->Value.Protocol.Data : "",
		pValue->Value.Protocol.Size
	);
	if ( pValue->Selected == 0 ) {
		return false;
	}
	pValue->Value.Protocol.Data = pValue->Selected;
	return true;
}



/* 创建客户端握手快照并生成原始请求字段。 */
void* xl6_xrt_ws_upgrade_client(
	const char* sHost,
	const char* sProtocols,
	bool bEnableDeflate,
	bool bRequireDeflate
)
{
	xl6_ws_upgrade* pValue;
	xhttpfield Fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];
	char sExtensions[XWS_DEFLATE_MAX_SIZE + 1u];
	size_t iExtensionSize = 0u;
	size_t iCount = 0u;
	xstrview Host;
	xstrview Extensions = { 0 };

	if ( (sHost == 0) || (sHost[0] == 0) ) {
		return 0;
	}
	pValue = (xl6_ws_upgrade*)xrtCalloc(1u, sizeof(*pValue));
	if ( pValue == 0 ) {
		return 0;
	}
	pValue->Protocols = xl6_ws_text_dup(sProtocols);
	if ( pValue->Protocols == 0 ) {
		xrtFree(pValue);
		return 0;
	}
	xrtWsUpgradeClientConfigInit(&pValue->Client);
	pValue->Client.Protocols = (xstrview){ pValue->Protocols, strlen(pValue->Protocols) };
	if ( bEnableDeflate ) {
		xrtWsDeflateInit(&pValue->Client.Deflate);
		pValue->Client.EnableDeflate = true;
		pValue->Client.RequireDeflate = bRequireDeflate != 0;
		if ( !xrtWsDeflateOfferWrite(&pValue->Client.Deflate, sExtensions, sizeof(sExtensions), &iExtensionSize) ) {
			xrtFree(pValue->Protocols);
			xrtFree(pValue);
			return 0;
		}
		Extensions = (xstrview){ sExtensions, iExtensionSize };
	}
	if ( !xrtWsKeyGenerate(pValue->Key, sizeof(pValue->Key)) ) {
		xrtFree(pValue->Protocols);
		xrtFree(pValue);
		return 0;
	}
	Host = (xstrview){ sHost, strlen(sHost) };
	if ( !xrtWsUpgradeRequestFields(
		Host,
		(xstrview){ pValue->Key, XWS_KEY_SIZE },
		pValue->Client.Protocols,
		Extensions,
		Fields,
		XWS_UPGRADE_REQUEST_FIELDS_MAX,
		&iCount
	) ) {
		xrtFree(pValue->Protocols);
		xrtFree(pValue);
		return 0;
	}
	pValue->Fields = xl6_ws_fields(Fields, iCount, &pValue->FieldSize);
	if ( pValue->Fields == 0 ) {
		xrtFree(pValue->Protocols);
		xrtFree(pValue);
		return 0;
	}
	pValue->Role = XWS_ROLE_CLIENT;
	return pValue;
}



/* 校验服务端请求并生成 101 响应字段。 */
void* xl6_xrt_ws_upgrade_server(
	void* pHeadHandle,
	const char* sProtocols,
	bool bEnableDeflate,
	bool bRequireDeflate
)
{
	xl6_http1_head* pHead = (xl6_http1_head*)pHeadHandle;
	xl6_ws_upgrade* pValue;
	xwsupgradeserverconfig Config;
	xhttpfield Fields[XWS_UPGRADE_RESPONSE_FIELDS_MAX];
	size_t iCount = 0u;

	if ( (pHead == 0) || (pHead->Status != XHTTP1_READY) ) {
		return 0;
	}
	pValue = (xl6_ws_upgrade*)xrtCalloc(1u, sizeof(*pValue));
	if ( pValue == 0 ) {
		return 0;
	}
	pValue->Protocols = xl6_ws_text_dup(sProtocols);
	if ( pValue->Protocols == 0 ) {
		xrtFree(pValue);
		return 0;
	}
	xrtWsUpgradeServerConfigInit(&Config);
	Config.Protocols = (xstrview){ pValue->Protocols, strlen(pValue->Protocols) };
	Config.EnableDeflate = bEnableDeflate != 0;
	Config.RequireDeflate = bRequireDeflate != 0;
	Config.AcceptDeflate = bEnableDeflate ? xl6_ws_deflate_accept : 0;
	if ( !xrtWsUpgradeRequestCheck(&pHead->Head, &Config, &pValue->Value) ||
		 !xl6_ws_upgrade_own_protocol(pValue) ||
		 !xrtWsUpgradeResponseFields(
			(xstrview){ pValue->Value.Accept, XWS_ACCEPT_SIZE },
			pValue->Value.Protocol,
			(xstrview){ pValue->Value.Extensions, pValue->Value.ExtensionSize },
			Fields,
			XWS_UPGRADE_RESPONSE_FIELDS_MAX,
			&iCount
		) ) {
		xrtFree(pValue->Selected);
		xrtFree(pValue->Protocols);
		xrtFree(pValue);
		return 0;
	}
	pValue->Fields = xl6_ws_fields(Fields, iCount, &pValue->FieldSize);
	if ( pValue->Fields == 0 ) {
		xrtFree(pValue->Selected);
		xrtFree(pValue->Protocols);
		xrtFree(pValue);
		return 0;
	}
	pValue->Role = XWS_ROLE_SERVER;
	pValue->Ready = true;
	return pValue;
}



/* 把 101 响应绑定到本次客户端 key、子协议和压缩 offer。 */
int xl6_xrt_ws_upgrade_check(void* pHandle, void* pHeadHandle)
{
	xl6_ws_upgrade* pValue = (xl6_ws_upgrade*)pHandle;
	xl6_http1_head* pHead = (xl6_http1_head*)pHeadHandle;
	xwsupgrade Upgrade;

	if ( (pValue == 0) || (pValue->Role != XWS_ROLE_CLIENT) ||
		 (pHead == 0) || (pHead->Status != XHTTP1_READY) ||
		 !xrtWsUpgradeResponseCheck(
			&pHead->Head,
			(xstrview){ pValue->Key, XWS_KEY_SIZE },
			&pValue->Client,
			&Upgrade
		) ) {
		return false;
	}
	pValue->Value = Upgrade;
	if ( !xl6_ws_upgrade_own_protocol(pValue) ) {
		return false;
	}
	pValue->Ready = true;
	return true;
}



/* 释放 Upgrade 快照。 */
void* xl6_xrt_ws_upgrade_wrap(void* pHandle) { return pHandle; }



XL6_NATIVE_VOID_RESULT xl6_xrt_ws_upgrade_destroy(void* pHandle)
{
	xl6_ws_upgrade* pValue = (xl6_ws_upgrade*)pHandle;

	if ( pValue != 0 ) {
		xrtFree(pValue->Fields);
		xrtFree(pValue->Selected);
		xrtFree(pValue->Protocols);
		xrtFree(pValue);
	}
	XL6_NATIVE_VOID_RETURN();
}



void* xl6_xrt_ws_upgrade_native(void* pHandle) { xl6_ws_upgrade* pValue = (xl6_ws_upgrade*)pHandle; return (pValue != 0) && pValue->Ready ? &pValue->Value : 0; }
long long xl6_xrt_ws_upgrade_role(void* pHandle) { return pHandle != 0 ? ((xl6_ws_upgrade*)pHandle)->Role : -1; }
int xl6_xrt_ws_upgrade_ready(void* pHandle) { return pHandle != 0 && ((xl6_ws_upgrade*)pHandle)->Ready; }
int xl6_xrt_ws_upgrade_deflate(void* pHandle) { return pHandle != 0 && ((xl6_ws_upgrade*)pHandle)->Ready && ((xl6_ws_upgrade*)pHandle)->Value.DeflateEnabled; }
void* xl6_xrt_ws_upgrade_key(void* pHandle) { xl6_ws_upgrade* pValue = (xl6_ws_upgrade*)pHandle; return pValue != 0 ? xl6_xrt_string_dup(pValue->Key, strlen(pValue->Key)) : xl6_xrt_string_dup("", 0u); }
void* xl6_xrt_ws_upgrade_protocol(void* pHandle) { xl6_ws_upgrade* pValue = (xl6_ws_upgrade*)pHandle; return (pValue != 0) && pValue->Ready ? xl6_xrt_string_dup(pValue->Value.Protocol.Data != 0 ? pValue->Value.Protocol.Data : "", pValue->Value.Protocol.Size) : xl6_xrt_string_dup("", 0u); }
void* xl6_xrt_ws_upgrade_extensions(void* pHandle) { xl6_ws_upgrade* pValue = (xl6_ws_upgrade*)pHandle; return (pValue != 0) && pValue->Ready ? xl6_xrt_string_dup(pValue->Value.Extensions, pValue->Value.ExtensionSize) : xl6_xrt_string_dup("", 0u); }
void* xl6_xrt_ws_upgrade_fields(void* pHandle) { xl6_ws_upgrade* pValue = (xl6_ws_upgrade*)pHandle; return pValue != 0 ? xl6_xrt_string_dup(pValue->Fields != 0 ? pValue->Fields : "", pValue->FieldSize) : xl6_xrt_string_dup("", 0u); }
);



/* Stream 配置单独输出，允许只构造配置而不带入网络引擎。 */
static const char xl6WebSocketStreamConfigSource[] = XL6_WEBSOCKET_SOURCE(
typedef struct xl6_ws_stream_config {
	size_t MessageLimit;
	uint64 FrameLimit;
	size_t SendLimit;
	size_t ControlReserve;
	uint64 CloseTimeout;
	bool AutoPong;
} xl6_ws_stream_config;



/* 创建 XRT 默认限制的配置快照。 */
void* xl6_xrt_ws_stream_config_create(void)
{
	xl6_ws_stream_config* pConfig = (xl6_ws_stream_config*)xrtCalloc(1u, sizeof(*pConfig));

	if ( pConfig != 0 ) {
		pConfig->MessageLimit = (size_t)1048576u;
		pConfig->FrameLimit = UINT64_C(1048576);
		pConfig->SendLimit = (size_t)1048576u;
		pConfig->ControlReserve = (size_t)512u;
		pConfig->CloseTimeout = UINT64_C(5000000);
		pConfig->AutoPong = true;
	}
	return pConfig;
}



/* 释放配置快照。 */
XL6_NATIVE_VOID_RESULT xl6_xrt_ws_stream_config_destroy(void* pHandle)
{
	xrtFree(pHandle);
	XL6_NATIVE_VOID_RETURN();
}



void* xl6_xrt_ws_stream_config_native(void* pHandle) { return pHandle; }



/* 同时设置消息、帧和永久发送预算。 */
int xl6_xrt_ws_stream_config_limits(
	void* pHandle,
	long long iMessage,
	long long iFrame,
	long long iSend
)
{
	xl6_ws_stream_config* pConfig = (xl6_ws_stream_config*)pHandle;

	if ( (pConfig == 0) || (iMessage < 0) || (iFrame < 0) || (iSend < 0) ||
		 ((uint64_t)iMessage > (uint64_t)SIZE_MAX) ||
		 ((uint64_t)iSend > (uint64_t)SIZE_MAX) ) {
		return false;
	}
	pConfig->MessageLimit = (size_t)iMessage;
	pConfig->FrameLimit = (uint64)iFrame;
	pConfig->SendLimit = (size_t)iSend;
	return true;
}



/* 设置控制帧预留和关闭握手超时。 */
int xl6_xrt_ws_stream_config_control(
	void* pHandle,
	long long iReserve,
	long long iTimeoutMs
)
{
	xl6_ws_stream_config* pConfig = (xl6_ws_stream_config*)pHandle;

	if ( (pConfig == 0) || (iReserve < 0) || (iTimeoutMs < 0) ||
		 ((uint64_t)iReserve > (uint64_t)SIZE_MAX) ||
		 ((uint64_t)iTimeoutMs > (UINT64_MAX / UINT64_C(1000))) ) {
		return false;
	}
	pConfig->ControlReserve = (size_t)iReserve;
	pConfig->CloseTimeout = ((uint64)iTimeoutMs * UINT64_C(1000));
	return true;
}



/* 切换自动 Pong。 */
void xl6_xrt_ws_stream_config_auto_pong(void* pHandle, int bEnabled)
{
	xl6_ws_stream_config* pConfig = (xl6_ws_stream_config*)pHandle;

	if ( pConfig != 0 ) {
		pConfig->AutoPong = bEnabled != 0;
	}
}
);
