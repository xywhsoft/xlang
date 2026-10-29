#include <string.h>

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_WIRE)

/* 校验只借用内存的 reader 状态。 */
static bool xsshReaderValid(const xsshreader* pReader)
{
	return (pReader != NULL) &&
		xrtMemRangeValid(pReader->Source.Data, pReader->Source.Size) &&
		(pReader->Position <= pReader->Source.Size);
}



/* 校验只借用内存的 writer 状态。 */
static bool xsshWriterValid(const xsshwriter* pWriter)
{
	return (pWriter != NULL) &&
		xrtMemRangeValid(pWriter->Data, pWriter->Capacity) &&
		(pWriter->Size <= pWriter->Capacity);
}



/* 将 uint32 写入已经验证过的四字节目标。 */
static void xsshStoreU32(bytes pOutput, uint32 iValue)
{
	pOutput[0] = (uint8)((iValue >> 24u) & 0xffu);
	pOutput[1] = (uint8)((iValue >> 16u) & 0xffu);
	pOutput[2] = (uint8)((iValue >> 8u) & 0xffu);
	pOutput[3] = (uint8)(iValue & 0xffu);
}



/* 校验单个 SSH 名称，不接受分隔符或非 ASCII 字节。 */
bool xrtSshNameValid(xstrview Name)
{
	size_t i;

	if ( (Name.Size == 0u) || !xrtMemRangeValid(Name.Data, Name.Size) ) {
		return false;
	}
	for ( i = 0u; i < Name.Size; ++i ) {
		uint8 iByte = (uint8)Name.Data[i];

		if ( (iByte < 33u) || (iByte > 126u) || (iByte == (uint8)',') ) {
			return false;
		}
	}
	return true;
}



/* 校验允许为空且不含控制字符的 ASCII language tag。 */
bool xrtSshLanguageValid(xstrview Language)
{
	size_t i;

	if ( !xrtMemRangeValid(Language.Data, Language.Size) ) {
		return false;
	}
	for ( i = 0u; i < Language.Size; ++i ) {
		uint8 iByte = (uint8)Language.Data[i];

		if ( (iByte < 33u) || (iByte > 126u) ) {
			return false;
		}
	}
	return true;
}



/* 比较一个 name-list 项与独立名称。 */
static bool xsshNameEqual(
	xstrview List,
	size_t iStart,
	size_t iEnd,
	xstrview Name
)
{
	return ((iEnd - iStart) == Name.Size) &&
		(memcmp(List.Data + iStart, Name.Data, Name.Size) == 0);
}



/* 校验非负 mpint 的规范二进制补码形式。 */
static bool xsshPositiveMpintCanonical(xbytesview Value)
{
	if ( (Value.Data == NULL) && (Value.Size != 0u) ) {
		return false;
	}
	if ( Value.Size == 0u ) {
		return true;
	}
	if ( (Value.Data[0] & 0x80u) != 0u ) {
		return false;
	}
	if ( Value.Data[0] == 0u ) {
		return (Value.Size > 1u) && ((Value.Data[1] & 0x80u) != 0u);
	}
	return true;
}



/* 校验任意符号 mpint 的最短二进制补码形式。 */
static bool xsshSignedMpintCanonical(xbytesview Value)
{
	if ( (Value.Data == NULL) && (Value.Size != 0u) ) {
		return false;
	}
	if ( Value.Size == 0u ) {
		return true;
	}
	if ( (Value.Size == 1u) && (Value.Data[0] == 0u) ) {
		return false;
	}
	if ( (Value.Size > 1u) && (Value.Data[0] == 0u) &&
		((Value.Data[1] & 0x80u) == 0u) ) {
		return false;
	}
	if ( (Value.Size > 1u) && (Value.Data[0] == 0xffu) &&
		((Value.Data[1] & 0x80u) != 0u) ) {
		return false;
	}
	return true;
}



/* 初始化借用输入的 SSH reader。 */
bool xrtSshReaderInit(xsshreader* pReader, xbytesview Source)
{
	if ( (pReader == NULL) ||
		!xrtMemRangeValid(Source.Data, Source.Size) ) {
		return false;
	}
	pReader->Source = Source;
	pReader->Position = 0u;
	return true;
}



/* 初始化借用输出缓冲的 SSH writer。 */
bool xrtSshWriterInit(xsshwriter* pWriter, void* pData, size_t iCapacity)
{
	if ( (pWriter == NULL) || !xrtMemRangeValid(pData, iCapacity) ) {
		return false;
	}
	pWriter->Data = (bytes)pData;
	pWriter->Capacity = iCapacity;
	pWriter->Size = 0u;
	return true;
}



/* 返回 reader 中仍可读取的字节数。 */
size_t xrtSshReaderRemaining(const xsshreader* pReader)
{
	if ( !xsshReaderValid(pReader) ) {
		return 0u;
	}
	return pReader->Source.Size - pReader->Position;
}



/* 返回 writer 中仍可写入的字节数。 */
size_t xrtSshWriterRemaining(const xsshwriter* pWriter)
{
	if ( !xsshWriterValid(pWriter) ) {
		return 0u;
	}
	return pWriter->Capacity - pWriter->Size;
}



/* 校验 writer 状态并确认本次写入有完整空间。 */
xsshcode xrtSshWriterReserve(const xsshwriter* pWriter, size_t iSize)
{
	if ( !xsshWriterValid(pWriter) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iSize > xrtSshWriterRemaining(pWriter) ) {
		return XSSH_ERROR_SPACE;
	}
	return XSSH_OK;
}



/* 校验完整输出范围与输入视图及描述数组不重叠。 */
xsshcode xrtSshWriterReserveInputs(
	const xsshwriter* pWriter,
	size_t iSize,
	const xbytesview* pInputs,
	size_t iInputCount
)
{
	const void* pOutput;
	size_t iInputBytes;
	size_t i;
	xsshcode Code;

	if ( (pInputs == NULL) && (iInputCount != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iInputCount > (SIZE_MAX / sizeof(*pInputs)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserve(pWriter, iSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pOutput = pWriter->Data == NULL ? NULL :
		(const void*)(pWriter->Data + pWriter->Size);
	iInputBytes = iInputCount * sizeof(*pInputs);
	if ( xrtMemRangesOverlap(
		pOutput,
		iSize,
		pWriter,
		sizeof(*pWriter)
	) || xrtMemRangesOverlap(
		pOutput,
		iSize,
		pInputs,
		iInputBytes
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < iInputCount; ++i ) {
		if ( !xrtMemRangeValid(pInputs[i].Data, pInputs[i].Size) ||
			xrtMemRangesOverlap(
				pOutput,
				iSize,
				pInputs[i].Data,
				pInputs[i].Size
			) ) {
			return XSSH_ERROR_ARGUMENT;
		}
	}
	return XSSH_OK;
}



/* 读取一个 SSH byte。 */
xsshcode xrtSshReadByte(xsshreader* pReader, uint8* pValue)
{
	uint8 iValue;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < 1u ) {
		return XSSH_NEED_MORE;
	}
	iValue = pReader->Source.Data[pReader->Position];
	pReader->Position++;
	*pValue = iValue;
	return XSSH_OK;
}



/* 读取零或非零编码的 SSH boolean。 */
xsshcode xrtSshReadBool(xsshreader* pReader, bool* pValue)
{
	uint8 iValue;
	xsshcode Code;

	if ( pValue == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iValue);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pValue = iValue != 0u;
	return XSSH_OK;
}



/* 读取一个网络字节序 uint32。 */
xsshcode xrtSshReadU32(xsshreader* pReader, uint32* pValue)
{
	cbytes pData;
	uint32 iValue;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < 4u ) {
		return XSSH_NEED_MORE;
	}
	pData = pReader->Source.Data + pReader->Position;
	iValue = ((uint32)pData[0] << 24u) |
		((uint32)pData[1] << 16u) |
		((uint32)pData[2] << 8u) |
		(uint32)pData[3];
	pReader->Position += 4u;
	*pValue = iValue;
	return XSSH_OK;
}



/* 读取一个网络字节序 uint64。 */
xsshcode xrtSshReadU64(xsshreader* pReader, uint64* pValue)
{
	cbytes pData;
	uint64 iValue;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < 8u ) {
		return XSSH_NEED_MORE;
	}
	pData = pReader->Source.Data + pReader->Position;
	iValue = ((uint64)pData[0] << 56u) |
		((uint64)pData[1] << 48u) |
		((uint64)pData[2] << 40u) |
		((uint64)pData[3] << 32u) |
		((uint64)pData[4] << 24u) |
		((uint64)pData[5] << 16u) |
		((uint64)pData[6] << 8u) |
		(uint64)pData[7];
	pReader->Position += 8u;
	*pValue = iValue;
	return XSSH_OK;
}



/* 读取指定数量的原始字节。 */
xsshcode xrtSshReadBytes(
	xsshreader* pReader,
	size_t iSize,
	xbytesview* pValue
)
{
	xbytesview Value;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < iSize ) {
		return XSSH_NEED_MORE;
	}
	Value.Data = pReader->Source.Data == NULL ?
		NULL : pReader->Source.Data + pReader->Position;
	Value.Size = iSize;
	pReader->Position += iSize;
	*pValue = Value;
	return XSSH_OK;
}



/* 读取 uint32 长度前缀的 SSH string。 */
xsshcode xrtSshReadString(xsshreader* pReader, xbytesview* pValue)
{
	cbytes pData;
	size_t iSize;
	xbytesview Value;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < 4u ) {
		return XSSH_NEED_MORE;
	}
	pData = pReader->Source.Data + pReader->Position;
	iSize = ((size_t)pData[0] << 24u) |
		((size_t)pData[1] << 16u) |
		((size_t)pData[2] << 8u) |
		(size_t)pData[3];
	if ( iSize > (xrtSshReaderRemaining(pReader) - 4u) ) {
		return XSSH_NEED_MORE;
	}
	Value.Data = pData + 4u;
	Value.Size = iSize;
	pReader->Position += 4u + iSize;
	*pValue = Value;
	return XSSH_OK;
}



/* 写入一个 SSH byte。 */
xsshcode xrtSshWriteByte(xsshwriter* pWriter, uint8 iValue)
{
	xsshcode Code = xrtSshWriterReserve(pWriter, 1u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	pWriter->Data[pWriter->Size] = iValue;
	pWriter->Size++;
	return XSSH_OK;
}



/* 写入规范的 SSH boolean。 */
xsshcode xrtSshWriteBool(xsshwriter* pWriter, bool bValue)
{
	return xrtSshWriteByte(pWriter, bValue ? 1u : 0u);
}



/* 以网络字节序写入 uint32。 */
xsshcode xrtSshWriteU32(xsshwriter* pWriter, uint32 iValue)
{
	xsshcode Code = xrtSshWriterReserve(pWriter, 4u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	xsshStoreU32(pWriter->Data + pWriter->Size, iValue);
	pWriter->Size += 4u;
	return XSSH_OK;
}



/* 以网络字节序写入 uint64。 */
xsshcode xrtSshWriteU64(xsshwriter* pWriter, uint64 iValue)
{
	bytes pOutput;
	xsshcode Code = xrtSshWriterReserve(pWriter, 8u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	pOutput = pWriter->Data + pWriter->Size;
	pOutput[0] = (uint8)((iValue >> 56u) & 0xffu);
	pOutput[1] = (uint8)((iValue >> 48u) & 0xffu);
	pOutput[2] = (uint8)((iValue >> 40u) & 0xffu);
	pOutput[3] = (uint8)((iValue >> 32u) & 0xffu);
	pOutput[4] = (uint8)((iValue >> 24u) & 0xffu);
	pOutput[5] = (uint8)((iValue >> 16u) & 0xffu);
	pOutput[6] = (uint8)((iValue >> 8u) & 0xffu);
	pOutput[7] = (uint8)(iValue & 0xffu);
	pWriter->Size += 8u;
	return XSSH_OK;
}



/* 不添加长度前缀，直接写入原始字节。 */
xsshcode xrtSshWriteBytes(xsshwriter* pWriter, xbytesview Value)
{
	xsshcode Code;

	if ( (Value.Data == NULL) && (Value.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserve(pWriter, Value.Size);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Value.Size != 0u ) {
		memcpy(pWriter->Data + pWriter->Size, Value.Data, Value.Size);
	}
	pWriter->Size += Value.Size;
	return XSSH_OK;
}



/* 写入 uint32 长度前缀的 SSH string。 */
xsshcode xrtSshWriteString(xsshwriter* pWriter, xbytesview Value)
{
	size_t iStart;
	xsshcode Code;

	if ( (Value.Data == NULL) && (Value.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (Value.Size > UINT32_MAX) || (Value.Size > (SIZE_MAX - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserve(pWriter, 4u + Value.Size);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iStart = pWriter->Size;
	xsshStoreU32(pWriter->Data + iStart, (uint32)Value.Size);
	if ( Value.Size != 0u ) {
		memcpy(pWriter->Data + iStart + 4u, Value.Data, Value.Size);
	}
	pWriter->Size = iStart + 4u + Value.Size;
	return XSSH_OK;
}



/* 校验并写入 SSH name-list string。 */
xsshcode xrtSshWriteNameList(xsshwriter* pWriter, xstrview List)
{
	xbytesview Value;

	if ( !xrtSshNameListValid(List) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Value.Data = (cbytes)List.Data;
	Value.Size = List.Size;
	return xrtSshWriteString(pWriter, Value);
}



/* 校验 SSH name-list。 */
bool xrtSshNameListValid(xstrview List)
{
	size_t iStart = 0u;
	size_t i;

	if ( (List.Data == NULL) && (List.Size != 0u) ) {
		return false;
	}
	if ( List.Size == 0u ) {
		return true;
	}
	for ( i = 0u; i <= List.Size; ++i ) {
		if ( (i == List.Size) || (List.Data[i] == ',') ) {
			xstrview Name;

			Name.Data = List.Data + iStart;
			Name.Size = i - iStart;
			if ( !xrtSshNameValid(Name) ) {
				return false;
			}
			iStart = i + 1u;
		}
	}
	return true;
}



/* 判断 SSH name-list 是否包含完整名称。 */
bool xrtSshNameListContains(xstrview List, xstrview Name)
{
	size_t iStart = 0u;
	size_t i;

	if ( !xrtSshNameListValid(List) || !xrtSshNameValid(Name) ) {
		return false;
	}
	for ( i = 0u; i <= List.Size; ++i ) {
		if ( (i == List.Size) || (List.Data[i] == ',') ) {
			if ( xsshNameEqual(List, iStart, i, Name) ) {
				return true;
			}
			iStart = i + 1u;
		}
	}
	return false;
}



/* 判断 SSH name-list 是否存在重复项。 */
bool xrtSshNameListHasDuplicate(xstrview List)
{
	size_t iStart = 0u;
	size_t i;

	if ( !xrtSshNameListValid(List) ) {
		return false;
	}
	for ( i = 0u; i <= List.Size; ++i ) {
		if ( (i == List.Size) || (List.Data[i] == ',') ) {
			xstrview Name;
			size_t jStart = i + 1u;
			size_t j;

			Name.Data = List.Data + iStart;
			Name.Size = i - iStart;
			for ( j = i + 1u; j <= List.Size; ++j ) {
				if ( (j == List.Size) || (List.Data[j] == ',') ) {
					if ( xsshNameEqual(List, jStart, j, Name) ) {
						return true;
					}
					jStart = j + 1u;
				}
			}
			iStart = i + 1u;
		}
	}
	return false;
}



/* 按首选顺序选择双方共同名称。 */
xsshcode xrtSshNameListFirstMatch(
	xstrview Preferred,
	xstrview Available,
	xstrview* pMatch
)
{
	size_t iStart = 0u;
	size_t i;

	if ( (pMatch == NULL) || !xrtSshNameListValid(Preferred) ||
		!xrtSshNameListValid(Available) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i <= Preferred.Size; ++i ) {
		if ( (i == Preferred.Size) || (Preferred.Data[i] == ',') ) {
			xstrview Name;

			Name.Data = Preferred.Data + iStart;
			Name.Size = i - iStart;
			if ( xrtSshNameListContains(Available, Name) ) {
				*pMatch = Name;
				return XSSH_OK;
			}
			iStart = i + 1u;
		}
	}
	return XSSH_ERROR_UNSUPPORTED;
}



/* 读取规范的非负 SSH mpint。 */
xsshcode xrtSshReadMpint(xsshreader* pReader, xbytesview* pValue)
{
	size_t iPosition;
	xbytesview Value;
	xsshcode Code;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iPosition = pReader->Position;
	Code = xrtSshReadString(pReader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshPositiveMpintCanonical(Value) ) {
		pReader->Position = iPosition;
		return XSSH_ERROR_PROTOCOL;
	}
	*pValue = Value;
	return XSSH_OK;
}



/* 将大端 magnitude 规范化为非负 SSH mpint。 */
xsshcode xrtSshWriteMpint(xsshwriter* pWriter, xbytesview Magnitude)
{
	size_t iOffset = 0u;
	size_t iEncoded;
	size_t iStart;
	bool bPrefix;
	xsshcode Code;

	if ( (Magnitude.Data == NULL) && (Magnitude.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	while ( (iOffset < Magnitude.Size) && (Magnitude.Data[iOffset] == 0u) ) {
		iOffset++;
	}
	if ( iOffset == Magnitude.Size ) {
		return xrtSshWriteU32(pWriter, 0u);
	}
	Magnitude.Data += iOffset;
	Magnitude.Size -= iOffset;
	bPrefix = (Magnitude.Data[0] & 0x80u) != 0u;
	if ( (Magnitude.Size > UINT32_MAX) ||
		(bPrefix && (Magnitude.Size == UINT32_MAX)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iEncoded = Magnitude.Size + (bPrefix ? 1u : 0u);
	if ( iEncoded > (SIZE_MAX - 4u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserve(pWriter, 4u + iEncoded);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iStart = pWriter->Size;
	xsshStoreU32(pWriter->Data + iStart, (uint32)iEncoded);
	if ( bPrefix ) {
		pWriter->Data[iStart + 4u] = 0u;
	}
	memcpy(
		pWriter->Data + iStart + 4u + (bPrefix ? 1u : 0u),
		Magnitude.Data,
		Magnitude.Size
	);
	pWriter->Size = iStart + 4u + iEncoded;
	return XSSH_OK;
}



/* 读取规范的任意符号 SSH mpint。 */
xsshcode xrtSshReadSignedMpint(
	xsshreader* pReader,
	xbytesview* pValue,
	bool* pNegative
)
{
	size_t iPosition;
	xbytesview Value;
	bool bNegative;
	xsshcode Code;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ||
		(pNegative == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iPosition = pReader->Position;
	Code = xrtSshReadString(pReader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshSignedMpintCanonical(Value) ) {
		pReader->Position = iPosition;
		return XSSH_ERROR_PROTOCOL;
	}
	bNegative = (Value.Size != 0u) && ((Value.Data[0] & 0x80u) != 0u);
	*pValue = Value;
	*pNegative = bNegative;
	return XSSH_OK;
}



/* 将大端 magnitude 与符号规范化为 SSH mpint。 */
xsshcode xrtSshWriteSignedMpint(
	xsshwriter* pWriter,
	xbytesview Magnitude,
	bool bNegative
)
{
	size_t iOffset = 0u;
	size_t iEncoded;
	size_t iStart;
	size_t i;
	uint8 iCarryToTop = 1u;
	uint8 iTop;
	uint16 iCarry = 1u;
	bool bPrefix;
	bytes pOutput;
	xsshcode Code;

	if ( (Magnitude.Data == NULL) && (Magnitude.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !bNegative ) {
		return xrtSshWriteMpint(pWriter, Magnitude);
	}
	while ( (iOffset < Magnitude.Size) && (Magnitude.Data[iOffset] == 0u) ) {
		iOffset++;
	}
	if ( iOffset == Magnitude.Size ) {
		return xrtSshWriteU32(pWriter, 0u);
	}
	Magnitude.Data += iOffset;
	Magnitude.Size -= iOffset;
	for ( i = 1u; i < Magnitude.Size; ++i ) {
		if ( Magnitude.Data[i] != 0u ) {
			iCarryToTop = 0u;
			break;
		}
	}
	iTop = (uint8)((uint8)~Magnitude.Data[0] + iCarryToTop);
	bPrefix = (iTop & 0x80u) == 0u;
	if ( (Magnitude.Size > UINT32_MAX) ||
		(bPrefix && (Magnitude.Size == UINT32_MAX)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iEncoded = Magnitude.Size + (bPrefix ? 1u : 0u);
	if ( iEncoded > (SIZE_MAX - 4u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserve(pWriter, 4u + iEncoded);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iStart = pWriter->Size;
	pOutput = pWriter->Data + iStart + 4u;
	if ( bPrefix ) {
		*pOutput++ = 0xffu;
	}
	for ( i = Magnitude.Size; i > 0u; --i ) {
		uint16 iValue = (uint16)((uint8)~Magnitude.Data[i - 1u]) + iCarry;

		pOutput[i - 1u] = (uint8)(iValue & 0xffu);
		iCarry = (uint16)(iValue >> 8u);
	}
	xsshStoreU32(pWriter->Data + iStart, (uint32)iEncoded);
	pWriter->Size = iStart + 4u + iEncoded;
	return XSSH_OK;
}



/* 校验 protoversion 后的非空 softwareversion。 */
static bool xsshSoftwareVersionValid(
	const char* pData,
	size_t iSize,
	size_t iOffset
)
{
	size_t iStart = iOffset;

	while ( (iOffset < iSize) && (pData[iOffset] != ' ') ) {
		uint8 iByte = (uint8)pData[iOffset];

		if ( (iByte < 0x21u) || (iByte > 0x7eu) ||
			(iByte == (uint8)'-') ) {
			return false;
		}
		iOffset++;
	}
	return iOffset > iStart;
}



/* 从增量输入中查找并校验 SSH identification 行。 */
xsshcode xrtSshBannerRead(
	xstrview Data,
	xstrview* pBanner,
	size_t* pConsumed
)
{
	size_t iLineStart = 0u;
	size_t i;

	if ( (pBanner == NULL) || (pConsumed == NULL) ||
		((Data.Data == NULL) && (Data.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < Data.Size; ++i ) {
		uint8 iByte = (uint8)Data.Data[i];

		if ( iByte == (uint8)'\r' ) {
			if ( (i + 1u) == Data.Size ) {
				break;
			}
			if ( Data.Data[i + 1u] != '\n' ) {
				return XSSH_ERROR_PROTOCOL;
			}
			continue;
		}
		if ( iByte == (uint8)'\n' ) {
			size_t iLineEnd = i;
			size_t iLineSize;
			size_t iPrefixSize;

			if ( (i + 1u - iLineStart) > XSSH_IDENTIFICATION_MAX ) {
				return XSSH_ERROR_OVERFLOW;
			}
			if ( (iLineEnd > iLineStart) &&
				(Data.Data[iLineEnd - 1u] == '\r') ) {
				iLineEnd--;
			}
			iLineSize = iLineEnd - iLineStart;
			if ( (iLineSize >= 4u) &&
				(memcmp(Data.Data + iLineStart, "SSH-", 4u) == 0) ) {
				bool bVersion2 = (iLineSize > 8u) &&
					(memcmp(Data.Data + iLineStart, "SSH-2.0-", 8u) == 0);
				bool bVersion199 = (iLineSize > 9u) &&
					(memcmp(Data.Data + iLineStart, "SSH-1.99-", 9u) == 0);

				if ( !bVersion2 && !bVersion199 ) {
					return (iLineSize > 8u) ?
						XSSH_ERROR_UNSUPPORTED : XSSH_ERROR_PROTOCOL;
				}
				iPrefixSize = bVersion2 ? 8u : 9u;
				if ( !xsshSoftwareVersionValid(
					Data.Data + iLineStart,
					iLineSize,
					iPrefixSize
				) ) {
					return XSSH_ERROR_PROTOCOL;
				}
				pBanner->Data = Data.Data + iLineStart;
				pBanner->Size = iLineSize;
				*pConsumed = i + 1u;
				return XSSH_OK;
			}
			iLineStart = i + 1u;
			continue;
		}
		if ( (iByte < 0x20u) || (iByte == 0x7fu) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( (Data.Size - iLineStart) >= XSSH_IDENTIFICATION_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	return XSSH_NEED_MORE;
}



/* 本端只发送 SSH-2.0，并严格校验 softwareversion 与整行长度。 */
xsshcode xrtSshBannerWrite(
	xsshwriter* pWriter,
	xstrview Banner
)
{
	xbytesview Input;
	xsshwriter Writer;
	size_t i;
	xsshcode Code;

	if ( !xrtMemRangeValid(Banner.Data, Banner.Size) ||
		(Banner.Size < 9u) ||
		(Banner.Size > (XSSH_IDENTIFICATION_MAX - 2u)) ||
		(memcmp(Banner.Data, "SSH-2.0-", 8u) != 0) ||
		!xsshSoftwareVersionValid(Banner.Data, Banner.Size, 8u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 8u; i < Banner.Size; ++i ) {
		uint8 iByte = (uint8)Banner.Data[i];

		if ( (iByte < 0x20u) || (iByte > 0x7eu) ) {
			return XSSH_ERROR_ARGUMENT;
		}
	}
	Input.Data = (const unsigned char*)Banner.Data;
	Input.Size = Banner.Size;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		Banner.Size + 2u,
		&Input,
		1u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteBytes(&Writer, Input) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, XRT_BYTES_LITERAL("\r\n")) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}

#endif
