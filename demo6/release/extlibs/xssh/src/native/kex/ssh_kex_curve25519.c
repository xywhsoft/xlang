#include <xrt/ssh_kex_curve25519.h>



#if defined(XSSH_FEATURE_KEX_CURVE25519)

/* 复用 XRT X25519 公钥导出，并映射为 SSH 稳定结果码。 */
xsshcode xrtSshCurve25519Public(
	const void* pPrivate,
	void* pPublic
)
{
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtX25519Public(pPrivate, pPublic) ?
		XSSH_OK : XSSH_ERROR_STATE;
}



/* XRT 已以常量时间完成全零共享秘密拒绝。 */
xsshcode xrtSshCurve25519Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
)
{
	if ( (pPrivate == NULL) || (pPeerPublic == NULL) ||
		(pShared == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtX25519Shared(pPrivate, pPeerPublic, pShared) ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}

#endif
