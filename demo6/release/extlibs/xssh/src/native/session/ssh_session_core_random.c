#include <xrt/ssh_session_core_random.h>



#if defined(XSSH_FEATURE_SESSION_CORE_RANDOM)

/* 使用系统安全随机临时密钥开始当前 KEX。 */
xsshcode xrtSshSessionCoreKexBegin(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey
)
{
	xsshkexexchange* pKex = xrtSshSessionCoreKex(pSession);

	if ( (pKex == NULL) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || pCore->Write.Active ||
		pCore->Read.Active ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeBegin(pKex, pCore, ServerHostKey);
}

#endif
