


// 辅助宏：同时更新 TLS 和全局的 iRet (线程安全)
#define XRT_SET_IRET(val) do { \
	xrtThreadLocal* _tls = xrtGetTLS(); \
	if (_tls) { _tls->iRet = (val); } \
	xCore.iRet = (val); \
} while(0)

// 创建字符串副本（ 需使用 xrtFree 释放 ）(线程安全)
XXAPI str xrtCopyStr(str sText, size_t iSize)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return xCore.sNull; }
	str sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) {
		XRT_SET_IRET(0);
		return xCore.sNull;
	}
	memcpy(sRet, sText, iSize);
	sRet[iSize] = 0;
	XRT_SET_IRET(iSize);
	return sRet;
}
XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return (u16str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u16len(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return (u16str)xCore.sNull; }
	u16str sRet = xrtMalloc((iSize + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) {
		XRT_SET_IRET(0);
		return (u16str)xCore.sNull;
	}
	memcpy(sRet, sText, iSize * sizeof(unsigned short));
	sRet[iSize] = 0;
	XRT_SET_IRET(iSize);
	return sRet;
}
XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return (u32str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u32len(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return (u32str)xCore.sNull; }
	u32str sRet = xrtMalloc((iSize + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) {
		XRT_SET_IRET(0);
		return (u32str)xCore.sNull;
	}
	memcpy(sRet, sText, iSize * sizeof(unsigned int));
	sRet[iSize] = 0;
	XRT_SET_IRET(iSize);
	return sRet;
}
XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize)
{
	if ( pMem == NULL ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return xCore.sNull; }
	ptr pRet = xrtMalloc(iSize);
	if ( pRet == NULL ) {
		XRT_SET_IRET(0);
		return xCore.sNull;
	}
	memcpy(pRet, pMem, iSize);
	XRT_SET_IRET(iSize);
	return pRet;
}



// 比较字符串
XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase)
{
	if ( iSize > 0 ) {
		if ( bCase ) {
			#if defined(_WIN32) || defined(_WIN64)
				// windows 方案
				return strnicmp(s1, s2, iSize);
			#else
				// 其他平台方案
				return strncasecmp(s1, s2, iSize);
			#endif
		} else {
			return strncmp(s1, s2, iSize);
		}
	} else {
		if ( bCase ) {
			#if defined(_WIN32) || defined(_WIN64)
				// windows 方案
				return stricmp(s1, s2);
			#else
				// 其他平台方案
				return strcasecmp(s1, s2);
			#endif
		} else {
			return strcmp(s1, s2);
		}
	}
}



// 字符串转为小写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet;
	if ( bSrcRevise ) { sRet = sText; } else { sRet = xrtCopyStr(sText, iSize); }
	for ( int i = 0; i < iSize; i++ ) {
		if ( sRet[i] & 0x80 ) {
			// 跳过 OEM 编码字符
			i++;
		} else {
			sRet[i] = tolower(sRet[i]);
		}
	}
	return sRet;
}



// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet;
	if ( bSrcRevise != FALSE ) { sRet = sText; } else { sRet = xrtCopyStr(sText, iSize); }
	for ( int i = 0; i < iSize; i++ ) {
		if ( sRet[i] & 0x80 ) {
			// 跳过 OEM 编码字符
			i++;
		} else {
			sRet[i] = toupper(sRet[i]);
		}
	}
	return sRet;
}



// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）(线程安全)
XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return NULL; }
	if ( sSubText == NULL ) { XRT_SET_IRET(0); return NULL; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { XRT_SET_IRET(0); return NULL; }
	str sSub;
	if ( bCase ) {
		str sText1 = xrtLCase(sText, 0, FALSE);
		str sText2 = xrtLCase(sSubText, 0, FALSE);
		sSub = memmem(sText1, iSize, sText2, iSubSize);
		if ( sSub ) {
			sSub = &sText[sSub - sText1];
		}
		free(sText1);
		free(sText2);
	} else {
		sSub = memmem(sText, iSize, sSubText, iSubSize);
	}
	if ( sSub ) {
		XRT_SET_IRET((sSub - sText) + 1);
		return sSub;
	} else {
		XRT_SET_IRET(0);
		return NULL;
	}
}
XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	xrtFindStr(sText, iSize, sSubText, iSubSize, bCase);
	xrtThreadLocal* _tls = xrtGetTLS();
	return _tls ? _tls->iRet : xCore.iRet;
}



// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize)
{
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { return NULL; }
	for ( int i = 0; i < iSize; i++ ) {
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					return &sText[i];
				}
			}
		} else if ( (sText[i] & 0b11000000) == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					return &sText[i];
				}
			}
			i++;
		} else if ( (sText[i] & 0b11100000) == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					return &sText[i];
				}
			}
			i += 2;
		} else if ( (sText[i] & 0b11110000) == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					return &sText[i];
				}
			}
			i += 3;
		} else if ( (sText[i] & 0b11111000) == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					return &sText[i];
				}
			}
			i += 4;
		} else if ( (sText[i] & 0b11111100) == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					return &sText[i];
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
	}
	return NULL;
}



// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = " \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = " \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCount++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( (sText[i] & 0b11000000) == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCount += 2;
					bBreak = FALSE;
					break;
				}
			}
			i++;
		} else if ( (sText[i] & 0b11100000) == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCount += 3;
					bBreak = FALSE;
					break;
				}
			}
			i += 2;
		} else if ( (sText[i] & 0b11110000) == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCount += 4;
					bBreak = FALSE;
					break;
				}
			}
			i += 3;
		} else if ( (sText[i] & 0b11111000) == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCount += 5;
					bBreak = FALSE;
					break;
				}
			}
			i += 4;
		} else if ( (sText[i] & 0b11111100) == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCount += 6;
					bBreak = FALSE;
					break;
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
		if ( bBreak ) {
			break;
		}
	}
	XRT_SET_IRET(iSize - iCount);
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCount], iSize - iCount);
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(&sText[iCount], iSize - iCount);
	}
}
XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = " \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = " \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCount++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( (sText[i] & 0b11000000) == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCount += 2;
					bBreak = FALSE;
					break;
				}
			}
			i++;
		} else if ( (sText[i] & 0b11100000) == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCount += 3;
					bBreak = FALSE;
					break;
				}
			}
			i += 2;
		} else if ( (sText[i] & 0b11110000) == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCount += 4;
					bBreak = FALSE;
					break;
				}
			}
			i += 3;
		} else if ( (sText[i] & 0b11111000) == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCount += 5;
					bBreak = FALSE;
					break;
				}
			}
			i += 4;
		} else if ( (sText[i] & 0b11111100) == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCount += 6;
					bBreak = FALSE;
					break;
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
		if ( bBreak ) {
			break;
		}
	}
	XRT_SET_IRET(iSize - iCount);
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(sText, iSize - iCount);
	}
}
XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = " \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = " \t\r\n"; iSubSize = 4; }
	int iCountL = 0;
	int iCountR = 0;
	// 裁剪左侧
	for ( int i = 0; i < iSize; i++ ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCountL++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( (sText[i] & 0b11000000) == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCountL += 2;
					bBreak = FALSE;
					break;
				}
			}
			i++;
		} else if ( (sText[i] & 0b11100000) == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCountL += 3;
					bBreak = FALSE;
					break;
				}
			}
			i += 2;
		} else if ( (sText[i] & 0b11110000) == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCountL += 4;
					bBreak = FALSE;
					break;
				}
			}
			i += 3;
		} else if ( (sText[i] & 0b11111000) == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCountL += 5;
					bBreak = FALSE;
					break;
				}
			}
			i += 4;
		} else if ( (sText[i] & 0b11111100) == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCountL += 6;
					bBreak = FALSE;
					break;
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
		if ( bBreak ) {
			break;
		}
	}
	// 全部裁剪需要特殊处理
	if ( iCountL >= iSize ) {
		XRT_SET_IRET(0);
		return xCore.sNull;
	}
	// 裁剪右侧
	for ( int i = iSize - 1; i >= 0; i-- ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCountR++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( (sText[i] & 0b11000000) == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCountR += 2;
					bBreak = FALSE;
					break;
				}
			}
			i++;
		} else if ( (sText[i] & 0b11100000) == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCountR += 3;
					bBreak = FALSE;
					break;
				}
			}
			i += 2;
		} else if ( (sText[i] & 0b11110000) == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCountR += 4;
					bBreak = FALSE;
					break;
				}
			}
			i += 3;
		} else if ( (sText[i] & 0b11111000) == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCountR += 5;
					bBreak = FALSE;
					break;
				}
			}
			i += 4;
		} else if ( (sText[i] & 0b11111100) == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCountR += 6;
					bBreak = FALSE;
					break;
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
		if ( bBreak ) {
			break;
		}
	}
	int iCount = iCountL + iCountR;
	XRT_SET_IRET(iSize - iCount);
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCountL], iSize - iCount);
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(&sText[iCountL], iSize - iCount);
	}
}



// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtFilterStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( sSubText == NULL ) { if ( bSrcRevise ) { XRT_SET_IRET(iSize); return sText; } else { XRT_SET_IRET(iSize); return xrtCopyStr(sText, iSize); } }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { if ( bSrcRevise ) { XRT_SET_IRET(iSize); return sText; } else { XRT_SET_IRET(iSize); return xrtCopyStr(sText, iSize); } }
	// 不改动源数据时，直接创建副本
	if ( bSrcRevise == FALSE ) {
		sText = xrtCopyStr(sText, iSize);
	}
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			int bCopy = TRUE;
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCount++;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
			}
		} else if ( (sText[i] & 0b11000000) == 0b11000000 ) {
			// 双字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCount += 2;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
			}
			i++;
		} else if ( (sText[i] & 0b11100000) == 0b11100000 ) {
			// 三字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCount += 3;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
				sText[i - iCount + 2] = sText[i + 2];
			}
			i += 2;
		} else if ( (sText[i] & 0b11110000) == 0b11110000 ) {
			// 四字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCount += 4;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
				sText[i - iCount + 2] = sText[i + 2];
				sText[i - iCount + 3] = sText[i + 3];
			}
			i += 3;
		} else if ( (sText[i] & 0b11111000) == 0b11111000 ) {
			// 五字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCount += 5;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
				sText[i - iCount + 2] = sText[i + 2];
				sText[i - iCount + 3] = sText[i + 3];
				sText[i - iCount + 4] = sText[i + 4];
			}
			i += 4;
		} else if ( (sText[i] & 0b11111100) == 0b11111100 ) {
			// 六字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCount += 6;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
				sText[i - iCount + 2] = sText[i + 2];
				sText[i - iCount + 3] = sText[i + 3];
				sText[i - iCount + 4] = sText[i + 4];
				sText[i - iCount + 5] = sText[i + 5];
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
	}
	sText[iSize - iCount] = 0;
	XRT_SET_IRET(iCount);
	return sText;
}



// 字符串格式化（ 需使用 xrtFree 释放 ）
XXAPI str xrtFormat(str sFormat, ...)
{
	if ( sFormat == NULL ) { XRT_SET_IRET(0); return xCore.sNull; }
	va_list ip;
	va_start(ip, sFormat);
	int iSize = vsnprintf(NULL, 0, sFormat, ip);
	va_end(ip);
	if ( iSize > 0 ) {
		str sRet = xrtMalloc(iSize + 1);
		if ( sRet == NULL ) {
			XRT_SET_IRET(0);
			return xCore.sNull;
		}
		va_start(ip, sFormat);
		iSize = vsnprintf(sRet, iSize + 1, sFormat, ip);
		va_end(ip);
		sRet[iSize] = 0;
		XRT_SET_IRET(iSize);
		return sRet;
	} else {
		XRT_SET_IRET(0);
		return xCore.sNull;
	}
}



// 字符串替换（ 需使用 xrtFree 释放 ）
XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize)
{
	if ( sText == NULL ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { XRT_SET_IRET(0); return xCore.sNull; }
	if ( sSubText == NULL ) { XRT_SET_IRET(iSize); return xrtCopyStr(sText, iSize); }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { XRT_SET_IRET(iSize); return xrtCopyStr(sText, iSize); }
	if ( sRepText == NULL ) { iRepSize = 0; } else { if ( iRepSize == 0 ) { iRepSize = strlen(sRepText); } }
	// 计算 sSubText 在 sText 中出现的次数
	size_t iFindCount = 0;
	str sTextPtr;
	str sSubPos;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (sTextPtr - sText) + 1, sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		iFindCount++;
	}
	// 为新字符串分配内存
	size_t iRetSize = iSize + iFindCount * (iRepSize - iSubSize);
	str sRet = (str)xrtMalloc(iRetSize + 1);
	if ( sRet == NULL ) {
		XRT_SET_IRET(0);
		return (str)xCore.sNull;
	}
	// 复制原始字符串, 替换需要改变的部分
	str sRetPtr = sRet;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (sTextPtr - sText) + 1, sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		size_t iSkipSize = sSubPos - sTextPtr;
		// 复制前面的部分，直到出现要查找的字符串
		strncpy(sRetPtr, sTextPtr, iSkipSize);
		sRetPtr += iSkipSize;
		// 复制要替换的字符串
		strncpy(sRetPtr, sRepText, iRepSize);
		sRetPtr += iRepSize;
	}
	// 复制最后一段剩下的字符串
	if ( &sText[iSize] > sTextPtr ) {
		memcpy(sRetPtr, sTextPtr, &sText[iSize] - sTextPtr);
	}
	sRet[iRetSize] = 0;
	XRT_SET_IRET(iRetSize);
	return sRet;
}



// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise)
{
	if ( sText == NULL ) { goto return_nullstr; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { goto return_nullstr; }
	if ( sSepText == NULL ) { goto return_nullsep; }
	if ( iSepSize == 0 ) { iSize = strlen(sSepText); }
	if ( iSepSize == 0 ) { goto return_nullsep; }
	// 统计分隔符出现的次数
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		str pPos = &sText[i];
		int bOK = TRUE;
		for ( int j = 0; j < iSepSize; j++ ) {
			if ( pPos[j] != sSepText[j] ) {
				bOK = FALSE;
				break;
			}
		}
		if ( bOK ) {
			iCount++;
			i += iSepSize - 1;
		}
	}
	// 如果字符串没有被分割，按照分隔符为空处理
	if ( iCount == 0 ) {
		goto return_nullsep;
	}
	// 准备返回的数据 [分割指针 + NULL + 字符串表 + \0]
	str* sRet;
	str pData;
	if ( bSrcRevise ) {
		sRet = xrtMalloc( (iCount + 2) * sizeof(ptr) );
		if ( sRet == NULL ) {
			goto return_nullstr;
		}
		pData = sText;
	} else {
		sRet = xrtMalloc( ((iCount + 2) * sizeof(ptr)) + (iSize - ((iSepSize - 1) * iCount)) + 1 );
		if ( sRet == NULL ) {
			goto return_nullstr;
		}
		pData = (str)&sRet[iCount + 2];
	}
	// 开始分割数据
	iCount = 0;
	int iPos = 0;
	str pAddr = pData;
	for ( int i = 0; i < iSize; i++ ) {
		str pPos = &sText[i];
		int bOK = TRUE;
		for ( int j = 0; j < iSepSize; j++ ) {
			if ( pPos[j] != sSepText[j] ) {
				bOK = FALSE;
				break;
			}
		}
		if ( bOK ) {
			// 找到分隔符
			sRet[iCount] = pAddr;
			iCount++;
			if ( bSrcRevise ) {
				pData[i] = 0;
				pAddr = pData + i + iSepSize;
			} else {
				pData[iPos] = 0;
				iPos++;
				pAddr = &pData[iPos];
			}
			i += iSepSize - 1;
		} else {
			// 没找到分隔符（不修改源数据时负责数据拷贝）
			if ( bSrcRevise == FALSE ) {
				pData[iPos] = sText[i];
				iPos++;
			}
		}
	}
	if ( bSrcRevise == FALSE ) {
		pData[iPos] = 0;
	}
	sRet[iCount] = pAddr;
	iCount++;
	sRet[iCount] = NULL;
	XRT_SET_IRET(iCount);
	return sRet;
	
// 处理内容为 空字符串 或 NULL 的情况（只返回包含一个空元素的数组）
return_nullstr:
	sRet = xrtMalloc(2 * sizeof(void*));
	if ( sRet == NULL ) {
		goto return_error;
	}
	sRet[0] = xCore.sNull;
	sRet[1] = NULL;
	XRT_SET_IRET(1);
	return sRet;
	
// 处理分隔符为 空字符串 或 NULL 的情况（只返回包含一个内容的数组）
return_nullsep:
	if ( bSrcRevise ) {
		sRet = xrtMalloc(2 * sizeof(void*));
		if ( sRet == NULL ) {
			goto return_error;
		}
		sRet[0] = sText;
	} else {
		sRet = xrtMalloc((2 * sizeof(void*)) + iSize + 1);
		if ( sRet == NULL ) {
			goto return_error;
		}
		str sTextRef = (str)&sRet[2];
		memcpy(sTextRef, sText, iSize);
		sTextRef[iSize] = 0;
		sRet[0] = sTextRef;
	}
	sRet[1] = NULL;
	XRT_SET_IRET(1);
	return sRet;
	
// 内存申请异常返回
return_error:
	XRT_SET_IRET(0);
	return (str*)xCore.sNull;
}



// 生成随机字符串（ 需使用 xrtFree 释放 ）
static const str RandStringDefaultTemplate = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen)
{
	if ( sTemplate == NULL ) { sTemplate = RandStringDefaultTemplate; iSize = 64; }
	if ( iSize == 0 ) { iSize = strlen(sTemplate); }
	if ( iSize == 0 ) { sTemplate = RandStringDefaultTemplate; iSize = 64; }
	if ( iLen == 0 ) { return xCore.sNull; }
	iSize--;
	str sRet = xrtMalloc(iLen + 1);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	for ( int i = 0; i < iLen; i++ ) {
		int idx = xrtRandRange(0, iSize);
		sRet[i] = sTemplate[idx];
	}
	sRet[iLen] = 0;
	return sRet;
}



// HEX 编码（ 需使用 xrtFree 释放 ）
#define dec2hex(c) (c > 9 ? c + 55 : c + '0')
XXAPI str xrtHexEncode(ptr pMem, size_t iSize)
{
	if ( pMem == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(pMem); }
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	str sRet = xrtMalloc((iSize * 2) + 1);
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return xCore.sNull;
	}
	uint8* pStr = pMem;
	int iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		int i1 = (pStr[i] & 0xF0) >> 4;
		int i2 = pStr[i] & 0x0F;
		sRet[iPos++] = dec2hex(i1);
		sRet[iPos++] = dec2hex(i2);
	}
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// HEX 解码（ 需使用 xrtFree 释放 ）
#define hex2dec(c) (c <= '9' ? c - '0' : c <= 'F' ? c - 55 : c - 87)
XXAPI ptr xrtHexDecode(str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	str sRet = xrtMalloc((iSize / 2) + 1);
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return xCore.sNull;
	}
	int iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint8 c0 = sText[i++];
		uint8 c1 = sText[i];
		sRet[iPos++] = (hex2dec(c0) << 4) + hex2dec(c1);
	}
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// Base64 编码（ 需使用 xrtFree 释放 ）
static const str Base64EncodeTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable)
{
	if ( pMem == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(pMem); }
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	if ( sTable == NULL ) { sTable = Base64EncodeTable; }
	// 申请返回值内存
	size_t iRet= 4 * ((iSize + 2) / 3);
	str sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return xCore.sNull;
	}
	// 开始编码
	uint8* pStr = pMem;
	for ( size_t i = 0, j = 0; i < iSize; ) {
		uint32_t octet_a = i < iSize ? pStr[i++] : 0;
		uint32_t octet_b = i < iSize ? pStr[i++] : 0;
		uint32_t octet_c = i < iSize ? pStr[i++] : 0;
		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
		sRet[j++] = sTable[(triple >> 3 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 2 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 1 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 0 * 6) & 0x3F];
	}
	// 添加填充字符 '='
	for ( size_t i = 0; i < (3 - iSize % 3) % 3; i++ ) {
		sRet[iRet - 1 - i] = '=';
	}
	// 返回编码后的数据
	sRet[iRet] = 0;
	xCore.iRet = iRet;
	return sRet;
}



// Base64 解码（ 需使用 xrtFree 释放 ）
static const str sErrorBase64_mul4 = "Base64 input length must be multiple of 4 !";
static const str sErrorBase64_char = "Base64 input contains invalid characters !";
XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	int8_t Base64DecodeTable[128] = {
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,		// 0-15
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,		// 16-31
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,		// 32-47
		52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,		// 48-63
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,		// 64-79
		15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,		// 80-95
		-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,		// 96-111
		41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1			// 112-127
	};
	if ( sTable != NULL ) {
		for ( int i = 0; i < 64; i++ ) {
			Base64DecodeTable[sTable[i]] = i;
		}
	}
	// 计算输出缓冲区大小
	if ( iSize % 4 != 0 ) {
		xrtSetError(sErrorBase64_mul4, FALSE);
		xCore.iRet = 0;
		return xCore.sNull;
	}
	// 计算返回长度
	int iRet = iSize / 4 * 3;
	if ( sText[iSize - 1] == '=' ) { iRet--; }
	if ( sText[iSize - 2] == '=' ) { iRet--; }
	// 申请返回值内存
	str sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return xCore.sNull;
	}
	// 开始解码
	for (size_t i = 0, j = 0; i < iSize;) {
		int8_t sextet_a = sText[i] == '=' ? 0 & i++ : Base64DecodeTable[(int)sText[i++]];
		int8_t sextet_b = sText[i] == '=' ? 0 & i++ : Base64DecodeTable[(int)sText[i++]];
		int8_t sextet_c = sText[i] == '=' ? 0 & i++ : Base64DecodeTable[(int)sText[i++]];
		int8_t sextet_d = sText[i] == '=' ? 0 & i++ : Base64DecodeTable[(int)sText[i++]];
		// 发现非法字符
		if (sextet_a == -1 || sextet_b == -1 || sextet_c == -1 || sextet_d == -1) {
			xrtSetError(sErrorBase64_char, FALSE);
			xrtFree(sRet);
			xCore.iRet = 0;
			return xCore.sNull;
		}
		// 组合 4 个 6 位值为 3 个 8 位字节
		uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);
		if ( j < iRet ) { sRet[j++] = (triple >> 2 * 8) & 0xFF; }
		if ( j < iRet ) { sRet[j++] = (triple >> 1 * 8) & 0xFF; }
		if ( j < iRet ) { sRet[j++] = (triple >> 0 * 8) & 0xFF; }
	}
	sRet[iRet] = '\0';
	xCore.iRet = iRet;
	return sRet;
}


