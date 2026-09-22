#ifndef XSSH_KNOWN_HOST_INTERNAL_H
#define XSSH_KNOWN_HOST_INTERNAL_H

#include <xrt/ssh_known_host.h>



/* 非默认端口使用虚拟 [host]:port 视图，避免为任意长度主机分配缓冲。 */
typedef struct xsshknownhosttarget {
	xstrview Host;
	char Port[5];
	size_t PortSize;
	size_t Size;
	bool Bracketed;
} xsshknownhosttarget;



/* 校验原始主机与端口，并预计算十进制端口。 */
static inline bool xsshKnownHostTargetInit(
	xsshknownhosttarget* pTarget,
	xstrview Host,
	uint32 iPort
)
{
	char arrReverse[5];
	size_t iPortSize = 0u;
	size_t i;
	uint32 iValue = iPort;

	if ( (pTarget == NULL) || !xrtMemRangeValid(Host.Data, Host.Size) ||
		(Host.Size == 0u) || (iPort == 0u) || (iPort > 65535u) ||
		(Host.Size > (SIZE_MAX - 8u)) ) {
		return false;
	}
	for ( i = 0u; i < Host.Size; ++i ) {
		unsigned char iCharacter = (unsigned char)Host.Data[i];

		if ( (iCharacter <= 0x20u) || (iCharacter == 0x7fu) ||
			(iCharacter == (unsigned char)',') ||
			(iCharacter == (unsigned char)'!') ||
			(iCharacter == (unsigned char)'*') ||
			(iCharacter == (unsigned char)'?') ||
			(iCharacter == (unsigned char)'[') ||
			(iCharacter == (unsigned char)']') ) {
			return false;
		}
	}
	do {
		arrReverse[iPortSize++] = (char)('0' + (iValue % 10u));
		iValue /= 10u;
	} while ( iValue != 0u );
	for ( i = 0u; i < iPortSize; ++i ) {
		pTarget->Port[i] = arrReverse[iPortSize - i - 1u];
	}
	pTarget->Host = Host;
	pTarget->PortSize = iPortSize;
	pTarget->Bracketed = iPort != XSSH_DEFAULT_PORT;
	pTarget->Size = pTarget->Bracketed ?
		Host.Size + iPortSize + 3u : Host.Size;
	return true;
}



/* 返回虚拟 target 的一个字节。 */
static inline unsigned char xsshKnownHostTargetAt(
	const xsshknownhosttarget* pTarget,
	size_t iIndex
)
{
	if ( !pTarget->Bracketed ) {
		return (unsigned char)pTarget->Host.Data[iIndex];
	}
	if ( iIndex == 0u ) {
		return (unsigned char)'[';
	}
	--iIndex;
	if ( iIndex < pTarget->Host.Size ) {
		return (unsigned char)pTarget->Host.Data[iIndex];
	}
	iIndex -= pTarget->Host.Size;
	if ( iIndex == 0u ) {
		return (unsigned char)']';
	}
	if ( iIndex == 1u ) {
		return (unsigned char)':';
	}
	return (unsigned char)pTarget->Port[iIndex - 2u];
}

#endif
