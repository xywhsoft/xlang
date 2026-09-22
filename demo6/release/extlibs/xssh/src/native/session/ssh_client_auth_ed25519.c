#include <xrt/ssh_client_auth_ed25519.h>



#if defined(XSSH_FEATURE_CLIENT_AUTH_ED25519)

#define XSSH_CLIENT_ED25519_SIGNATURE_CAPACITY \
	(8u + (sizeof(XSSH_HOSTKEY_ED25519) - 1u) + \
	 XSSH_ED25519_SIGNATURE_SIZE)



/* 使用客户端动态输出暂存签名原文，再把固定签名原子写成最终认证报文。 */
xsshcode xrtSshClientEd25519Auth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
)
{
	const xsshed25519identity* pIdentity =
		(const xsshed25519identity*)pUserData;
	unsigned char arrSignature[XSSH_CLIENT_ED25519_SIGNATURE_CAPACITY];
	xsshwriter SignData;
	xsshwriter Signature;
	xsshwriter Writer;
	xsshcode Code;

	(void)pClient;
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pAuth, sizeof(*pAuth)) ||
		!xrtMemRangeValid(pIdentity, sizeof(*pIdentity)) ||
		(pWriter->Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pAuth->Methods.Size != 0u) &&
		!xrtSshNameListContains(
			pAuth->Methods,
			XRT_STR_LITERAL(XSSH_AUTH_METHOD_PUBLICKEY)
		) ) {
		return XSSH_ERROR_AUTHENTICATION;
	}
	SignData = *pWriter;
	Code = xrtSshAuthPublicKeySignDataWrite(
		&SignData,
		pAuth->SessionId,
		pAuth->User,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		pIdentity->PublicKeyBlob
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshWriterInit(
		&Signature,
		arrSignature,
		sizeof(arrSignature)
	) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshPrivateKeyEd25519SignatureWrite(
		&Signature,
		pIdentity,
		(xbytesview){ SignData.Data, SignData.Size }
	);
	if ( Code == XSSH_OK ) {
		Writer = *pWriter;
		Code = xrtSshAuthPublicKeySignedWrite(
			&Writer,
			pAuth->User,
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			pIdentity->PublicKeyBlob,
			(xbytesview){ arrSignature, Signature.Size }
		);
		if ( Code == XSSH_OK ) {
			*pWriter = Writer;
		}
	}
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	return Code;
}

#endif
