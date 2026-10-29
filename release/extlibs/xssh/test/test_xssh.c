#define XSSH_MODULE_SSH_FINGERPRINT
#define XSSH_MODULE_SSH_KNOWN_HOST_HASH

#include <xssh.h>

#include <stdio.h>
#include <string.h>



/* 验证条件，不满足时输出稳定诊断。 */
static bool testRequire(bool bValue, const char* sMessage)
{
	if ( !bValue ) {
		fprintf(stderr, "xssh C test failed: %s\n", sMessage);
	}
	return bValue;
}



/* 验证 SSH wire 写入、读取和失败不推进。 */
static bool testWire(void)
{
	unsigned char arrData[64];
	xsshwriter Writer;
	xsshreader Reader;
	xbytesview Text;
	size_t iPosition;
	uint8 iByte;
	uint32 iValue;

	if ( !testRequire(
		xrtSshWriterInit(&Writer, arrData, sizeof(arrData)) &&
		(xrtSshWriteByte(&Writer, 20u) == XSSH_OK) &&
		(xrtSshWriteU32(&Writer, UINT32_C(0x01020304)) == XSSH_OK) &&
		(xrtSshWriteString(&Writer, XRT_BYTES_LITERAL("ssh-userauth")) == XSSH_OK),
		"wire write"
	) || !testRequire(
		xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrData, Writer.Size }
		) && (xrtSshReadByte(&Reader, &iByte) == XSSH_OK) &&
		(iByte == 20u) && (xrtSshReadU32(&Reader, &iValue) == XSSH_OK) &&
		(iValue == UINT32_C(0x01020304)) &&
		(xrtSshReadString(&Reader, &Text) == XSSH_OK) &&
		(Text.Size == 12u) && (memcmp(Text.Data, "ssh-userauth", 12u) == 0),
		"wire read"
	) ) {
		return false;
	}
	iPosition = Reader.Position;
	return testRequire(
		(xrtSshReadByte(&Reader, &iByte) == XSSH_NEED_MORE) &&
		(Reader.Position == iPosition),
		"wire failure advanced reader"
	);
}



/* 验证 OpenSSH SHA-256 指纹固定向量。 */
static bool testFingerprint(void)
{
	static const char sExpected[] =
		"SHA256:47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU";
	char sOutput[64];
	size_t iOutputSize;

	return testRequire(
		(xrtSshHostKeyFingerprintSha256(
			(xbytesview){ NULL, 0u },
			sOutput,
			sizeof(sOutput),
			&iOutputSize
		) == XSSH_OK) && (iOutputSize == (sizeof(sExpected) - 1u)) &&
		(strcmp(sOutput, sExpected) == 0),
		"fingerprint vector"
	);
}



/* 验证 OpenSSH hashed-host HMAC-SHA1 固定向量。 */
static bool testKnownHostHash(void)
{
	static const char sExpected[] =
		"|1|ICEiIyQlJicoKSorLC0uLzAxMjM=|xBQE+zEva59fZY5Xy11PuF9jNd8=";
	unsigned char arrSalt[XSSH_KNOWN_HOST_HASH_SIZE];
	char sOutput[80];
	size_t iOutputSize;
	bool bMatch;
	size_t i;

	for ( i = 0u; i < sizeof(arrSalt); i++ ) {
		arrSalt[i] = (unsigned char)(0x20u + i);
	}
	return testRequire(
		(xrtSshKnownHostHashWrite(
			XRT_STR_LITERAL("hashed.example"),
			22u,
			(xbytesview){ arrSalt, sizeof(arrSalt) },
			sOutput,
			sizeof(sOutput),
			&iOutputSize
		) == XSSH_OK) && (iOutputSize == (sizeof(sExpected) - 1u)) &&
		(strcmp(sOutput, sExpected) == 0) &&
		(xrtSshKnownHostHashMatch(
			XRT_STR_LITERAL(sExpected),
			XRT_STR_LITERAL("HASHED.EXAMPLE"),
			22u,
			&bMatch
		) == XSSH_OK) && bMatch,
		"known-host hash vector"
	);
}



/* 运行 xssh 公共 C API 冒烟测试。 */
int main(void)
{
	if ( !testWire() || !testFingerprint() || !testKnownHostHash() ) {
		return 1;
	}
	printf("xssh C package ok\n");
	return 0;
}
