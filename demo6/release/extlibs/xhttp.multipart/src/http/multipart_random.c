#include "../internal/xrt_http.h"




#if defined(XHTTP_FEATURE_MULTIPART_RANDOM)

/* 使用系统安全随机源生成 128 位 form-data boundary。 */
XRT_API bool xrtMultipartBoundaryRandom(
	xmultipartboundary* pBoundary
)
{
	static const char Hex[] = "0123456789abcdef";
	static const char Prefix[] = "----xrt-form-";
	uint8 Random[16];
	xmultipartboundary Boundary;
	size_t i;

	if ( !xrtMemRangeValid(pBoundary, sizeof(Boundary)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtSecureRandom(Random, sizeof(Random)) ) {
		memset(Random, 0, sizeof(Random));
		return false;
	}
	memset(&Boundary, 0, sizeof(Boundary));
	memcpy(Boundary.Data, Prefix, sizeof(Prefix) - 1u);
	Boundary.Size = sizeof(Prefix) - 1u;
	for ( i = 0; i < sizeof(Random); i++ ) {
		Boundary.Data[Boundary.Size++] = Hex[Random[i] >> 4u];
		Boundary.Data[Boundary.Size++] = Hex[Random[i] & 0x0Fu];
	}
	Boundary.Data[Boundary.Size] = '\0';
	memset(Random, 0, sizeof(Random));
	memcpy(pBoundary, &Boundary, sizeof(Boundary));
	return true;
}

#endif
