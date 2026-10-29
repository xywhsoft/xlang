#include <xrt/ssh_kex_exchange_random.h>



#if defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM)

/* 用安全随机临时密钥委托给确定性交换核心。 */
xsshcode xrtSshKexExchangeBegin(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey
)
{
	uint8 arrPrivate[XSSH_CURVE25519_PRIVATE_SIZE];
	uint8 arrPublic[XSSH_CURVE25519_PUBLIC_SIZE];
	xsshcode Code;

	if ( !xrtSshKexExchangeReady(pExchange, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshCurve25519KeyPair(arrPrivate, arrPublic);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexExchangeBeginWithPrivate(
			pExchange,
			pCore,
			ServerHostKey,
			(xbytesview){ arrPrivate, sizeof(arrPrivate) }
		);
	}
	xrtSecureZero(arrPrivate, sizeof(arrPrivate));
	xrtSecureZero(arrPublic, sizeof(arrPublic));
	return Code;
}

#endif
