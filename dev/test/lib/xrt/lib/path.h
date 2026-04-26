


// 通过路径获取文件名 + 扩展名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetNameExt(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				xCore.iRet = 0;
				return xCore.sNull;
			} else {
				xCore.iRet = iSize - i - 1;
				return xrtCopyStr(&sPath[i + 1], iSize - i - 1);
			}
		}
	}
	xCore.iRet = iSize;
	return xrtCopyStr(sPath, iSize);
}



// 通过路径获取文件名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetName(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	uint iPointPos = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( sPath[i] == L'.' ) {
			iPointPos = iSize - i;
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				xCore.iRet = 0;
				return xCore.sNull;
			} else {
				xCore.iRet = iSize - i - iPointPos - 1;
				return xrtCopyStr(&sPath[i + 1], iSize - i - iPointPos - 1);
			}
		}
	}
	xCore.iRet = iSize;
	return xrtCopyStr(sPath, iSize);
}



// 通过路径获取扩展名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetExt(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( sPath[i] == L'.' ) {
			xCore.iRet = iSize - i - 1;
			return xrtCopyStr(&sPath[i + 1], iSize - i - 1);
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			xCore.iRet = 0;
			return xCore.sNull;
		}
	}
	xCore.iRet = 0;
	return xCore.sNull;
}



// 通过路径获取文件夹（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetDir(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				xCore.iRet = iSize - 1;
				return xrtCopyStr(sPath, iSize - 1);
			} else {
				xCore.iRet = i;
				return xrtCopyStr(sPath, i);
			}
		}
	}
	xCore.iRet = 0;
	return xCore.sNull;
}



// 判断是否为绝对路径（Linux 系统以 / 开头为绝对路径，Windows系统含 : 为绝对路径）
XXAPI bool xrtPathIsAbs(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return FALSE; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { return FALSE; }
	if ( sPath[0] == '/' ) {
		return TRUE;
	}
	for ( int i = 0; i < iSize; i++ ) {
		if ( sPath[i] == ':' ) {
			return TRUE;
		}
	}
	return FALSE;
}



// 获取随机不存在的路径（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathRandom(str sHead, size_t iHeadSize, str sFoot, size_t iFootSize, size_t iLen)
{
	if ( sHead ) {
		if ( iHeadSize == 0 ) { iHeadSize = strlen(sHead); }
		if ( iHeadSize == 0 ) { sHead = NULL; }
	} else {
		iHeadSize = 0;
	}
	if ( sFoot ) {
		if ( iFootSize == 0 ) { iFootSize = strlen(sFoot); }
		if ( iFootSize == 0 ) { sFoot = NULL; }
	} else {
		iFootSize = 0;
	}
	int iSize = iHeadSize + iFootSize + iLen;
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	str sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return xCore.sNull;
	}
	if ( sHead ) {
		memcpy(sRet, sHead, iHeadSize);
	}
	for ( int i = 0; i < iLen; i++ ) {
		int idx = xrtRandRange(0, 61);		// 这里写 61，可以忽略 - 和 _ 字符
		sRet[iHeadSize + i] = RandStringDefaultTemplate[idx];
	}
	if ( sFoot ) {
		memcpy(&sRet[iHeadSize + iLen], sFoot, iFootSize);
	}
	sRet[iSize] = 0;
	xCore.iRet = iSize;
	return sRet;
}



// 拼接路径（ 需要使用 xrtFree 释放内存 ）
XXAPI str xrtPathJoin(uint iCount, ...)
{
	if ( iCount == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	str sRet = xrtMalloc(4096);
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return xCore.sNull;
	}
	va_list args;
	va_start(args, iCount);
	size_t iPos = 0;
	for ( int i = 0; i < iCount; i++ ) {
		str sPath = va_arg(args, str);
		if ( sPath == NULL ) { continue; }
		size_t iSize = strlen(sPath);
		if ( iSize == 0 ) { continue; }
		if ( (iPos + iSize) > 4094 ) { xrtFree(sRet); xCore.iRet = 0; return xCore.sNull; }
		memcpy(&sRet[iPos], sPath, iSize);
		iPos += iSize;
		if ( i < (iCount - 1) ) {
			if ( (sRet[iPos-1] != L'\\') && (sRet[iSize-1] != L'/') ) {
				#if defined(_WIN32) || defined(_WIN64)
					sRet[iPos] = L'\\';
				#else
					sRet[iPos] = L'/';
				#endif
				iPos++;
			}
		}
	}
	va_end(args);
	if ( iPos > 4000 ) {
		sRet[iPos] = 0;
		xCore.iRet = iPos;
		return sRet;
	} else {
		str sRetTrim = xrtMalloc(iPos + 1);
		if ( sRetTrim == NULL ) {
			sRet[iPos] = 0;
			xCore.iRet = iPos;
			return sRet;
		} else {
			memcpy(sRetTrim, sRet, iPos);
			xrtFree(sRet);
			sRetTrim[iPos] = 0;
			xCore.iRet = iPos;
			return sRetTrim;
		}
	}
}


