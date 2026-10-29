#include <xrt/ssh_hostkey_ed25519.h>



#if defined(XSSH_FEATURE_HOSTKEY_ED25519)

/* 先完成 SSH 结构校验，再把可信边界交给 XRT Ed25519。 */
xsshcode xrtSshEd25519HostKeyVerify(
	xbytesview PublicKeyBlob,
	xbytesview SignatureBlob,
	xbytesview Message
)
{
	xbytesview PublicKey;
	xbytesview Signature;
	xsshcode Code;

	if ( (Message.Data == NULL) && (Message.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshEd25519PublicKeyRead(PublicKeyBlob, &PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshEd25519SignatureRead(SignatureBlob, &Signature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtEd25519Verify(
		PublicKey.Data,
		Message.Data,
		Message.Size,
		Signature.Data
	) ? XSSH_OK : XSSH_ERROR_AUTHENTICATION;
}

#endif
