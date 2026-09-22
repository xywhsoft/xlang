#include <string.h>

#include <xrt/ssh_kexinit.h>



#if defined(XSSH_FEATURE_KEXINIT)

/* 返回显式列表或模块默认列表。 */
static xstrview xsshKexList(
	xstrview Value,
	xstrview DefaultValue
)
{
	return (Value.Data == NULL) && (Value.Size == 0u) ?
		DefaultValue : Value;
}



/* 验证单个 name-list 并累加长度前缀。 */
static xsshcode xsshKexListSize(xstrview Value, size_t* pTotalSize)
{
	if ( (pTotalSize == NULL) || (Value.Size > UINT32_MAX) ||
		!xrtSshNameListValid(Value) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotalSize > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotalSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotalSize += 4u + Value.Size;
	return XSSH_OK;
}



/* 读取并严格验证一个 SSH name-list。 */
static xsshcode xsshKexReadList(
	xsshreader* pReader,
	xstrview* pList
)
{
	xbytesview Value;
	xsshcode Code;

	Code = xrtSshReadString(pReader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshNameListValid((xstrview){
		(const char*)Value.Data,
		Value.Size
	}) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pList->Data = (const char*)Value.Data;
	pList->Size = Value.Size;
	return XSSH_OK;
}



/* 返回 name-list 的第一项。 */
static xsshcode xsshKexFirst(
	xstrview List,
	xstrview* pFirst
)
{
	size_t i;

	if ( (pFirst == NULL) || !xrtSshNameListValid(List) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( List.Size == 0u ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	for ( i = 0u; i < List.Size; ++i ) {
		if ( List.Data[i] == ',' ) {
			break;
		}
	}
	pFirst->Data = List.Data;
	pFirst->Size = i;
	return XSSH_OK;
}



/* 比较两个文本视图。 */
static bool xsshKexEqual(xstrview Left, xstrview Right)
{
	if ( ((Left.Data == NULL) && (Left.Size != 0u)) ||
		((Right.Data == NULL) && (Right.Size != 0u)) ) {
		return false;
	}
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		(memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 判断 endpoint 角色是否属于公开枚举。 */
static bool xsshKexRoleValid(xsshrole Role)
{
	return (Role == XSSH_ROLE_CLIENT) || (Role == XSSH_ROLE_SERVER);
}



/* 判断名称是否只是 KEX 协商扩展标记，而不是真实算法。 */
static bool xsshKexIndicator(xstrview Name)
{
	return xsshKexEqual(Name, XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT)) ||
		xsshKexEqual(Name, XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER)) ||
		xsshKexEqual(Name, XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT)) ||
		xsshKexEqual(Name, XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER)) ||
		xsshKexEqual(
			Name,
			XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD)
		) || xsshKexEqual(
			Name,
			XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD)
		);
}



/* 返回 KEX 清单中第一项真实算法，跳过 RFC 8308/strict-kex 标记。 */
static xsshcode xsshKexFirstAlgorithm(
	xstrview List,
	xstrview* pFirst
)
{
	size_t iStart = 0u;
	size_t i;

	if ( (pFirst == NULL) || !xrtSshNameListValid(List) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( List.Size == 0u ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	for ( i = 0u; i <= List.Size; ++i ) {
		if ( (i == List.Size) || (List.Data[i] == ',') ) {
			xstrview Name = { List.Data + iStart, i - iStart };

			if ( !xsshKexIndicator(Name) ) {
				*pFirst = Name;
				return XSSH_OK;
			}
			iStart = i + 1u;
		}
	}
	return XSSH_ERROR_UNSUPPORTED;
}



/* 判断清单是否包含任一首次 KEX 专用标记。 */
static bool xsshKexHasIndicator(xstrview List)
{
	return xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD)
	);
}



/* 返回角色和 KEX 阶段对应的默认算法清单。 */
static xstrview xsshKexDefaultAlgorithms(
	xsshrole Role,
	bool bInitial
)
{
	if ( !bInitial ) {
		return XRT_STR_LITERAL(XSSH_KEX_DEFAULT);
	}
	return Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT) :
		XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT);
}



/* 校验本端清单没有使用对端标记或在重协商中保留首次标记。 */
static bool xsshKexConfigIndicatorsValid(
	const xsshkexinitconfig* pConfig,
	xstrview Algorithms
)
{
	xstrview WrongExt = pConfig->Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER) :
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT);
	xstrview WrongStrict = pConfig->Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT);
	xstrview WrongStrictPreStandard = pConfig->Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD);

	if ( xrtSshNameListContains(Algorithms, WrongExt) ||
		xrtSshNameListContains(Algorithms, WrongStrict) ||
		xrtSshNameListContains(Algorithms, WrongStrictPreStandard) ) {
		return false;
	}
	return pConfig->Initial || !xsshKexHasIndicator(Algorithms);
}



/* 初始化当前已经闭环实现的默认算法和现代扩展标记。 */
bool xrtSshKexInitConfigInit(
	xsshkexinitconfig* pConfig,
	xsshrole Role,
	bool bInitial
)
{
	xsshkexinitconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!xsshKexRoleValid(Role) ) {
		return false;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Role = Role;
	Config.Initial = bInitial;
	Config.KexAlgorithms = xsshKexDefaultAlgorithms(Role, bInitial);
	Config.ServerHostKeyAlgorithms = XRT_STR_LITERAL(XSSH_HOSTKEY_DEFAULT);
	Config.EncryptionClientToServer = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Config.EncryptionServerToClient = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Config.MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Config.MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Config.CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Config.CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Config.LanguagesClientToServer = XRT_STR_LITERAL("");
	Config.LanguagesServerToClient = XRT_STR_LITERAL("");
	*pConfig = Config;
	return true;
}



/* 预先验证完整 payload，随后一次性提交 writer 状态。 */
xsshcode xrtSshKexInitWrite(
	xsshwriter* pWriter,
	xbytesview Cookie,
	const xsshkexinitconfig* pConfig
)
{
	xstrview arrLists[10];
	xbytesview arrInputs[12];
	xsshwriter Writer;
	size_t iTotalSize = 1u + XSSH_KEX_COOKIE_SIZE + 1u + 4u;
	size_t i;
	xsshcode Code;

	if ( (pWriter == NULL) || (Cookie.Data == NULL) ||
		(Cookie.Size != XSSH_KEX_COOKIE_SIZE) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!xsshKexRoleValid(pConfig->Role) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrLists[0] = xsshKexList(
		pConfig->KexAlgorithms,
		xsshKexDefaultAlgorithms(pConfig->Role, pConfig->Initial)
	);
	arrLists[1] = xsshKexList(
		pConfig->ServerHostKeyAlgorithms,
		XRT_STR_LITERAL(XSSH_HOSTKEY_DEFAULT)
	);
	arrLists[2] = xsshKexList(
		pConfig->EncryptionClientToServer,
		XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT)
	);
	arrLists[3] = xsshKexList(
		pConfig->EncryptionServerToClient,
		XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT)
	);
	arrLists[4] = xsshKexList(
		pConfig->MacClientToServer,
		XRT_STR_LITERAL(XSSH_MAC_DEFAULT)
	);
	arrLists[5] = xsshKexList(
		pConfig->MacServerToClient,
		XRT_STR_LITERAL(XSSH_MAC_DEFAULT)
	);
	arrLists[6] = xsshKexList(
		pConfig->CompressionClientToServer,
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT)
	);
	arrLists[7] = xsshKexList(
		pConfig->CompressionServerToClient,
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT)
	);
	arrLists[8] = xsshKexList(
		pConfig->LanguagesClientToServer,
		XRT_STR_LITERAL("")
	);
	arrLists[9] = xsshKexList(
		pConfig->LanguagesServerToClient,
		XRT_STR_LITERAL("")
	);
	for ( i = 0u; i < 10u; ++i ) {
		Code = xsshKexListSize(arrLists[i], &iTotalSize);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( !xsshKexConfigIndicatorsValid(pConfig, arrLists[0]) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrInputs[0] = Cookie;
	arrInputs[1].Data = (const unsigned char*)pConfig;
	arrInputs[1].Size = sizeof(*pConfig);
	for ( i = 0u; i < 10u; ++i ) {
		arrInputs[i + 2u].Data = (const unsigned char*)arrLists[i].Data;
		arrInputs[i + 2u].Size = arrLists[i].Size;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotalSize,
		arrInputs,
		12u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_KEXINIT) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Cookie) != XSSH_OK) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = 0u; i < 10u; ++i ) {
		if ( xrtSshWriteString(
			&Writer,
			(xbytesview){
				(const unsigned char*)arrLists[i].Data,
				arrLists[i].Size
			}
		) != XSSH_OK ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( (xrtSshWriteBool(
		&Writer,
		pConfig->FirstKexPacketFollows
	) != XSSH_OK) || (xrtSshWriteU32(&Writer, 0u) != XSSH_OK) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 在 reader 副本中完成全量解析，成功后发布所有借用视图。 */
xsshcode xrtSshKexInitRead(
	xbytesview Payload,
	xsshkexinit* pKexInit
)
{
	xsshreader Reader;
	xsshkexinit KexInit;
	uint8 iMessage;
	uint32 iReserved;
	xsshcode Code;

	if ( (pKexInit == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&KexInit, 0, sizeof(KexInit));
	if ( !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_KEXINIT ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBytes(
		&Reader,
		XSSH_KEX_COOKIE_SIZE,
		&KexInit.Cookie
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xsshKexReadList(&Reader, &KexInit.KexAlgorithms)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.ServerHostKeyAlgorithms
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.EncryptionClientToServer
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.EncryptionServerToClient
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(&Reader, &KexInit.MacClientToServer)) != XSSH_OK) ||
		((Code = xsshKexReadList(&Reader, &KexInit.MacServerToClient)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.CompressionClientToServer
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.CompressionServerToClient
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.LanguagesClientToServer
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.LanguagesServerToClient
		)) != XSSH_OK) ) {
		return Code;
	}
	Code = xrtSshReadBool(&Reader, &KexInit.FirstKexPacketFollows);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iReserved);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (iReserved != 0u) || (xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pKexInit = KexInit;
	return XSSH_OK;
}



/* 已知 AEAD cipher 不需要传统 MAC 选择。 */
bool xrtSshCipherIsAead(xstrview Cipher)
{
	return xsshKexEqual(
		Cipher,
		XRT_STR_LITERAL("aes128-gcm@openssh.com")
	) || xsshKexEqual(
		Cipher,
		XRT_STR_LITERAL("aes256-gcm@openssh.com")
	) || xsshKexEqual(
		Cipher,
		XRT_STR_LITERAL("chacha20-poly1305@openssh.com")
	);
}



/* 按客户端列表顺序协商；AEAD 方向保留空 MAC 视图。 */
xsshcode xrtSshKexNegotiate(
	const xsshkexinit* pClient,
	const xsshkexinit* pServer,
	xsshkexnegotiation* pNegotiation
)
{
	xsshkexnegotiation Negotiation;
	xsshcode Code;

	if ( (pClient == NULL) || (pServer == NULL) ||
		(pNegotiation == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&Negotiation, 0, sizeof(Negotiation));
	if ( ((Code = xrtSshNameListFirstMatch(
		pClient->KexAlgorithms,
		pServer->KexAlgorithms,
		&Negotiation.KexAlgorithm
	)) != XSSH_OK) || ((Code = xrtSshNameListFirstMatch(
		pClient->ServerHostKeyAlgorithms,
		pServer->ServerHostKeyAlgorithms,
		&Negotiation.ServerHostKeyAlgorithm
	)) != XSSH_OK) || ((Code = xrtSshNameListFirstMatch(
		pClient->EncryptionClientToServer,
		pServer->EncryptionClientToServer,
		&Negotiation.CipherClientToServer
	)) != XSSH_OK) || ((Code = xrtSshNameListFirstMatch(
		pClient->EncryptionServerToClient,
		pServer->EncryptionServerToClient,
		&Negotiation.CipherServerToClient
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( xsshKexIndicator(Negotiation.KexAlgorithm) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( !xrtSshCipherIsAead(Negotiation.CipherClientToServer) ) {
		Code = xrtSshNameListFirstMatch(
			pClient->MacClientToServer,
			pServer->MacClientToServer,
			&Negotiation.MacClientToServer
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( !xrtSshCipherIsAead(Negotiation.CipherServerToClient) ) {
		Code = xrtSshNameListFirstMatch(
			pClient->MacServerToClient,
			pServer->MacServerToClient,
			&Negotiation.MacServerToClient
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( ((Code = xrtSshNameListFirstMatch(
		pClient->CompressionClientToServer,
		pServer->CompressionClientToServer,
		&Negotiation.CompressionClientToServer
	)) != XSSH_OK) || ((Code = xrtSshNameListFirstMatch(
		pClient->CompressionServerToClient,
		pServer->CompressionServerToClient,
		&Negotiation.CompressionServerToClient
	)) != XSSH_OK) ) {
		return Code;
	}
	*pNegotiation = Negotiation;
	return XSSH_OK;
}



/* 解析首次 KEX 的角色相关扩展标记。 */
xsshcode xrtSshKexFeatures(
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	xsshrole Role,
	bool bInitial,
	xsshkexfeatures* pFeatures
)
{
	xsshkexfeatures Features;
	xstrview LocalExt;
	xstrview PeerExt;
	xstrview LocalStrict;
	xstrview PeerStrict;
	xstrview LocalStrictPreStandard;
	xstrview PeerStrictPreStandard;

	if ( !xrtMemRangeValid(pLocal, sizeof(*pLocal)) ||
		!xrtMemRangeValid(pPeer, sizeof(*pPeer)) ||
		!xrtMemRangeValid(pFeatures, sizeof(*pFeatures)) ||
		!xsshKexRoleValid(Role) ||
		!xrtSshNameListValid(pLocal->KexAlgorithms) ||
		!xrtSshNameListValid(pPeer->KexAlgorithms) ||
		xrtMemRangesOverlap(
			pFeatures,
			sizeof(*pFeatures),
			pLocal,
			sizeof(*pLocal)
		) || xrtMemRangesOverlap(
			pFeatures,
			sizeof(*pFeatures),
			pPeer,
			sizeof(*pPeer)
		) || xrtMemRangesOverlap(
			pFeatures,
			sizeof(*pFeatures),
			pLocal->KexAlgorithms.Data,
			pLocal->KexAlgorithms.Size
		) || xrtMemRangesOverlap(
			pFeatures,
			sizeof(*pFeatures),
			pPeer->KexAlgorithms.Data,
			pPeer->KexAlgorithms.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	LocalExt = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT) :
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER);
	PeerExt = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER) :
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT);
	LocalStrict = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER);
	PeerStrict = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT);
	LocalStrictPreStandard = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD);
	PeerStrictPreStandard = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD);
	if ( bInitial ) {
		if ( xrtSshNameListContains(pLocal->KexAlgorithms, PeerExt) ||
			xrtSshNameListContains(pLocal->KexAlgorithms, PeerStrict) ||
			xrtSshNameListContains(
				pLocal->KexAlgorithms,
				PeerStrictPreStandard
			) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		if ( xrtSshNameListContains(pPeer->KexAlgorithms, LocalExt) ||
			xrtSshNameListContains(pPeer->KexAlgorithms, LocalStrict) ||
			xrtSshNameListContains(
				pPeer->KexAlgorithms,
				LocalStrictPreStandard
			) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	memset(&Features, 0, sizeof(Features));
	if ( bInitial ) {
		Features.AcceptExtInfo = xrtSshNameListContains(
			pLocal->KexAlgorithms,
			LocalExt
		);
		Features.SendExtInfo = xrtSshNameListContains(
			pPeer->KexAlgorithms,
			PeerExt
		);
		Features.Strict = (xrtSshNameListContains(
			pLocal->KexAlgorithms,
			LocalStrict
		) && xrtSshNameListContains(
			pPeer->KexAlgorithms,
			PeerStrict
		)) || (xrtSshNameListContains(
			pLocal->KexAlgorithms,
			LocalStrictPreStandard
		) && xrtSshNameListContains(
			pPeer->KexAlgorithms,
			PeerStrictPreStandard
		));
	}
	*pFeatures = Features;
	return XSSH_OK;
}



/* first_kex_packet_follows 只在 peer 前两项猜测均正确时保留下一包。 */
xsshcode xrtSshKexGuessSkip(
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	bool* pSkip
)
{
	xstrview FirstKex;
	xstrview FirstHostKey;
	xsshcode Code;

	if ( (pPeer == NULL) || (pNegotiation == NULL) || (pSkip == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !pPeer->FirstKexPacketFollows ) {
		*pSkip = false;
		return XSSH_OK;
	}
	Code = xsshKexFirstAlgorithm(pPeer->KexAlgorithms, &FirstKex);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshKexFirst(
		pPeer->ServerHostKeyAlgorithms,
		&FirstHostKey
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pSkip = !xsshKexEqual(FirstKex, pNegotiation->KexAlgorithm) ||
		!xsshKexEqual(
			FirstHostKey,
			pNegotiation->ServerHostKeyAlgorithm
		);
	return XSSH_OK;
}

#endif
