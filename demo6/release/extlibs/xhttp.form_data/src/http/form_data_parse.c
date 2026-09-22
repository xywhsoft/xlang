#include "../internal/xrt_form_data.h"



#if defined(XHTTP_FEATURE_FORM_DATA_PARSE)

/* 设置拥有型解析失败的 Multipart 位置和 FormData 域错误。 */
static xformdata* __xrtFormDataParseFail(
	xmultiparterrorinfo* pError,
	xmultiparterror Code,
	size_t iOffset,
	xerrkind Kind,
	cstr sMessage
)
{
	const xmultiparterrorinfo Error = { Code, iOffset };

	if ( xrtMemRangeValid(pError, sizeof(Error)) ) {
		memcpy(pError, &Error, sizeof(Error));
	}
	(void)__xrtFormDataFail(
		Kind,
		XFORM_DATA_ERROR_MULTIPART,
		"parse",
		sMessage
	);
	return NULL;
}



/* 发布支持未对齐存储的 Multipart 错误结果。 */
static void __xrtFormDataParseErrorSet(
	xmultiparterrorinfo* pError,
	const xmultiparterrorinfo* pValue
)
{
	if ( pError != NULL ) {
		memcpy(pError, pValue, sizeof(*pValue));
	}
}



/* 把 Multipart 细分错误映射为稳定的 FormData 错误类别。 */
static xerrkind __xrtFormDataMultipartErrorKind(
	xmultiparterror Code
)
{
	if ( Code == XMULTIPART_ERROR_ARGUMENT ) {
		return XERR_ARGUMENT;
	}
	if ( Code >= XMULTIPART_ERROR_PARTS_LIMIT ) {
		return XERR_RANGE;
	}
	return XERR_PROTOCOL;
}



/* 验证输入描述符和可选错误输出均为完整、互不覆盖的范围。 */
static bool __xrtFormDataParseCommonOutputValid(
	xbytesview Body,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
)
{
	if ( !xrtMemRangeValid(Body.Data, Body.Size) ||
		((pConfig != NULL) && !xrtMemRangeValid(
			pConfig, sizeof(*pConfig)
		)) || ((pLimits != NULL) && !xrtMemRangeValid(
			pLimits, sizeof(*pLimits)
		)) || ((pError != NULL) && !xrtMemRangeValid(
			pError, sizeof(*pError)
		)) ) {
		return false;
	}
	if ( pError == NULL ) {
		return true;
	}
	return !xrtMemRangesOverlap(
		pError, sizeof(*pError), Body.Data, Body.Size
	) && ((pConfig == NULL) || !xrtMemRangesOverlap(
		pError, sizeof(*pError),
		pConfig, sizeof(*pConfig)
	)) && ((pLimits == NULL) || !xrtMemRangesOverlap(
		pError, sizeof(*pError),
		pLimits, sizeof(*pLimits)
	));
}



/* 验证 multipart 解析错误输出不会覆盖 boundary。 */
static bool __xrtFormDataParseOutputValid(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
)
{
	return xrtMemRangeValid(pBoundary, sizeof(*pBoundary)) &&
		__xrtFormDataParseCommonOutputValid(
		Body, pConfig, pLimits, pError
	) && ((pError == NULL) || !xrtMemRangesOverlap(
		pError, sizeof(*pError),
		pBoundary, sizeof(*pBoundary)
	));
}



/* 验证 Content-Type 解析错误输出不会覆盖媒体类型。 */
static bool __xrtFormDataParseContentTypeOutputValid(
	xstrview ContentType,
	xbytesview Body,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
)
{
	return xrtMemRangeValid(ContentType.Data, ContentType.Size) &&
		__xrtFormDataParseCommonOutputValid(
		Body, pConfig, pLimits, pError
	) && ((pError == NULL) || !xrtMemRangesOverlap(
		pError, sizeof(*pError),
		ContentType.Data, ContentType.Size
	));
}



/* 从已经解析的 Header 块借用原始 Content-Type 字段值。 */
static xstrview __xrtFormDataPartContentType(
	const xmultipartpart* pPart
)
{
	xhttpfield Field;
	size_t iOffset = 0;

	while ( xrtHttpFieldNext(
		pPart->Headers, &iOffset, &Field
	) == XHTTP_NEXT_ITEM ) {
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Content-Type")
		) ) {
			return Field.Value;
		}
	}
	return (xstrview){ NULL, 0 };
}



/* 判断不需要变换正文的历史传输编码是否可以安全忽略。 */
static bool __xrtFormDataTransferEncodingValid(
	const xmultipartpart* pPart
)
{
	if ( (pPart->Flags &
		XMULTIPART_PART_TRANSFER_ENCODING) == 0 ) {
		return true;
	}
	return xrtHttpTokenEqual(
		pPart->TransferEncoding,
		XRT_STR_LITERAL("binary")
	) || xrtHttpTokenEqual(
		pPart->TransferEncoding,
		XRT_STR_LITERAL("8bit")
	) || xrtHttpTokenEqual(
		pPart->TransferEncoding,
		XRT_STR_LITERAL("7bit")
	);
}



/* 分配并解码一个 Part 名称。 */
static str __xrtFormDataPartName(
	const xmultipartpart* pPart,
	size_t* pSize,
	xmultiparterror* pProtocolError
)
{
	str sName;

	if ( !xrtMultipartPartNameWrite(
		pPart, NULL, 0, pSize
	) || (*pSize == SIZE_MAX) ) {
		*pProtocolError = XMULTIPART_ERROR_DISPOSITION;
		return NULL;
	}
	sName = (str)xrtMalloc(*pSize + 1u);
	if ( sName == NULL ) {
		return NULL;
	}
	if ( !xrtMultipartPartNameWrite(
		pPart, sName, *pSize, pSize
	) ) {
		*pProtocolError = XMULTIPART_ERROR_DISPOSITION;
		xrtFree(sName);
		return NULL;
	}
	sName[*pSize] = '\0';
	return sName;
}



/* 分配并解码一个可选文件名。 */
static str __xrtFormDataPartFilename(
	const xmultipartpart* pPart,
	size_t* pSize,
	xmultiparterror* pProtocolError
)
{
	str sFilename;

	if ( !xrtMultipartPartFileNameWrite(
		pPart, NULL, 0, pSize
	) || (*pSize == SIZE_MAX) ) {
		*pProtocolError = XMULTIPART_ERROR_DISPOSITION;
		return NULL;
	}
	sFilename = (str)xrtMalloc(*pSize + 1u);
	if ( sFilename == NULL ) {
		return NULL;
	}
	if ( !xrtMultipartPartFileNameWrite(
		pPart, sFilename, *pSize, pSize
	) ) {
		*pProtocolError = XMULTIPART_ERROR_DISPOSITION;
		xrtFree(sFilename);
		return NULL;
	}
	sFilename[*pSize] = '\0';
	return sFilename;
}



/* 把一个已校验 Part 复制并提交到拥有型容器。 */
static bool __xrtFormDataParsePart(
	xformdata* pForm,
	const xmultipartpart* pPart,
	xmultiparterror* pProtocolError
)
{
	str sName = NULL;
	str sFilename = NULL;
	xhttpbody* pBody = NULL;
	xstrview Name;
	xstrview Filename;
	xstrview ContentType;
	size_t iName;
	size_t iFilename = 0;
	size_t iQuoted;
	bool bHasFilename;
	bool bResult = false;

	*pProtocolError = XMULTIPART_ERROR_NONE;
	if ( !xrtMultipartFormPartValid(pPart) ) {
		*pProtocolError = XMULTIPART_ERROR_DISPOSITION;
		(void)__xrtFormDataFail(
			XERR_PROTOCOL,
			XFORM_DATA_ERROR_MULTIPART,
			"parse",
			"multipart Part is not a valid form-data field"
		);
		return false;
	}
	if ( !__xrtFormDataTransferEncodingValid(pPart) ) {
		*pProtocolError = XMULTIPART_ERROR_TRANSFER_ENCODING;
		(void)__xrtFormDataFail(
			XERR_UNSUPPORTED,
			XFORM_DATA_ERROR_MULTIPART,
			"parse",
			"FormData parser does not transform Content-Transfer-Encoding"
		);
		return false;
	}
	sName = __xrtFormDataPartName(
		pPart, &iName, pProtocolError
	);
	if ( sName == NULL ) {
		goto done;
	}
	bHasFilename = (pPart->Disposition.Flags & (
		XCONTENT_DISPOSITION_FILENAME |
		XCONTENT_DISPOSITION_FILENAME_EXT
	)) != 0;
	if ( bHasFilename ) {
		sFilename = __xrtFormDataPartFilename(
			pPart, &iFilename, pProtocolError
		);
		if ( sFilename == NULL ) {
			goto done;
		}
	}
	Name = (xstrview){ sName, iName };
	Filename = (xstrview){ sFilename, iFilename };
	if ( !xrtHttpQuotedWrite(Name, NULL, 0, &iQuoted) ||
		(bHasFilename && !xrtHttpQuotedWrite(
			Filename, NULL, 0, &iQuoted
		)) ) {
		*pProtocolError = XMULTIPART_ERROR_DISPOSITION;
		(void)__xrtFormDataFail(
			XERR_PROTOCOL,
			XFORM_DATA_ERROR_MULTIPART,
			"parse",
			"decoded FormData name or filename is not serializable"
		);
		goto done;
	}
	pBody = xrtHttpBodyCopy(pPart->Body);
	if ( pBody == NULL ) {
		goto done;
	}
	ContentType = (pPart->Flags &
		XMULTIPART_PART_CONTENT_TYPE) != 0 ?
		__xrtFormDataPartContentType(pPart) :
		(xstrview){ NULL, 0 };
	bResult = xrtFormDataAppendBody(
		pForm,
		Name,
		pBody,
		bHasFilename ? &Filename : NULL,
		ContentType
	);

done:
	xrtHttpBodyDestroy(pBody);
	xrtFree(sFilename);
	xrtFree(sName);
	return bResult;
}



/* 把完整 multipart/form-data 正文复制为拥有型容器。 */
XRT_API xformdata* xrtFormDataParse(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
)
{
	xmultipartboundary Boundary;
	xmultipartlimits Limits;
	xmultiparterrorinfo Error;
	xmultipartpart Part;
	xformdataconfig Config;
	const xformdataconfig* pResolvedConfig = NULL;
	xformdata* pForm;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iPartOffset;
	size_t iPartCount;
	xmultiparterror PartError;

	memset(&Error, 0, sizeof(Error));
	if ( !__xrtFormDataParseOutputValid(
		Body, pBoundary, pConfig, pLimits, pError
	) ) {
		return __xrtFormDataParseFail(
			NULL,
			XMULTIPART_ERROR_ARGUMENT,
			0,
			XERR_ARGUMENT,
			"FormData parse arguments or output are invalid"
		);
	}
	memcpy(&Boundary, pBoundary, sizeof(Boundary));
	if ( pConfig != NULL ) {
		memcpy(&Config, pConfig, sizeof(Config));
		pResolvedConfig = &Config;
	}
	xrtMultipartLimitsInit(&Limits);
	if ( pLimits != NULL ) {
		memcpy(&Limits, pLimits, sizeof(Limits));
	}
	__xrtFormDataParseErrorSet(pError, &Error);
	if ( !xrtMultipartValidateCount(
		Body,
		&Boundary,
		&Limits,
		XMULTIPART_VALIDATE_ALLOW_EMPTY,
		&iPartCount,
		&Error
	) ) {
		__xrtFormDataParseErrorSet(pError, &Error);
		(void)__xrtFormDataFail(
			__xrtFormDataMultipartErrorKind(Error.Code),
			XFORM_DATA_ERROR_MULTIPART,
			"parse",
			"multipart FormData validation failed"
		);
		return NULL;
	}
	if ( iPartCount == 0 ) {
		return xrtFormDataCreate(pResolvedConfig);
	}
	pForm = xrtFormDataCreate(pResolvedConfig);
	if ( pForm == NULL ) {
		return NULL;
	}
	iOffset = 0;
	for ( ;; ) {
		iPartOffset = iOffset;
		Next = xrtMultipartNext(
			Body, &Boundary, &iOffset, &Part, &Error
		);
		if ( Next == XHTTP_NEXT_END ) {
			return pForm;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			(void)__xrtFormDataFail(
				__xrtFormDataMultipartErrorKind(Error.Code),
				XFORM_DATA_ERROR_MULTIPART,
				"parse",
				"validated multipart FormData changed while parsing"
			);
			break;
		}
		if ( !__xrtFormDataParsePart(
			pForm, &Part, &PartError
		) ) {
			Error.Code = PartError;
			Error.Offset = PartError != XMULTIPART_ERROR_NONE ?
				iPartOffset : 0;
			break;
		}
	}
	xrtFormDataDestroy(pForm);
	__xrtFormDataParseErrorSet(pError, &Error);
	return NULL;
}



/* 从 Content-Type 读取 boundary，再解析完整正文。 */
XRT_API xformdata* xrtFormDataParseContentType(
	xstrview ContentType,
	xbytesview Body,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
)
{
	xmultipartboundary Boundary;

	if ( !__xrtFormDataParseContentTypeOutputValid(
		ContentType, Body, pConfig, pLimits, pError
	) ) {
		return __xrtFormDataParseFail(
			NULL,
			XMULTIPART_ERROR_ARGUMENT,
			0,
			XERR_ARGUMENT,
			"FormData Content-Type arguments or output are invalid"
		);
	}
	if ( !xrtMultipartBoundaryFromContentType(
		ContentType, &Boundary
	) ) {
		return __xrtFormDataParseFail(
			pError,
			XMULTIPART_ERROR_BOUNDARY,
			0,
			XERR_VALUE,
			"FormData Content-Type boundary is invalid"
		);
	}
	return xrtFormDataParse(
		Body,
		&Boundary,
		pConfig,
		pLimits,
		pError
	);
}

#endif
