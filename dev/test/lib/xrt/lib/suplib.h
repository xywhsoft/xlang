


// 内存查找
#if defined(_WIN32) || defined(_WIN64)
	XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize)
	{
		if ( (iMemSize == 0) || (iSubSize == 0) ) {
			return NULL;
		}
		if ( iMemSize  < iSubSize ) {
			return NULL;
		}
		char* pMemC = pMem;
		char* pSubC = pSub;
		size_t iRange = iMemSize - iSubSize;
		for ( int i = 0; i < iRange; i++ ) {
			char* pPos = &pMemC[i];
			int bOK = TRUE;
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( pPos[j] != pSubC[j] ) {
					bOK = FALSE;
					break;
				}
			}
			if ( bOK ) {
				return pPos;
			}
		}
		return NULL;
	}
#endif



// 获取字符串长度 ( 补充 utf16 和 utf32 支持 )
XXAPI size_t u16len(u16str sText)
{
	size_t iSize = 0;
	while ( sText[iSize] != 0 ) {
		iSize++;
	}
	return iSize;
}
XXAPI size_t u32len(u32str sText)
{
	size_t iSize = 0;
	while ( sText[iSize] != 0 ) {
		iSize++;
	}
	return iSize;
}


