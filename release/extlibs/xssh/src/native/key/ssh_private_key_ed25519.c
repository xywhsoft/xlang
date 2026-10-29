#include <xrt/ssh_private_key_ed25519.h>

#include <xrt/crypto.h>

#include <string.h>



#if defined(XSSH_FEATURE_PRIVATE_KEY_ED25519)

/* 比较借用文本与编译期名称。 */
static bool xsshPrivateEd25519NameEqual(
	xstrview Name,
	const char* sExpected,
	size_t iExpectedSize
)
{
	return (Name.Size == iExpectedSize) &&
		(memcmp(Name.Data, sExpected, iExpectedSize) == 0);
}



/* 完整私钥列表中的短输入统一视为协议错误。 */
static xsshcode xsshPrivateEd25519Complete(xsshcode Code)
{
	return Code == XSSH_NEED_MORE ? XSSH_ERROR_PROTOCOL : Code;
}



/* 校验借用身份字段和 seed/public 对应关系。 */
static xsshcode xsshPrivateEd25519Validate(
	const xsshed25519identity* pIdentity
)
{
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE];
	xbytesview BlobPublic;
	xsshcode Code;

	if ( !xrtMemRangeValid(pIdentity, sizeof(*pIdentity)) ||
		!xrtMemRangeValid(
			pIdentity->PublicKeyBlob.Data,
			pIdentity->PublicKeyBlob.Size
		) || !xrtMemRangeValid(pIdentity->Seed.Data, pIdentity->Seed.Size) ||
		!xrtMemRangeValid(
			pIdentity->PublicKey.Data,
			pIdentity->PublicKey.Size
		) || !xrtMemRangeValid(
			pIdentity->Comment.Data,
			pIdentity->Comment.Size
		) || (pIdentity->Seed.Size != XRT_ED25519_SEED_SIZE) ||
		(pIdentity->PublicKey.Size != XSSH_ED25519_PUBLIC_SIZE) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshEd25519PublicKeyRead(
		pIdentity->PublicKeyBlob,
		&BlobPublic
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtEd25519Public(pIdentity->Seed.Data, arrPublic) ) {
		xrtSecureZero(arrPublic, sizeof(arrPublic));
		return XSSH_ERROR_STATE;
	}
	if ( (memcmp(
		arrPublic,
		pIdentity->PublicKey.Data,
		sizeof(arrPublic)
	) != 0) || (memcmp(
		BlobPublic.Data,
		pIdentity->PublicKey.Data,
		sizeof(arrPublic)
	) != 0) ) {
		xrtSecureZero(arrPublic, sizeof(arrPublic));
		return XSSH_ERROR_PROTOCOL;
	}
	xrtSecureZero(arrPublic, sizeof(arrPublic));
	return XSSH_OK;
}



/* 解析 OpenSSH 单 Ed25519 私钥字段并校验 checkint、padding 和公私钥一致性。 */
xsshcode xrtSshPrivateKeyEd25519Read(
	const xsshopensshprivatekey* pPrivateKey,
	xsshed25519identity* pIdentity
)
{
	xsshopensshprivatekey PrivateKey;
	xsshprivatekeypublics Publics;
	xbytesview PublicKeyBlob;
	xbytesview BlobPublic;
	xbytesview KeyType;
	xbytesview PublicRaw;
	xbytesview PrivateRaw;
	xbytesview Comment;
	xsshreader Reader;
	xsshed25519identity Identity;
	unsigned char arrDerived[XSSH_ED25519_PUBLIC_SIZE];
	uint32 iCheckFirst;
	uint32 iCheckSecond;
	size_t iPadding;
	size_t i;
	bool bEncrypted;
	xsshcode Code;

	if ( !xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		!xrtMemRangeValid(pIdentity, sizeof(*pIdentity)) ||
		xrtMemRangesOverlap(
			pPrivateKey,
			sizeof(*pPrivateKey),
			pIdentity,
			sizeof(*pIdentity)
		) || xrtMemRangesOverlap(
			pPrivateKey->Blob.Data,
			pPrivateKey->Blob.Size,
			pIdentity,
			sizeof(*pIdentity)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPrivateKeyRead(pPrivateKey->Blob, &PrivateKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	bEncrypted = !xsshPrivateEd25519NameEqual(
		PrivateKey.Cipher,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	) || !xsshPrivateEd25519NameEqual(
		PrivateKey.Kdf,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	) || (PrivateKey.KdfOptions.Size != 0u);
	if ( bEncrypted || (PrivateKey.KeyCount != 1u) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( !xrtSshReaderInit(&Publics.Reader, PrivateKey.PublicKeys) ) {
		return XSSH_ERROR_STATE;
	}
	Publics.Remaining = PrivateKey.KeyCount;
	Code = xrtSshPrivateKeyPublicsNext(&Publics, &PublicKeyBlob);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshPrivateEd25519Complete(xrtSshEd25519PublicKeyRead(
		PublicKeyBlob,
		&BlobPublic
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshReaderInit(&Reader, PrivateKey.PrivateList) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPrivateEd25519Complete(xrtSshReadU32(
		&Reader,
		&iCheckFirst
	));
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadU32(
			&Reader,
			&iCheckSecond
		));
	}
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadString(
			&Reader,
			&KeyType
		));
	}
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadString(
			&Reader,
			&PublicRaw
		));
	}
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadString(
			&Reader,
			&PrivateRaw
		));
	}
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadString(
			&Reader,
			&Comment
		));
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (iCheckFirst != iCheckSecond) ||
		!xsshPrivateEd25519NameEqual(
			(xstrview){ (const char*)KeyType.Data, KeyType.Size },
			XSSH_HOSTKEY_ED25519,
			sizeof(XSSH_HOSTKEY_ED25519) - 1u
		) || (PublicRaw.Size != XSSH_ED25519_PUBLIC_SIZE) ||
		(PrivateRaw.Size !=
		 (XRT_ED25519_SEED_SIZE + XSSH_ED25519_PUBLIC_SIZE)) ||
		(memcmp(
			PrivateRaw.Data + XRT_ED25519_SEED_SIZE,
			PublicRaw.Data,
			XSSH_ED25519_PUBLIC_SIZE
		) != 0) || (memcmp(
			BlobPublic.Data,
			PublicRaw.Data,
			XSSH_ED25519_PUBLIC_SIZE
		) != 0) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iPadding = xrtSshReaderRemaining(&Reader);
	if ( iPadding > 255u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = 0u; i < iPadding; ++i ) {
		uint8 iByte;

		if ( (xrtSshReadByte(&Reader, &iByte) != XSSH_OK) ||
			(iByte != (uint8)(i + 1u)) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( !xrtEd25519Public(PrivateRaw.Data, arrDerived) ) {
		xrtSecureZero(arrDerived, sizeof(arrDerived));
		return XSSH_ERROR_STATE;
	}
	if ( memcmp(
		arrDerived,
		PublicRaw.Data,
		sizeof(arrDerived)
	) != 0 ) {
		xrtSecureZero(arrDerived, sizeof(arrDerived));
		return XSSH_ERROR_PROTOCOL;
	}
	xrtSecureZero(arrDerived, sizeof(arrDerived));
	Identity.PublicKeyBlob = PublicKeyBlob;
	Identity.Seed.Data = PrivateRaw.Data;
	Identity.Seed.Size = XRT_ED25519_SEED_SIZE;
	Identity.PublicKey = PublicRaw;
	Identity.Comment = Comment;
	*pIdentity = Identity;
	return XSSH_OK;
}



/* 在局部缓冲完成签名，成功后一次发布固定长度结果。 */
xsshcode xrtSshPrivateKeyEd25519Sign(
	const xsshed25519identity* pIdentity,
	xbytesview Message,
	void* pSignature
)
{
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	xsshcode Code;

	if ( !xrtMemRangeValid(Message.Data, Message.Size) ||
		!xrtMemRangeValid(pSignature, XSSH_ED25519_SIGNATURE_SIZE) ||
		xrtMemRangesOverlap(
			pIdentity,
			sizeof(*pIdentity),
			pSignature,
			XSSH_ED25519_SIGNATURE_SIZE
		) || xrtMemRangesOverlap(
			Message.Data,
			Message.Size,
			pSignature,
			XSSH_ED25519_SIGNATURE_SIZE
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPrivateEd25519Validate(pIdentity);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtMemRangesOverlap(
		pIdentity->PublicKeyBlob.Data,
		pIdentity->PublicKeyBlob.Size,
		pSignature,
		XSSH_ED25519_SIGNATURE_SIZE
	) || xrtMemRangesOverlap(
		pIdentity->Seed.Data,
		pIdentity->Seed.Size,
		pSignature,
		XSSH_ED25519_SIGNATURE_SIZE
	) || xrtMemRangesOverlap(
		pIdentity->PublicKey.Data,
		pIdentity->PublicKey.Size,
		pSignature,
		XSSH_ED25519_SIGNATURE_SIZE
	) || xrtMemRangesOverlap(
		pIdentity->Comment.Data,
		pIdentity->Comment.Size,
		pSignature,
		XSSH_ED25519_SIGNATURE_SIZE
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtEd25519Sign(
		pIdentity->Seed.Data,
		Message.Data,
		Message.Size,
		arrSignature
	) ) {
		xrtSecureZero(arrSignature, sizeof(arrSignature));
		return XSSH_ERROR_STATE;
	}
	memcpy(pSignature, arrSignature, sizeof(arrSignature));
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	return XSSH_OK;
}



/* 预留最终输出后签名，并复用通用 signature blob writer。 */
xsshcode xrtSshPrivateKeyEd25519SignatureWrite(
	xsshwriter* pWriter,
	const xsshed25519identity* pIdentity,
	xbytesview Message
)
{
	xbytesview arrInputs[5];
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	xsshwriter Writer;
	xsshcode Code;

	if ( !xrtMemRangeValid(pIdentity, sizeof(*pIdentity)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrInputs[0] = pIdentity->PublicKeyBlob;
	arrInputs[1] = pIdentity->Seed;
	arrInputs[2] = pIdentity->PublicKey;
	arrInputs[3] = pIdentity->Comment;
	arrInputs[4] = Message;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		8u + (sizeof(XSSH_HOSTKEY_ED25519) - 1u) +
			XSSH_ED25519_SIGNATURE_SIZE,
		arrInputs,
		5u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshPrivateKeyEd25519Sign(
		pIdentity,
		Message,
		arrSignature
	);
	if ( Code != XSSH_OK ) {
		xrtSecureZero(arrSignature, sizeof(arrSignature));
		return Code;
	}
	Writer = *pWriter;
	Code = xrtSshSignatureWrite(
		&Writer,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		(xbytesview){ arrSignature, sizeof(arrSignature) }
	);
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}

#endif
