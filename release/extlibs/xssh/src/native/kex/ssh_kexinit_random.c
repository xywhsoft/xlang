#include <xrt/memory.h>
#include <xrt/ssh_kexinit_random.h>



#if defined(XSSH_FEATURE_KEXINIT_RANDOM)

/* 生成一次性 cookie，并在构建后清除栈副本。 */
xsshcode xrtSshKexInitWriteSecure(
	xsshwriter* pWriter,
	const xsshkexinitconfig* pConfig
)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE];
	xsshcode Code;

	if ( (pWriter == NULL) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		((pConfig->Role != XSSH_ROLE_CLIENT) &&
		 (pConfig->Role != XSSH_ROLE_SERVER)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSecureRandom(arrCookie, sizeof(arrCookie)) ) {
		return XSSH_ERROR_CALLBACK;
	}
	Code = xrtSshKexInitWrite(
		pWriter,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		pConfig
	);
	xrtSecureZero(arrCookie, sizeof(arrCookie));
	return Code;
}

#endif
