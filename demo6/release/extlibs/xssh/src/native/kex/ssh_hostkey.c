#include <xrt/ssh_hostkey.h>

#include <string.h>



#if defined(XSSH_FEATURE_HOSTKEY)

/* 比较借用算法名与编译期文本。 */
static bool xsshHostKeyAlgorithmEqual(
	xstrview Algorithm,
	const char* sExpected,
	size_t iExpectedSize
)
{
	return (Algorithm.Size == iExpectedSize) &&
		(memcmp(Algorithm.Data, sExpected, iExpectedSize) == 0);
}



/* 读取并校验一个算法名 string。 */
static xsshcode xsshHostKeyReadAlgorithm(
	xsshreader* pReader,
	xstrview* pAlgorithm
)
{
	xbytesview Value;
	xstrview Algorithm;
	xsshcode Code;

	Code = xrtSshReadString(pReader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Algorithm.Data = (const char*)Value.Data;
	Algorithm.Size = Value.Size;
	if ( !xrtSshNameValid(Algorithm) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pAlgorithm = Algorithm;
	return XSSH_OK;
}



/* 验证 writer、容量和所有输入，随后返回未提交的输出区域。 */
static xsshcode xsshHostKeyWritePrepare(
	xsshwriter* pWriter,
	size_t iWriteSize,
	xbytesview First,
	xbytesview Second,
	xsshwriter* pCopy
)
{
	xsshwriter Writer;
	xbytesview arrInputs[2];
	xsshcode Code;

	if ( (pWriter == NULL) || (pCopy == NULL) ||
		((First.Data == NULL) && (First.Size != 0u)) ||
		((Second.Data == NULL) && (Second.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrInputs[0] = First;
	arrInputs[1] = Second;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iWriteSize,
		arrInputs,
		2u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	*pCopy = Writer;
	return XSSH_OK;
}



/* 读取公钥的通用前缀，并把剩余算法参数保持为原始视图。 */
xsshcode xrtSshPublicKeyRead(
	xbytesview Blob,
	xsshpublickey* pPublicKey
)
{
	xsshreader Reader;
	xsshpublickey PublicKey;
	xsshcode Code;

	if ( (pPublicKey == NULL) ||
		!xrtSshReaderInit(&Reader, Blob) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostKeyReadAlgorithm(&Reader, &PublicKey.Algorithm);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBytes(
		&Reader,
		xrtSshReaderRemaining(&Reader),
		&PublicKey.Parameters
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pPublicKey = PublicKey;
	return XSSH_OK;
}



/* 严格读取固定为两个 SSH string 的签名 blob。 */
xsshcode xrtSshSignatureRead(
	xbytesview Blob,
	xsshsignature* pSignature
)
{
	xsshreader Reader;
	xsshsignature Signature;
	xsshcode Code;

	if ( (pSignature == NULL) ||
		!xrtSshReaderInit(&Reader, Blob) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostKeyReadAlgorithm(&Reader, &Signature.Algorithm);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Signature.Signature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSignature = Signature;
	return XSSH_OK;
}



/* 预计算容量后一次提交通用签名 blob。 */
xsshcode xrtSshSignatureWrite(
	xsshwriter* pWriter,
	xstrview Algorithm,
	xbytesview Signature
)
{
	xsshwriter Writer;
	xbytesview AlgorithmBytes;
	size_t iWriteSize;
	xsshcode Code;

	if ( !xrtSshNameValid(Algorithm) ||
		((Signature.Data == NULL) && (Signature.Size != 0u)) ||
		(Signature.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (Algorithm.Size > (SIZE_MAX - 8u)) ||
		(Signature.Size > (SIZE_MAX - 8u - Algorithm.Size)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iWriteSize = 8u + Algorithm.Size + Signature.Size;
	AlgorithmBytes.Data = (const unsigned char*)Algorithm.Data;
	AlgorithmBytes.Size = Algorithm.Size;
	Code = xsshHostKeyWritePrepare(
		pWriter,
		iWriteSize,
		AlgorithmBytes,
		Signature,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, AlgorithmBytes) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Signature) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 在通用公钥前缀后严格解释 Ed25519 的单一 32 字节字段。 */
xsshcode xrtSshEd25519PublicKeyRead(
	xbytesview Blob,
	xbytesview* pPublicKey
)
{
	xsshpublickey PublicKey;
	xsshreader Reader;
	xbytesview Key;
	xsshcode Code;

	if ( pPublicKey == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPublicKeyRead(Blob, &PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshHostKeyAlgorithmEqual(
		PublicKey.Algorithm,
		XSSH_HOSTKEY_ED25519,
		sizeof(XSSH_HOSTKEY_ED25519) - 1u
	) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( !xrtSshReaderInit(&Reader, PublicKey.Parameters) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshReadString(&Reader, &Key);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (Key.Size != XSSH_ED25519_PUBLIC_SIZE) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPublicKey = Key;
	return XSSH_OK;
}



/* 写入算法名与固定长度 Ed25519 公钥。 */
xsshcode xrtSshEd25519PublicKeyWrite(
	xsshwriter* pWriter,
	xbytesview PublicKey
)
{
	xsshwriter Writer;
	xbytesview Algorithm = XRT_BYTES_LITERAL(XSSH_HOSTKEY_ED25519);
	size_t iWriteSize = 8u + Algorithm.Size + XSSH_ED25519_PUBLIC_SIZE;
	xsshcode Code;

	if ( PublicKey.Size != XSSH_ED25519_PUBLIC_SIZE ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostKeyWritePrepare(
		pWriter,
		iWriteSize,
		Algorithm,
		PublicKey,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Algorithm) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, PublicKey) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 在通用签名结构上收紧算法和固定签名长度。 */
xsshcode xrtSshEd25519SignatureRead(
	xbytesview Blob,
	xbytesview* pSignature
)
{
	xsshsignature Signature;
	xsshcode Code;

	if ( pSignature == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSignatureRead(Blob, &Signature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshHostKeyAlgorithmEqual(
		Signature.Algorithm,
		XSSH_HOSTKEY_ED25519,
		sizeof(XSSH_HOSTKEY_ED25519) - 1u
	) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( Signature.Signature.Size != XSSH_ED25519_SIGNATURE_SIZE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSignature = Signature.Signature;
	return XSSH_OK;
}



/* 复用通用签名 writer 构建 Ed25519 blob。 */
xsshcode xrtSshEd25519SignatureWrite(
	xsshwriter* pWriter,
	xbytesview Signature
)
{
	if ( Signature.Size != XSSH_ED25519_SIGNATURE_SIZE ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshSignatureWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		Signature
	);
}

#endif
