#include <string.h>

#include <xrt/ssh_client_core.h>
#include <xrt/ssh_transport_message.h>



#if defined(XSSH_FEATURE_CLIENT_CORE)

#define XSSH_CLIENT_CORE_GUARD UINT32_C(0x5343434f)



typedef xsshcode (*xsshclientbuildproc)(xsshwriter* pWriter, ptr pData);



typedef struct xsshclientauthbuild {
	xsshclientcore* Client;
	xsshclientauth Auth;
} xsshclientauthbuild;



/* 验证核心哨兵和动态输出边界。 */
static bool xsshClientCoreValid(const xsshclientcore* pClient)
{
	return xrtMemRangeValid(pClient, sizeof(*pClient)) &&
		(pClient->Guard == XSSH_CLIENT_CORE_GUARD) &&
		pClient->Initialized &&
		(((pClient->OutputCapacity == 0u) &&
		  (pClient->Output == NULL)) ||
		 ((pClient->OutputCapacity != 0u) &&
		  (pClient->Output != NULL))) &&
		(pClient->OutputCapacity <= pClient->Config.OutputLimit);
}



/* 在不改变上限的前提下扩展敏感输出，失败时保留已清零的旧缓冲。 */
static bool xsshClientCoreGrow(xsshclientcore* pClient)
{
	bytes pOutput;
	size_t iCapacity;

	if ( pClient->OutputCapacity >= pClient->Config.OutputLimit ) {
		return false;
	}
	iCapacity = pClient->OutputCapacity;
	if ( iCapacity == 0u ) {
		iCapacity = pClient->Config.OutputInitial;
	} else if ( iCapacity <= (pClient->Config.OutputLimit / 2u) ) {
		iCapacity *= 2u;
	} else {
		iCapacity = pClient->Config.OutputLimit;
	}
	if ( pClient->Output != NULL ) {
		xrtSecureZero(pClient->Output, pClient->OutputCapacity);
		pOutput = (bytes)xrtRealloc(pClient->Output, iCapacity);
	} else {
		pOutput = (bytes)xrtMalloc(iCapacity);
	}
	if ( pOutput == NULL ) {
		return false;
	}
	pClient->Output = pOutput;
	pClient->OutputCapacity = iCapacity;
	return true;
}



/* 对事务式构建器执行有界扩容重试，并发布唯一稳定 payload。 */
static xsshcode xsshClientCoreBuild(
	xsshclientcore* pClient,
	xsshclientbuildproc pBuild,
	ptr pData,
	xbytesview* pPayload
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( (pClient->OutputCapacity == 0u) &&
		!xsshClientCoreGrow(pClient) ) {
		return XSSH_ERROR_SPACE;
	}
	for ( ;; ) {
		xrtSecureZero(pClient->Output, pClient->OutputCapacity);
		if ( !xrtSshWriterInit(
			&Writer,
			pClient->Output,
			pClient->OutputCapacity
		) ) {
			return XSSH_ERROR_STATE;
		}
		Code = pBuild(&Writer, pData);
		if ( Code != XSSH_ERROR_SPACE ) {
			break;
		}
		if ( !xsshClientCoreGrow(pClient) ) {
			return XSSH_ERROR_SPACE;
		}
	}
	if ( Code != XSSH_OK ) {
		xrtSecureZero(pClient->Output, pClient->OutputCapacity);
		return Code;
	}
	*pPayload = (xbytesview){ pClient->Output, Writer.Size };
	return XSSH_OK;
}



/* 构建本轮初始或 rekey KEXINIT。 */
static xsshcode xsshClientCoreKexInitBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	return xrtSshKexInitWriteSecure(
		pWriter,
		(const xsshkexinitconfig*)pData
	);
}



/* 构建客户端 Curve25519 方法首包。 */
static xsshcode xsshClientCoreEcdhBuild(xsshwriter* pWriter, ptr pData)
{
	return xrtSshKexSessionEcdhInitPrepare(
		(xsshkexsession*)pData,
		pWriter
	);
}



/* 构建当前 KEX 的 NEWKEYS。 */
static xsshcode xsshClientCoreNewKeysBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	return xrtSshKexSessionNewKeysPrepare(
		(xsshkexsession*)pData,
		pWriter
	);
}



/* 构建 ssh-userauth service request。 */
static xsshcode xsshClientCoreServiceBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	(void)pData;
	return xrtSshServiceRequestWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
	);
}



/* 首次 none 探测只用于取得服务端支持的方法列表。 */
static xsshcode xsshClientCoreNoneBuild(xsshwriter* pWriter, ptr pData)
{
	return xrtSshAuthNoneWrite(pWriter, *(const xstrview*)pData);
}



/* 把自定义认证器适配到统一的动态构建循环。 */
static xsshcode xsshClientCoreAuthBuild(xsshwriter* pWriter, ptr pData)
{
	xsshclientauthbuild* pBuild = (xsshclientauthbuild*)pData;

	return pBuild->Client->Config.Authenticate(
		pBuild->Client,
		pWriter,
		&pBuild->Auth,
		pBuild->Client->Config.AuthenticateData
	);
}



/* 返回当前 KEX 方法会话。 */
static xsshkexsession* xsshClientCoreKex(xsshsessiontcp* pSession)
{
	xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
	xsshkexexchange* pExchange;

	if ( pCore == NULL ) {
		return NULL;
	}
	pExchange = xrtSshSessionCoreKex(pCore);
	return pExchange != NULL ?
		xrtSshKexExchangeSession(pExchange) : NULL;
}



/* 把服务端方法列表复制成独立稳定文本，旧结果只在成功后替换。 */
static xsshcode xsshClientCoreMethodsSet(
	xsshclientcore* pClient,
	xstrview Methods,
	bool bPartialSuccess
)
{
	char* sMethods = NULL;

	if ( Methods.Size == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( Methods.Size != 0u ) {
		sMethods = (char*)xrtMalloc(Methods.Size + 1u);
		if ( sMethods == NULL ) {
			return XSSH_ERROR_SPACE;
		}
		memcpy(sMethods, Methods.Data, Methods.Size);
		sMethods[Methods.Size] = '\0';
	}
	xrtFree(pClient->AuthMethods);
	pClient->AuthMethods = sMethods;
	pClient->AuthMethodsSize = Methods.Size;
	pClient->AuthPartialSuccess = bPartialSuccess;
	return XSSH_OK;
}



/* 写入客户端安全默认配置。 */
bool xrtSshClientCoreConfigInit(xsshclientcoreconfig* pConfig)
{
	xsshclientcoreconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	memset(&Config, 0, sizeof(Config));
	if ( !xrtSshKexInitConfigInit(
		&Config.Kex,
		XSSH_ROLE_CLIENT,
		true
	) ) {
		return false;
	}
	xrtSshAuthGuardPolicyInit(&Config.AuthGuard);
	Config.Version = XRT_STR_LITERAL(XSSH_CLIENT_VERSION_DEFAULT);
	Config.OutputInitial = XSSH_CLIENT_OUTPUT_INITIAL_DEFAULT;
	Config.OutputLimit = XSSH_CLIENT_OUTPUT_LIMIT_DEFAULT;
	Config.ProbeNone = true;
	*pConfig = Config;
	return true;
}



/* 初始化不拥有会话和 Reader 的客户端动作核心。 */
bool xrtSshClientCoreInit(
	xsshclientcore* pClient,
	const xsshclientcoreconfig* pConfig
)
{
	char arrBanner[XSSH_IDENTIFICATION_MAX + 2u];
	xsshclientcore Client;
	xsshwriter Writer;

	if ( !xrtMemRangeValid(pClient, sizeof(*pClient)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			pConfig,
			sizeof(*pConfig)
		) || (pConfig->Kex.Role != XSSH_ROLE_CLIENT) ||
		(pConfig->OutputInitial == 0u) ||
		(pConfig->OutputInitial > pConfig->OutputLimit) ||
		!xrtMemRangeValid(pConfig->Version.Data, pConfig->Version.Size) ||
		!xrtMemRangeValid(pConfig->User.Data, pConfig->User.Size) ) {
		return false;
	}
	if ( !xrtSshWriterInit(&Writer, arrBanner, sizeof(arrBanner)) ||
		(xrtSshBannerWrite(&Writer, pConfig->Version) != XSSH_OK) ) {
		return false;
	}
	memset(&Client, 0, sizeof(Client));
	Client.Config = *pConfig;
	Client.Initialized = true;
	Client.Guard = XSSH_CLIENT_CORE_GUARD;
	*pClient = Client;
	return true;
}



/* 清理认证敏感输出与方法副本。 */
void xrtSshClientCoreClear(xsshclientcore* pClient)
{
	if ( xsshClientCoreValid(pClient) ) {
		if ( pClient->Output != NULL ) {
			xrtSecureZero(pClient->Output, pClient->OutputCapacity);
			xrtFree(pClient->Output);
		}
		xrtFree(pClient->AuthMethods);
	}
	if ( xrtMemRangeValid(pClient, sizeof(*pClient)) ) {
		xrtSecureZero(pClient, sizeof(*pClient));
	}
}



/* 推进客户端动作直到出现外部边界。 */
xsshcode xrtSshClientCoreNext(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession,
	const xsshsessionreader* pReader,
	uint64 iNowMs,
	xsshclientnext* pNext
)
{
	xsshclientnext Next;
	uint32 iSteps;

	if ( !xsshClientCoreValid(pClient) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pNext, sizeof(*pNext)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			pNext,
			sizeof(*pNext)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pNext,
			sizeof(*pNext)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&Next, 0, sizeof(Next));
	for ( iSteps = 0u; iSteps < 32u; ++iSteps ) {
		xsshsessionaction Action = xrtSshSessionTcpAction(pSession);
		xsshcode Code;

		if ( Action == XSSH_SESSION_ACTION_WRITE_IDENTIFICATION ) {
			Next.Kind = XSSH_CLIENT_NEXT_IDENTIFICATION;
			Next.Text = pClient->Config.Version;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_KEXINIT ) {
			xsshkexinitconfig Kex = pClient->Config.Kex;
			xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
			const xsshkexexchange* pExchange = pCore != NULL ?
				xrtSshSessionCoreKexConst(pCore) : NULL;

			if ( pExchange == NULL ) {
				return XSSH_ERROR_STATE;
			}
			Kex.Initial = !pExchange->Session.HasSessionId;
			Code = xsshClientCoreBuild(
				pClient,
				xsshClientCoreKexInitBuild,
				&Kex,
				&Next.Data
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_BEGIN_KEX ) {
			Code = xrtSshSessionTcpKexBegin(
				pSession,
				(xbytesview){ NULL, 0u }
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			continue;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_ECDH_INIT ) {
			xsshkexsession* pKex = xsshClientCoreKex(pSession);

			if ( pKex == NULL ) {
				return XSSH_ERROR_STATE;
			}
			Code = xsshClientCoreBuild(
				pClient,
				xsshClientCoreEcdhBuild,
				pKex,
				&Next.Data
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_VERIFY_HOST_KEY ) {
			xsshkexsession* pKex = xsshClientCoreKex(pSession);
			xsshclienthost Host;
			xsshclienthostdecision Decision;

			if ( (pKex == NULL) ||
				!xrtMemRangeValid(pReader, sizeof(*pReader)) ) {
				return XSSH_ERROR_STATE;
			}
			memset(&Host, 0, sizeof(Host));
			Host.Key = xrtSshSessionReaderHostKey(pReader);
			Code = xrtSshKexSessionNegotiation(
				pKex,
				&Host.Negotiation
			);
			if ( (Code != XSSH_OK) || (Host.Key.Size == 0u) ) {
				return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
			}
			Decision = pClient->Config.HostKey != NULL ?
				pClient->Config.HostKey(
					pClient,
					&Host,
					pClient->Config.HostKeyData
				) : XSSH_CLIENT_HOST_REJECT;
			if ( Decision == XSSH_CLIENT_HOST_DEFER ) {
				Next.Kind = XSSH_CLIENT_NEXT_HOST_KEY;
				*pNext = Next;
				return XSSH_OK;
			}
			if ( Decision != XSSH_CLIENT_HOST_ACCEPT ) {
				xrtSshKexSessionFail(pKex);
				return XSSH_ERROR_AUTHENTICATION;
			}
			Code = xrtSshKexSessionHostKeyAccept(pKex);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			continue;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_NEWKEYS ) {
			xsshkexsession* pKex = xsshClientCoreKex(pSession);

			if ( pKex == NULL ) {
				return XSSH_ERROR_STATE;
			}
			Code = xsshClientCoreBuild(
				pClient,
				xsshClientCoreNewKeysBuild,
				pKex,
				&Next.Data
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_BEGIN_AUTH ) {
			Code = xrtSshSessionTcpAuthBegin(
				pSession,
				&pClient->Config.AuthGuard,
				iNowMs
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			continue;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_SERVICE_REQUEST ) {
			Code = xsshClientCoreBuild(
				pClient,
				xsshClientCoreServiceBuild,
				NULL,
				&Next.Data
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_AUTH_REQUEST ) {
			xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
			xsshauthsession* pAuthSession = pCore != NULL ?
				xrtSshSessionCoreAuth(pCore) : NULL;
			xsshkexsession* pKex = xsshClientCoreKex(pSession);
			xsshauthguard Budget;
			xsshclientauthbuild Build;

			if ( (pAuthSession == NULL) || (pKex == NULL) ||
				(xrtSshAuthSessionBudget(
					pAuthSession,
					&Budget
				) != XSSH_OK) ) {
				return XSSH_ERROR_STATE;
			}
			if ( pClient->Config.ProbeNone && (Budget.Attempts == 0u) ) {
				Code = xsshClientCoreBuild(
					pClient,
					xsshClientCoreNoneBuild,
					&pClient->Config.User,
					&Next.Data
				);
			} else if ( pClient->Config.Authenticate == NULL ) {
				Next.Kind = XSSH_CLIENT_NEXT_AUTH;
				*pNext = Next;
				return XSSH_OK;
			} else {
				memset(&Build, 0, sizeof(Build));
				Build.Client = pClient;
				Build.Auth.User = pClient->Config.User;
				Build.Auth.Methods = (xstrview){
					pClient->AuthMethods,
					pClient->AuthMethodsSize
				};
				Build.Auth.Attempts = Budget.Attempts;
				Build.Auth.PartialSuccess =
					pClient->AuthPartialSuccess;
				Code = xrtSshKexSessionId(
					pKex,
					&Build.Auth.SessionId
				);
				if ( Code == XSSH_OK ) {
					Code = xsshClientCoreBuild(
						pClient,
						xsshClientCoreAuthBuild,
						&Build,
						&Next.Data
					);
				}
			}
			if ( Code == XSSH_NEED_MORE ) {
				Next.Kind = XSSH_CLIENT_NEXT_AUTH;
				*pNext = Next;
				return XSSH_OK;
			}
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_CONNECTION ) {
			Next.Kind = XSSH_CLIENT_NEXT_READY;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( (Action == XSSH_SESSION_ACTION_READ_IDENTIFICATION) ||
			(Action == XSSH_SESSION_ACTION_READ_KEXINIT) ||
			(Action == XSSH_SESSION_ACTION_READ_ECDH_REPLY) ||
			(Action == XSSH_SESSION_ACTION_READ_NEWKEYS) ||
			(Action == XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT) ||
			(Action == XSSH_SESSION_ACTION_READ_AUTH_RESULT) ) {
			Next.Kind = XSSH_CLIENT_NEXT_INPUT;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( (Action == XSSH_SESSION_ACTION_WRITE_PENDING) ||
			(Action == XSSH_SESSION_ACTION_READ_PENDING) ) {
			Next.Kind = XSSH_CLIENT_NEXT_TRANSACTION;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_CLOSING ) {
			Next.Kind = XSSH_CLIENT_NEXT_CLOSING;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_FAILED ) {
			return XSSH_ERROR_STATE;
		}
		return XSSH_ERROR_STATE;
	}
	return XSSH_ERROR_STATE;
}



/* 保存认证失败方法，使 packet 提交后的认证器不再借用输入。 */
xsshcode xrtSshClientCoreObserve(
	xsshclientcore* pClient,
	const xsshsessiontcp* pSession,
	const xsshsessiontcppacket* pPacket
)
{
	const xsshsessioncore* pCore;
	const xsshauthsession* pAuth;
	xsshauthfailure Failure;

	if ( !xsshClientCoreValid(pClient) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pPacket->Session.Kind != XSSH_SESSION_PACKET_AUTH) ||
		(pPacket->Session.Message.Auth !=
		 XSSH_AUTH_SESSION_PACKET_FAILURE) ) {
		return XSSH_OK;
	}
	pCore = xrtSshSessionTcpCoreConst(pSession);
	pAuth = pCore != NULL ? xrtSshSessionCoreAuthConst(pCore) : NULL;
	if ( (pAuth == NULL) ||
		(xrtSshAuthSessionFailure(pAuth, &Failure) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	return xsshClientCoreMethodsSet(
		pClient,
		Failure.Methods,
		Failure.PartialSuccess
	);
}



/* 接受已完成密码学验证的当前主机密钥。 */
xsshcode xrtSshClientCoreHostKeyAccept(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession
)
{
	xsshkexsession* pKex;

	if ( !xsshClientCoreValid(pClient) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		(xrtSshSessionTcpAction(pSession) !=
		 XSSH_SESSION_ACTION_VERIFY_HOST_KEY) ) {
		return XSSH_ERROR_STATE;
	}
	pKex = xsshClientCoreKex(pSession);
	return pKex != NULL ?
		xrtSshKexSessionHostKeyAccept(pKex) : XSSH_ERROR_STATE;
}



/* 拒绝当前主机密钥并终止本轮 KEX。 */
xsshcode xrtSshClientCoreHostKeyReject(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession
)
{
	xsshkexsession* pKex;

	if ( !xsshClientCoreValid(pClient) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		(xrtSshSessionTcpAction(pSession) !=
		 XSSH_SESSION_ACTION_VERIFY_HOST_KEY) ) {
		return XSSH_ERROR_STATE;
	}
	pKex = xsshClientCoreKex(pSession);
	if ( pKex == NULL ) {
		return XSSH_ERROR_STATE;
	}
	xrtSshKexSessionFail(pKex);
	return XSSH_OK;
}



/* 返回稳定认证方法副本。 */
xstrview xrtSshClientCoreAuthMethods(
	const xsshclientcore* pClient,
	bool* pPartialSuccess
)
{
	if ( pPartialSuccess != NULL ) {
		*pPartialSuccess = false;
	}
	if ( !xsshClientCoreValid(pClient) ) {
		return (xstrview){ NULL, 0u };
	}
	if ( pPartialSuccess != NULL ) {
		*pPartialSuccess = pClient->AuthPartialSuccess;
	}
	return (xstrview){
		pClient->AuthMethods,
		pClient->AuthMethodsSize
	};
}



/* 使用显式借用口令构建 password 认证。 */
xsshcode xrtSshClientPasswordAuth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
)
{
	const xstrview* pPassword = (const xstrview*)pUserData;

	(void)pClient;
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pAuth, sizeof(*pAuth)) ||
		!xrtMemRangeValid(pPassword, sizeof(*pPassword)) ||
		!xrtMemRangeValid(pPassword->Data, pPassword->Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pAuth->Methods.Size != 0u) &&
		!xrtSshNameListContains(
			pAuth->Methods,
			XRT_STR_LITERAL(XSSH_AUTH_METHOD_PASSWORD)
		) ) {
		return XSSH_ERROR_AUTHENTICATION;
	}
	return xrtSshAuthPasswordWrite(
		pWriter,
		pAuth->User,
		*pPassword
	);
}

#endif
