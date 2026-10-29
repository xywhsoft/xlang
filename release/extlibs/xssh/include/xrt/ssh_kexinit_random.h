#ifndef XRT_SSH_KEXINIT_RANDOM_H
#define XRT_SSH_KEXINIT_RANDOM_H

#include <xrt/ssh_kexinit.h>
#include <xrt/random.h>



#if defined(XSSH_FEATURE_KEXINIT_RANDOM) && \
	(!defined(XSSH_FEATURE_KEXINIT) || !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XSSH_FEATURE_KEXINIT_RANDOM requires kexinit and random_secure"
#endif



#if defined(XSSH_FEATURE_KEXINIT_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统安全随机 cookie 和显式角色配置构建 KEXINIT payload。 */
XRT_API xsshcode xrtSshKexInitWriteSecure(
	xsshwriter* pWriter,
	const xsshkexinitconfig* pConfig
);



XRT_EXTERN_C_END

#endif

#endif
