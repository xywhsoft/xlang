#include <xrt/ssh_private_key.h>

#include <string.h>



#if defined(XSSH_FEATURE_PRIVATE_KEY)

/* 完整容器中的短输入都属于协议格式错误，而不是增量等待。 */
static xsshcode xsshPrivateKeyComplete(xsshcode Code)
{
	return Code == XSSH_NEED_MORE ? XSSH_ERROR_PROTOCOL : Code;
}



/* 把 SSH string 字节视图转换为名称文本。 */
static xstrview xsshPrivateKeyText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 比较名称与编译期文本。 */
static bool xsshPrivateKeyNameEqual(
	xstrview Name,
	const char* sExpected,
	size_t iExpectedSize
)
{
	return (Name.Size == iExpectedSize) &&
		(memcmp(Name.Data, sExpected, iExpectedSize) == 0);
}



/* 解析并预验证完整容器，未知 cipher/KDF 只作为元数据保留。 */
xsshcode xrtSshPrivateKeyRead(
	xbytesview Blob,
	xsshopensshprivatekey* pPrivateKey
)
{
	static const unsigned char arrMagic[] = XSSH_PRIVATE_KEY_MAGIC;
	xsshopensshprivatekey PrivateKey;
	xsshreader Reader;
	xbytesview Value;
	xbytesview PublicKeyBlob;
	xsshpublickey PublicKey;
	size_t iPublicStart;
	uint32 i;
	xsshcode Code;
	bool bCipherNone;
	bool bKdfNone;

	if ( !xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		!xrtSshReaderInit(&Reader, Blob) ||
		xrtMemRangesOverlap(
			Blob.Data,
			Blob.Size,
			pPrivateKey,
			sizeof(*pPrivateKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPrivateKeyComplete(xrtSshReadBytes(
		&Reader,
		sizeof(arrMagic),
		&Value
	));
	if ( (Code != XSSH_OK) ||
		(memcmp(Value.Data, arrMagic, sizeof(arrMagic)) != 0) ) {
		return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
	}
	Code = xsshPrivateKeyComplete(xrtSshReadString(&Reader, &Value));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PrivateKey.Cipher = xsshPrivateKeyText(Value);
	Code = xsshPrivateKeyComplete(xrtSshReadString(&Reader, &Value));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PrivateKey.Kdf = xsshPrivateKeyText(Value);
	Code = xsshPrivateKeyComplete(xrtSshReadString(
		&Reader,
		&PrivateKey.KdfOptions
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshPrivateKeyComplete(xrtSshReadU32(
		&Reader,
		&PrivateKey.KeyCount
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshNameValid(PrivateKey.Cipher) ||
		!xrtSshNameValid(PrivateKey.Kdf) || (PrivateKey.KeyCount == 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	bCipherNone = xsshPrivateKeyNameEqual(
		PrivateKey.Cipher,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	);
	bKdfNone = xsshPrivateKeyNameEqual(
		PrivateKey.Kdf,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	);
	if ( (bCipherNone != bKdfNone) ||
		(bCipherNone && (PrivateKey.KdfOptions.Size != 0u)) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iPublicStart = Reader.Position;
	for ( i = 0u; i < PrivateKey.KeyCount; ++i ) {
		Code = xsshPrivateKeyComplete(xrtSshReadString(
			&Reader,
			&PublicKeyBlob
		));
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Code = xsshPrivateKeyComplete(xrtSshPublicKeyRead(
			PublicKeyBlob,
			&PublicKey
		));
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	PrivateKey.PublicKeys.Data = Blob.Data + iPublicStart;
	PrivateKey.PublicKeys.Size = Reader.Position - iPublicStart;
	Code = xsshPrivateKeyComplete(xrtSshReadString(
		&Reader,
		&PrivateKey.PrivateList
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (PrivateKey.PrivateList.Size == 0u) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	PrivateKey.Blob = Blob;
	*pPrivateKey = PrivateKey;
	return XSSH_OK;
}



/* 未加密容器必须同时使用 cipher=none、kdf=none 和空 options。 */
xsshcode xrtSshPrivateKeyIsEncrypted(
	const xsshopensshprivatekey* pPrivateKey,
	bool* pEncrypted
)
{
	xsshopensshprivatekey PrivateKey;
	bool bEncrypted;
	xsshcode Code;

	if ( !xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		!xrtMemRangeValid(pEncrypted, sizeof(*pEncrypted)) ||
		!xrtMemRangeValid(pPrivateKey->Blob.Data, pPrivateKey->Blob.Size) ||
		xrtMemRangesOverlap(
			pPrivateKey,
			sizeof(*pPrivateKey),
			pEncrypted,
			sizeof(*pEncrypted)
		) || xrtMemRangesOverlap(
			pPrivateKey->Blob.Data,
			pPrivateKey->Blob.Size,
			pEncrypted,
			sizeof(*pEncrypted)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPrivateKeyRead(pPrivateKey->Blob, &PrivateKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	bEncrypted = !xsshPrivateKeyNameEqual(
		PrivateKey.Cipher,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	) || !xsshPrivateKeyNameEqual(
		PrivateKey.Kdf,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	) || (PrivateKey.KdfOptions.Size != 0u);
	*pEncrypted = bEncrypted;
	return XSSH_OK;
}



/* 从解析结果建立有界公钥 string 游标。 */
xsshcode xrtSshPrivateKeyPublicsInit(
	const xsshopensshprivatekey* pPrivateKey,
	xsshprivatekeypublics* pPublics
)
{
	xsshopensshprivatekey PrivateKey;
	xsshprivatekeypublics Publics;
	xsshcode Code;

	if ( !xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		!xrtMemRangeValid(pPublics, sizeof(*pPublics)) ||
		!xrtMemRangeValid(pPrivateKey->Blob.Data, pPrivateKey->Blob.Size) ||
		xrtMemRangesOverlap(
			pPrivateKey,
			sizeof(*pPrivateKey),
			pPublics,
			sizeof(*pPublics)
		) || xrtMemRangesOverlap(
			pPrivateKey->Blob.Data,
			pPrivateKey->Blob.Size,
			pPublics,
			sizeof(*pPublics)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPrivateKeyRead(pPrivateKey->Blob, &PrivateKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshReaderInit(&Publics.Reader, PrivateKey.PublicKeys) ) {
		return XSSH_ERROR_STATE;
	}
	Publics.Remaining = PrivateKey.KeyCount;
	*pPublics = Publics;
	return XSSH_OK;
}



/* 事务式返回下一把预验证公钥。 */
xsshcode xrtSshPrivateKeyPublicsNext(
	xsshprivatekeypublics* pPublics,
	xbytesview* pPublicKey
)
{
	xsshprivatekeypublics Publics;
	xbytesview PublicKeyBlob;
	xsshpublickey PublicKey;
	xsshcode Code;

	if ( !xrtMemRangeValid(pPublics, sizeof(*pPublics)) ||
		!xrtMemRangeValid(pPublicKey, sizeof(*pPublicKey)) ||
		!xrtMemRangeValid(
			pPublics->Reader.Source.Data,
			pPublics->Reader.Source.Size
		) || (pPublics->Reader.Position > pPublics->Reader.Source.Size) ||
		xrtMemRangesOverlap(
			pPublics,
			sizeof(*pPublics),
			pPublicKey,
			sizeof(*pPublicKey)
		) || xrtMemRangesOverlap(
			pPublics->Reader.Source.Data,
			pPublics->Reader.Source.Size,
			pPublicKey,
			sizeof(*pPublicKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pPublics->Remaining == 0u ) {
		return xrtSshReaderRemaining(&pPublics->Reader) == 0u ?
			XSSH_NEED_MORE : XSSH_ERROR_PROTOCOL;
	}
	Publics = *pPublics;
	Code = xsshPrivateKeyComplete(xrtSshReadString(
		&Publics.Reader,
		&PublicKeyBlob
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshPrivateKeyComplete(xrtSshPublicKeyRead(
		PublicKeyBlob,
		&PublicKey
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	--Publics.Remaining;
	if ( (Publics.Remaining == 0u) &&
		(xrtSshReaderRemaining(&Publics.Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPublics = Publics;
	*pPublicKey = PublicKeyBlob;
	return XSSH_OK;
}

#endif
