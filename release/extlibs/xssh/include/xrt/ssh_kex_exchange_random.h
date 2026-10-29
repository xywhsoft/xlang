#ifndef XRT_SSH_KEX_EXCHANGE_RANDOM_H
#define XRT_SSH_KEX_EXCHANGE_RANDOM_H

#include <xrt/ssh_kex_exchange.h>
#include <xrt/ssh_kex_curve25519_random.h>



#if defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) && \
	(!defined(XSSH_FEATURE_KEX_EXCHANGE) || \
	 !defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM))
	#error "XSSH_FEATURE_KEX_EXCHANGE_RANDOM requires KEX exchange and secure Curve25519 keys"
#endif



#if defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统安全随机临时私钥开始本代 KEX，并在返回前清除临时副本。 */
XRT_API xsshcode xrtSshKexExchangeBegin(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey
);



XRT_EXTERN_C_END

#endif

#endif
