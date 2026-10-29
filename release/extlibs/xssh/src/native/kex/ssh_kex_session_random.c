#include <xrt/ssh_kex_session_random.h>



#if defined(XSSH_FEATURE_KEX_SESSION_RANDOM)

/* 生成临时密钥，并保证所有退出路径都清除私钥副本。 */
xsshcode xrtSshKexSessionBegin(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	const xsshkextranscript* pTranscript,
	xbytesview ServerHostKey
)
{
	uint8 arrPrivate[XSSH_CURVE25519_PRIVATE_SIZE];
	uint8 arrPublic[XSSH_CURVE25519_PUBLIC_SIZE];
	xsshcode Code;

	Code = xrtSshCurve25519KeyPair(arrPrivate, arrPublic);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexSessionBeginWithPrivate(
			pSession,
			pCore,
			pTranscript,
			ServerHostKey,
			(xbytesview){ arrPrivate, sizeof(arrPrivate) }
		);
	}
	xrtSecureZero(arrPrivate, sizeof(arrPrivate));
	xrtSecureZero(arrPublic, sizeof(arrPublic));
	return Code;
}

#endif
