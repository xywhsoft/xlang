#include <xrt/ssh_key_text.h>

#include <xrt/codec.h>

#include "ssh_key_text_internal.h"



#if defined(XSSH_FEATURE_KEY_TEXT)

/* 判断结构输出是否覆盖任意借用字段。 */
static bool xsshKeyTextOutputOverlaps(
	const xsshopensshkeyline* pKeyLine,
	const void* pOutput,
	size_t iOutputSize
)
{
	return xrtMemRangesOverlap(
		pKeyLine->Options.Data,
		pKeyLine->Options.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pKeyLine->Algorithm.Data,
		pKeyLine->Algorithm.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pKeyLine->Base64.Data,
		pKeyLine->Base64.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pKeyLine->Comment.Data,
		pKeyLine->Comment.Size,
		pOutput,
		iOutputSize
	);
}



/* 校验调用方保存的公钥文本视图，并重新确认 Base64 容量。 */
static xsshcode xsshKeyTextLineValidate(
	const xsshopensshkeyline* pKeyLine
)
{
	size_t iBlobSize;

	if ( !xrtMemRangeValid(pKeyLine, sizeof(*pKeyLine)) ||
		!xrtMemRangeValid(pKeyLine->Options.Data, pKeyLine->Options.Size) ||
		!xrtMemRangeValid(pKeyLine->Algorithm.Data, pKeyLine->Algorithm.Size) ||
		!xrtMemRangeValid(pKeyLine->Base64.Data, pKeyLine->Base64.Size) ||
		!xrtMemRangeValid(pKeyLine->Comment.Data, pKeyLine->Comment.Size) ||
		!xrtSshNameValid(pKeyLine->Algorithm) ||
		!xsshKeyTextBase64Shape(pKeyLine->Base64) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtBase64Decode(
		pKeyLine->Base64.Data,
		pKeyLine->Base64.Size,
		NULL,
		0u,
		&iBlobSize,
		NULL
	) || (iBlobSize != pKeyLine->BlobSize) ||
		!xsshKeyTextBase64AlgorithmEqual(
			pKeyLine->Base64,
			iBlobSize,
			pKeyLine->Algorithm
		) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return XSSH_OK;
}



/* 解析字段边界，并用 XRT Base64 查询精确 blob 容量。 */
xsshcode xrtSshPublicKeyLineRead(
	xstrview Line,
	xsshopensshkeyline* pKeyLine
)
{
	xsshopensshkeyline KeyLine;
	xstrview Algorithm;
	xstrview Base64;
	size_t iStart;
	size_t iEnd;
	size_t iPosition;
	size_t iLook;
	size_t iOptionsEnd;
	bool bPresent;
	xsshcode Code;

	if ( !xrtMemRangeValid(pKeyLine, sizeof(*pKeyLine)) ||
		!xrtMemRangeValid(Line.Data, Line.Size) ||
		xrtMemRangesOverlap(
			Line.Data,
			Line.Size,
			pKeyLine,
			sizeof(*pKeyLine)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyTextBounds(Line, &iStart, &iEnd);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iPosition = iStart;
	while ( iPosition < iEnd ) {
		Code = xsshKeyTextToken(
			Line,
			iEnd,
			&iPosition,
			&Algorithm,
			&bPresent
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( !bPresent ) {
			break;
		}
		iLook = iPosition;
		Code = xsshKeyTextToken(
			Line,
			iEnd,
			&iLook,
			&Base64,
			&bPresent
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( xrtSshNameValid(Algorithm) && bPresent &&
			xsshKeyTextBase64Shape(Base64) ) {
			if ( !xrtBase64Decode(
				Base64.Data,
				Base64.Size,
				NULL,
				0u,
				&KeyLine.BlobSize,
				NULL
			) || !xsshKeyTextBase64AlgorithmEqual(
				Base64,
				KeyLine.BlobSize,
				Algorithm
			) ) {
				return XSSH_ERROR_PROTOCOL;
			}
			break;
		}
	}
	if ( (iPosition >= iEnd) || !bPresent ||
		!xrtSshNameValid(Algorithm) ||
		!xsshKeyTextBase64Shape(Base64) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	iOptionsEnd = (size_t)(Algorithm.Data - Line.Data);
	while ( (iOptionsEnd > iStart) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iOptionsEnd - 1u]
	) ) {
		--iOptionsEnd;
	}
	if ( iOptionsEnd == iStart ) {
		KeyLine.Options.Data = NULL;
		KeyLine.Options.Size = 0u;
	} else {
		KeyLine.Options.Data = Line.Data + iStart;
		KeyLine.Options.Size = iOptionsEnd - iStart;
	}
	KeyLine.Algorithm = Algorithm;
	KeyLine.Base64 = Base64;
	while ( (iLook < iEnd) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iLook]
	) ) {
		++iLook;
	}
	if ( iLook == iEnd ) {
		KeyLine.Comment.Data = NULL;
		KeyLine.Comment.Size = 0u;
	} else {
		KeyLine.Comment.Data = Line.Data + iLook;
		KeyLine.Comment.Size = iEnd - iLook;
	}
	*pKeyLine = KeyLine;
	return XSSH_OK;
}



/* 解码完整 blob 后，才发布算法无关公钥视图。 */
xsshcode xrtSshPublicKeyLineDecode(
	const xsshopensshkeyline* pKeyLine,
	void* pBlob,
	size_t iCapacity,
	xsshpublickey* pPublicKey
)
{
	xsshpublickey PublicKey;
	size_t iBlobSize;
	xsshcode Code;

	if ( !xrtMemRangeValid(pKeyLine, sizeof(*pKeyLine)) ||
		!xrtMemRangeValid(pPublicKey, sizeof(*pPublicKey)) ||
		!xrtMemRangeValid(pBlob, iCapacity) ||
		xsshKeyTextOutputOverlaps(
			pKeyLine,
			pPublicKey,
			sizeof(*pPublicKey)
		) || xrtMemRangesOverlap(
			pKeyLine,
			sizeof(*pKeyLine),
			pPublicKey,
			sizeof(*pPublicKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyTextLineValidate(pKeyLine);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iBlobSize = pKeyLine->BlobSize;
	if ( iCapacity < iBlobSize ) {
		return XSSH_ERROR_SPACE;
	}
	if ( xsshKeyTextOutputOverlaps(pKeyLine, pBlob, iBlobSize) ||
		xrtMemRangesOverlap(
			pKeyLine,
			sizeof(*pKeyLine),
			pBlob,
			iBlobSize
		) || xrtMemRangesOverlap(
			pBlob,
			iBlobSize,
			pPublicKey,
			sizeof(*pPublicKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtBase64Decode(
		pKeyLine->Base64.Data,
		pKeyLine->Base64.Size,
		pBlob,
		iCapacity,
		&iBlobSize,
		NULL
	) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshPublicKeyRead(
		(xbytesview){ (const unsigned char*)pBlob, iBlobSize },
		&PublicKey
	);
	if ( Code == XSSH_NEED_MORE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshKeyTextEqual(PublicKey.Algorithm, pKeyLine->Algorithm) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPublicKey = PublicKey;
	return XSSH_OK;
}



/* 比较规范文本与原始 blob，避免 known_hosts 扫描期间反复分配工作区。 */
xsshcode xrtSshPublicKeyLineMatch(
	const xsshopensshkeyline* pKeyLine,
	xbytesview Blob,
	bool* pMatch
)
{
	xsshpublickey PublicKey;
	xsshcode Code;
	bool bMatch;

	if ( !xrtMemRangeValid(pKeyLine, sizeof(*pKeyLine)) ||
		!xrtMemRangeValid(Blob.Data, Blob.Size) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			pKeyLine,
			sizeof(*pKeyLine),
			pMatch,
			sizeof(*pMatch)
		) || xrtMemRangesOverlap(
			Blob.Data,
			Blob.Size,
			pMatch,
			sizeof(*pMatch)
		) || xsshKeyTextOutputOverlaps(
			pKeyLine,
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyTextLineValidate(pKeyLine);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshPublicKeyRead(Blob, &PublicKey);
	if ( Code == XSSH_NEED_MORE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	bMatch = (Blob.Size == pKeyLine->BlobSize) &&
		xsshKeyTextEqual(PublicKey.Algorithm, pKeyLine->Algorithm) &&
		xsshKeyTextBase64Equal(pKeyLine->Base64, Blob);
	*pMatch = bMatch;
	return XSSH_OK;
}

#endif
