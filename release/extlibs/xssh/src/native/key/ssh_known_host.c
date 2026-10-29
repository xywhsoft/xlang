#include <xrt/ssh_known_host.h>

#include <xrt/codec.h>

#include "ssh_key_text_internal.h"
#include "ssh_known_host_internal.h"



#if defined(XSSH_FEATURE_KNOWN_HOST)

/* 比较 marker 与编译期名称。 */
static bool xsshKnownHostMarkerEqual(
	xstrview Marker,
	const char* sExpected,
	size_t iExpectedSize
)
{
	return (Marker.Size == iExpectedSize) &&
		(memcmp(Marker.Data, sExpected, iExpectedSize) == 0);
}



/* 校验明文 pattern-list 或单个 OpenSSH hashed host token。 */
static bool xsshKnownHostPatternsValid(
	xstrview Patterns,
	bool* pHashed
)
{
	size_t iStart;
	size_t i;

	if ( !xrtMemRangeValid(Patterns.Data, Patterns.Size) ||
		(Patterns.Size == 0u) || (pHashed == NULL) ) {
		return false;
	}
	if ( Patterns.Data[0] == '|' ) {
		for ( i = 0u; i < Patterns.Size; ++i ) {
			unsigned char iCharacter = (unsigned char)Patterns.Data[i];

			if ( (iCharacter <= 0x20u) || (iCharacter == 0x7fu) ||
				(iCharacter == (unsigned char)',') ||
				(iCharacter == (unsigned char)'!') ||
				(iCharacter == (unsigned char)'*') ||
				(iCharacter == (unsigned char)'?') ) {
				return false;
			}
		}
		*pHashed = true;
		return true;
	}
	iStart = 0u;
	for ( i = 0u; i <= Patterns.Size; ++i ) {
		if ( (i == Patterns.Size) || (Patterns.Data[i] == ',') ) {
			size_t iPattern = iStart;

			if ( iPattern == i ) {
				return false;
			}
			if ( Patterns.Data[iPattern] == '!' ) {
				++iPattern;
				if ( iPattern == i ) {
					return false;
				}
			}
			for ( ; iPattern < i; ++iPattern ) {
				unsigned char iCharacter =
					(unsigned char)Patterns.Data[iPattern];

				if ( (iCharacter <= 0x20u) || (iCharacter == 0x7fu) ||
					(iCharacter == (unsigned char)'|') ||
					(iCharacter == (unsigned char)'"') ||
					(iCharacter == (unsigned char)'\\') ) {
					return false;
				}
			}
			iStart = i + 1u;
		}
	}
	*pHashed = false;
	return true;
}



/* ASCII 主机比较大小写不敏感，其他字节保持原值。 */
static unsigned char xsshKnownHostFold(unsigned char iCharacter)
{
	if ( (iCharacter >= (unsigned char)'A') &&
		(iCharacter <= (unsigned char)'Z') ) {
		return (unsigned char)(iCharacter + ('a' - 'A'));
	}
	return iCharacter;
}



/* 用线性回退匹配 * 与 ?，不使用递归或临时主机字符串。 */
static bool xsshKnownHostPatternMatch(
	xstrview Pattern,
	const xsshknownhosttarget* pTarget
)
{
	size_t iPattern = 0u;
	size_t iTarget = 0u;
	size_t iStar = SIZE_MAX;
	size_t iRetry = 0u;

	while ( iTarget < pTarget->Size ) {
		if ( (iPattern < Pattern.Size) &&
			((Pattern.Data[iPattern] == '?') ||
			 (xsshKnownHostFold((unsigned char)Pattern.Data[iPattern]) ==
			  xsshKnownHostFold(xsshKnownHostTargetAt(
				  pTarget,
				  iTarget
			  )))) ) {
			++iPattern;
			++iTarget;
			continue;
		}
		if ( (iPattern < Pattern.Size) &&
			(Pattern.Data[iPattern] == '*') ) {
			iStar = iPattern++;
			iRetry = iTarget;
			continue;
		}
		if ( iStar != SIZE_MAX ) {
			iPattern = iStar + 1u;
			iTarget = ++iRetry;
			continue;
		}
		return false;
	}
	while ( (iPattern < Pattern.Size) &&
		(Pattern.Data[iPattern] == '*') ) {
		++iPattern;
	}
	return iPattern == Pattern.Size;
}



/* 解析 known_hosts 固定字段并保留未知 marker。 */
xsshcode xrtSshKnownHostLineRead(
	xstrview Line,
	xsshknownhostline* pKnownHost
)
{
	xsshknownhostline KnownHost;
	xstrview Token;
	size_t iStart;
	size_t iEnd;
	size_t iPosition;
	bool bPresent;
	xsshcode Code;

	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(Line.Data, Line.Size) ||
		xrtMemRangesOverlap(
			Line.Data,
			Line.Size,
			pKnownHost,
			sizeof(*pKnownHost)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyTextBounds(Line, &iStart, &iEnd);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iPosition = iStart;
	Code = xsshKeyTextToken(
		Line,
		iEnd,
		&iPosition,
		&Token,
		&bPresent
	);
	if ( (Code != XSSH_OK) || !bPresent ) {
		return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
	}
	KnownHost.Marker.Data = NULL;
	KnownHost.Marker.Size = 0u;
	KnownHost.MarkerKind = XSSH_KNOWN_HOST_MARKER_NONE;
	if ( Token.Data[0] == '@' ) {
		xstrview MarkerName = { Token.Data + 1u, Token.Size - 1u };

		if ( (Token.Size <= 1u) || !xrtSshNameValid(MarkerName) ) {
			return XSSH_ERROR_PROTOCOL;
		}
		KnownHost.Marker = Token;
		if ( xsshKnownHostMarkerEqual(
			Token,
			XSSH_KNOWN_HOST_CERT_AUTHORITY,
			sizeof(XSSH_KNOWN_HOST_CERT_AUTHORITY) - 1u
		) ) {
			KnownHost.MarkerKind = XSSH_KNOWN_HOST_MARKER_CERT_AUTHORITY;
		} else if ( xsshKnownHostMarkerEqual(
			Token,
			XSSH_KNOWN_HOST_REVOKED,
			sizeof(XSSH_KNOWN_HOST_REVOKED) - 1u
		) ) {
			KnownHost.MarkerKind = XSSH_KNOWN_HOST_MARKER_REVOKED;
		} else {
			KnownHost.MarkerKind = XSSH_KNOWN_HOST_MARKER_UNKNOWN;
		}
		Code = xsshKeyTextToken(
			Line,
			iEnd,
			&iPosition,
			&Token,
			&bPresent
		);
		if ( (Code != XSSH_OK) || !bPresent ) {
			return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
		}
	}
	KnownHost.Hosts = Token;
	if ( !xsshKnownHostPatternsValid(
		KnownHost.Hosts,
		&KnownHost.Hashed
	) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshKeyTextToken(
		Line,
		iEnd,
		&iPosition,
		&KnownHost.Algorithm,
		&bPresent
	);
	if ( (Code != XSSH_OK) || !bPresent ||
		!xrtSshNameValid(KnownHost.Algorithm) ) {
		return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
	}
	Code = xsshKeyTextToken(
		Line,
		iEnd,
		&iPosition,
		&KnownHost.Base64,
		&bPresent
	);
	if ( (Code != XSSH_OK) || !bPresent ||
		!xsshKeyTextBase64Shape(KnownHost.Base64) ||
		!xrtBase64Decode(
			KnownHost.Base64.Data,
			KnownHost.Base64.Size,
			NULL,
			0u,
			&KnownHost.BlobSize,
			NULL
		) || !xsshKeyTextBase64AlgorithmEqual(
			KnownHost.Base64,
			KnownHost.BlobSize,
			KnownHost.Algorithm
		) ) {
		return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
	}
	while ( (iPosition < iEnd) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iPosition]
	) ) {
		++iPosition;
	}
	if ( iPosition == iEnd ) {
		KnownHost.Comment.Data = NULL;
		KnownHost.Comment.Size = 0u;
	} else {
		KnownHost.Comment.Data = Line.Data + iPosition;
		KnownHost.Comment.Size = iEnd - iPosition;
	}
	*pKnownHost = KnownHost;
	return XSSH_OK;
}



/* 复用 public-key 文本的 Base64 与算法一致性检查。 */
xsshcode xrtSshKnownHostLineDecode(
	const xsshknownhostline* pKnownHost,
	void* pBlob,
	size_t iCapacity,
	xsshpublickey* pPublicKey
)
{
	xsshopensshkeyline KeyLine;

	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(pBlob, iCapacity) ||
		!xrtMemRangeValid(pPublicKey, sizeof(*pPublicKey)) ||
		xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pBlob,
			pKnownHost->BlobSize
		) || xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pPublicKey,
			sizeof(*pPublicKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	KeyLine.Options.Data = NULL;
	KeyLine.Options.Size = 0u;
	KeyLine.Algorithm = pKnownHost->Algorithm;
	KeyLine.Base64 = pKnownHost->Base64;
	KeyLine.Comment = pKnownHost->Comment;
	KeyLine.BlobSize = pKnownHost->BlobSize;
	return xrtSshPublicKeyLineDecode(
		&KeyLine,
		pBlob,
		iCapacity,
		pPublicKey
	);
}



/* 把 known_hosts 字段映射为公共 key-text 视图并复用零分配比较。 */
xsshcode xrtSshKnownHostLineKeyMatch(
	const xsshknownhostline* pKnownHost,
	xbytesview Blob,
	bool* pMatch
)
{
	xsshopensshkeyline KeyLine;

	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	KeyLine.Options.Data = NULL;
	KeyLine.Options.Size = 0u;
	KeyLine.Algorithm = pKnownHost->Algorithm;
	KeyLine.Base64 = pKnownHost->Base64;
	KeyLine.Comment = pKnownHost->Comment;
	KeyLine.BlobSize = pKnownHost->BlobSize;
	return xrtSshPublicKeyLineMatch(&KeyLine, Blob, pMatch);
}



/* 匹配整个 pattern-list，并让否定项覆盖正向项。 */
xsshcode xrtSshKnownHostPatternsMatch(
	xstrview Patterns,
	xstrview Host,
	uint32 iPort,
	xsshknownhostmatch* pMatch
)
{
	xsshknownhosttarget Target;
	xsshknownhostmatch Match = XSSH_KNOWN_HOST_NO_MATCH;
	bool bHashed;
	size_t iStart = 0u;
	size_t i;

	if ( !xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		!xsshKnownHostPatternsValid(Patterns, &bHashed) ||
		!xsshKnownHostTargetInit(&Target, Host, iPort) ||
		xrtMemRangesOverlap(
			Patterns.Data,
			Patterns.Size,
			pMatch,
			sizeof(*pMatch)
		) || xrtMemRangesOverlap(
			Host.Data,
			Host.Size,
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( bHashed ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	for ( i = 0u; i <= Patterns.Size; ++i ) {
		if ( (i == Patterns.Size) || (Patterns.Data[i] == ',') ) {
			xstrview Pattern;
			bool bNegated = Patterns.Data[iStart] == '!';

			Pattern.Data = Patterns.Data + iStart + (bNegated ? 1u : 0u);
			Pattern.Size = i - iStart - (bNegated ? 1u : 0u);
			if ( xsshKnownHostPatternMatch(Pattern, &Target) ) {
				if ( bNegated ) {
					Match = XSSH_KNOWN_HOST_NEGATED;
					break;
				}
				Match = XSSH_KNOWN_HOST_MATCH;
			}
			iStart = i + 1u;
		}
	}
	*pMatch = Match;
	return XSSH_OK;
}



/* 使用已解析的 Hosts 字段执行便利匹配。 */
xsshcode xrtSshKnownHostLineMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	xsshknownhostmatch* pMatch
)
{
	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pKnownHost->Hashed ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	return xrtSshKnownHostPatternsMatch(
		pKnownHost->Hosts,
		Host,
		iPort,
		pMatch
	);
}

#endif
