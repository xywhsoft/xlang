#include <xrt/ssh_kex_curve25519_random.h>



#if defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM)

/* 复用 XRT 的安全随机密钥对生成与输出原子性。 */
xsshcode xrtSshCurve25519KeyPair(
	void* pPrivate,
	void* pPublic
)
{
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtX25519KeyPair(pPrivate, pPublic) ?
		XSSH_OK : XSSH_ERROR_STATE;
}

#endif
