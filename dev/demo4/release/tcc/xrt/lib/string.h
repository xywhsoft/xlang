


// 创建字符串副本（ 需使用 xrtFree 释放 ）(线程安全)
XXAPI str xrtCopyStr(str sText, size_t iSize)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	memcpy(sRet, sText, iSize);
	sRet[iSize] = 0;
	return sRet;
}
XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { return (u16str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u16len(sText); }
	if ( iSize == 0 ) { return (u16str)xCore.sNull; }
	u16str sRet = xrtMalloc((iSize + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) { return (u16str)xCore.sNull; }
	memcpy(sRet, sText, iSize * sizeof(unsigned short));
	sRet[iSize] = 0;
	return sRet;
}
XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { return (u32str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u32len(sText); }
	if ( iSize == 0 ) { return (u32str)xCore.sNull; }
	u32str sRet = xrtMalloc((iSize + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) { return (u32str)xCore.sNull; }
	memcpy(sRet, sText, iSize * sizeof(unsigned int));
	sRet[iSize] = 0;
	return sRet;
}
XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize)
{
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { return xCore.sNull; }
	ptr pRet = xrtMalloc(iSize);
	if ( pRet == NULL ) { return xCore.sNull; }
	memcpy(pRet, pMem, iSize);
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
		unsigned char c = (unsigned char)sRet[i];
		if ( (c & 0x80) == 0 ) {
			// ASCII 字符
			sRet[i] = tolower(sRet[i]);
		} else if ( (c & 0xE0) == 0xC0 ) {
			// UTF-8 双字节字符，跳过
			i++;
		} else if ( (c & 0xF0) == 0xE0 ) {
			// UTF-8 三字节字符，跳过
			i += 2;
		} else if ( (c & 0xF8) == 0xF0 ) {
			// UTF-8 四字节字符，跳过
			i += 3;
		} else if ( (c & 0xFC) == 0xF8 ) {
			// UTF-8 五字节字符，跳过
			i += 4;
		} else if ( (c & 0xFE) == 0xFC ) {
			// UTF-8 六字节字符，跳过
			i += 5;
		}
		// 其他情况（单独的续字节或异常字符）跳过
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
		unsigned char c = (unsigned char)sRet[i];
		if ( (c & 0x80) == 0 ) {
			// ASCII 字符
			sRet[i] = toupper(sRet[i]);
		} else if ( (c & 0xE0) == 0xC0 ) {
			// UTF-8 双字节字符，跳过
			i++;
		} else if ( (c & 0xF0) == 0xE0 ) {
			// UTF-8 三字节字符，跳过
			i += 2;
		} else if ( (c & 0xF8) == 0xF0 ) {
			// UTF-8 四字节字符，跳过
			i += 3;
		} else if ( (c & 0xFC) == 0xF8 ) {
			// UTF-8 五字节字符，跳过
			i += 4;
		} else if ( (c & 0xFE) == 0xFC ) {
			// UTF-8 六字节字符，跳过
			i += 5;
		}
		// 其他情况（单独的续字节或异常字符）跳过
	}
	return sRet;
}



// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）(线程安全)
XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	if ( sText == NULL ) { return NULL; }
	if ( sSubText == NULL ) { return NULL; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { return NULL; }
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
	return sSub;
}
XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	if ( sText == NULL ) { return 0; }
	if ( sSubText == NULL ) { return 0; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return 0; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { return 0; }
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
		return (sSub - sText) + 1;
	} else {
		return 0;
	}
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
XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
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
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
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
XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
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
		} else if ( (sText[i] & 0b11000000) == 0b10000000 ) {
			// UTF-8 续字节，向前查找首字节
			int iEnd = i;
			while ( (i > 0) && ((sText[i] & 0b11000000) == 0b10000000) ) {
				i--;
			}
			// 现在 i 指向首字节
			int iCharLen = iEnd - i + 1;
			if ( iCharLen <= iSubSize ) {
				size_t iLen = iSubSize - iCharLen + 1;
				for ( int j = 0; j < iLen; j++ ) {
					bool bMatch = TRUE;
					for ( int k = 0; k < iCharLen; k++ ) {
						if ( sSubText[j + k] != sText[i + k] ) {
							bMatch = FALSE;
							break;
						}
					}
					if ( bMatch ) {
						iCount += iCharLen;
						bBreak = FALSE;
						break;
					}
				}
			}
			// i 已经在首字节，循环会 i-- 移到前一个字符末尾
		} else {
			// 孤立的首字节或异常字符（FE、FF），跳过
		}
		if ( bBreak ) {
			break;
		}
	}
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(sText, iSize - iCount);
	}
}
XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
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
	if ( iCountL >= iSize ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
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
		} else if ( (sText[i] & 0b11000000) == 0b10000000 ) {
			// UTF-8 续字节，向前查找首字节
			int iEnd = i;
			while ( (i > 0) && ((sText[i] & 0b11000000) == 0b10000000) ) {
				i--;
			}
			// 现在 i 指向首字节
			int iCharLen = iEnd - i + 1;
			if ( iCharLen <= iSubSize ) {
				size_t iLen = iSubSize - iCharLen + 1;
				for ( int j = 0; j < iLen; j++ ) {
					bool bMatch = TRUE;
					for ( int k = 0; k < iCharLen; k++ ) {
						if ( sSubText[j + k] != sText[i + k] ) {
							bMatch = FALSE;
							break;
						}
					}
					if ( bMatch ) {
						iCountR += iCharLen;
						bBreak = FALSE;
						break;
					}
				}
			}
			// i 已经在首字节，循环会 i-- 移到前一个字符末尾
		} else {
			// 孤立的首字节或异常字符（FE、FF），跳过
		}
		if ( bBreak ) {
			break;
		}
	}
	int iCount = iCountL + iCountR;
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
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
XXAPI str xrtFilterStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { if ( bSrcRevise ) { if ( iRetSize ) { *iRetSize = iSize; } return sText; } else { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); } }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { if ( bSrcRevise ) { if ( iRetSize ) { *iRetSize = iSize; } return sText; } else { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); } }
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
	if ( iRetSize ) { *iRetSize = iCount; }
	return sText;
}



// 字符串格式化（ 需使用 xrtFree 释放 ）
XXAPI str xrtFormat(str sFormat, ...)
{
	if ( sFormat == NULL ) { return xCore.sNull; }
	va_list ip;
	va_start(ip, sFormat);
	int iSize = vsnprintf(NULL, 0, sFormat, ip);
	va_end(ip);
	if ( iSize > 0 ) {
		str sRet = xrtMalloc(iSize + 1);
		if ( sRet == NULL ) { return xCore.sNull; }
		va_start(ip, sFormat);
		iSize = vsnprintf(sRet, iSize + 1, sFormat, ip);
		va_end(ip);
		sRet[iSize] = 0;
		return sRet;
	} else {
		return xCore.sNull;
	}
}



// 字符串替换（ 需使用 xrtFree 释放 ）
XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); }
	if ( sRepText == NULL ) { iRepSize = 0; } else { if ( iRepSize == 0 ) { iRepSize = strlen(sRepText); } }
	// 计算 sSubText 在 sText 中出现的次数
	size_t iFindCount = 0;
	str sTextPtr;
	str sSubPos;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (sTextPtr - sText) + 1, sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		iFindCount++;
	}
	// 为新字符串分配内存
	size_t iRet = iSize + iFindCount * (iRepSize - iSubSize);
	str sRet = (str)xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (str)xCore.sNull; }
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
	sRet[iRet] = 0;
	if ( iRetSize ) { *iRetSize = iRet; }
	return sRet;
}



// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise, size_t* iRetSize)
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
	if ( iRetSize ) { *iRetSize = iCount; }
	return sRet;
	
// 处理内容为 空字符串 或 NULL 的情况（只返回包含一个空元素的数组）
return_nullstr:
	sRet = xrtMalloc(2 * sizeof(void*));
	if ( sRet == NULL ) {
		goto return_error;
	}
	sRet[0] = xCore.sNull;
	sRet[1] = NULL;
	if ( iRetSize ) { *iRetSize = 1; }
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
	if ( iRetSize ) { *iRetSize = 1; }
	return sRet;
	
// 内存申请异常返回
return_error:
	if ( iRetSize ) { *iRetSize = 0; }
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
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(pMem); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc((iSize * 2) + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	uint8* pStr = pMem;
	int iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		int i1 = (pStr[i] & 0xF0) >> 4;
		int i2 = pStr[i] & 0x0F;
		sRet[iPos++] = dec2hex(i1);
		sRet[iPos++] = dec2hex(i2);
	}
	sRet[iPos] = 0;
	return sRet;
}



// HEX 解码（ 需使用 xrtFree 释放 ）
#define hex2dec(c) (c <= '9' ? c - '0' : c <= 'F' ? c - 55 : c - 87)
XXAPI ptr xrtHexDecode(str sText, size_t iSize)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc((iSize / 2) + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	int iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint8 c0 = sText[i++];
		uint8 c1 = sText[i];
		sRet[iPos++] = (hex2dec(c0) << 4) + hex2dec(c1);
	}
	sRet[iPos] = 0;
	return sRet;
}



// Base64 编码（ 需使用 xrtFree 释放 ）
static const str Base64EncodeTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable)
{
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(pMem); }
	if ( iSize == 0 ) { return xCore.sNull; }
	if ( sTable == NULL ) { sTable = Base64EncodeTable; }
	// 申请返回值内存
	size_t iRet= 4 * ((iSize + 2) / 3);
	str sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
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
	return sRet;
}



// Base64 解码（ 需使用 xrtFree 释放 ）
static const str sErrorBase64_mul4 = "Base64 input length must be multiple of 4 !";
static const str sErrorBase64_char = "Base64 input contains invalid characters !";
XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return xCore.sNull; }
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
		return xCore.sNull;
	}
	// 计算返回长度
	int iRet = iSize / 4 * 3;
	if ( sText[iSize - 1] == '=' ) { iRet--; }
	if ( sText[iSize - 2] == '=' ) { iRet--; }
	// 申请返回值内存
	str sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) {
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
			return xCore.sNull;
		}
		// 组合 4 个 6 位值为 3 个 8 位字节
		uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);
		if ( j < iRet ) { sRet[j++] = (triple >> 2 * 8) & 0xFF; }
		if ( j < iRet ) { sRet[j++] = (triple >> 1 * 8) & 0xFF; }
		if ( j < iRet ) { sRet[j++] = (triple >> 0 * 8) & 0xFF; }
	}
	sRet[iRet] = '\0';
	return sRet;
}



// 通配符匹配（ * 匹配任意字符序列，? 匹配单个UTF-8字符，bCase 为 TRUE 时忽略大小写 ）
// 使用贪婪匹配算法：O(n*m) 最坏时间复杂度，O(1) 空间复杂度
XXAPI bool xrtStrLike(str sText, size_t iTextSize, str sPattern, size_t iPatSize, bool bCase)
{
	// 参数检查
	if ( sPattern == NULL ) { return FALSE; }
	if ( iPatSize == 0 ) { iPatSize = strlen(sPattern); }
	
	// 空模式只匹配空字符串
	if ( iPatSize == 0 ) {
		if ( sText == NULL ) { return TRUE; }
		if ( iTextSize == 0 ) { iTextSize = strlen(sText); }
		return iTextSize == 0;
	}
	
	// 处理空文本
	if ( sText == NULL ) {
		// 空文本只能匹配全是 * 的模式
		for ( size_t i = 0; i < iPatSize; i++ ) {
			if ( sPattern[i] != '*' ) { return FALSE; }
		}
		return TRUE;
	}
	if ( iTextSize == 0 ) { iTextSize = strlen(sText); }
	if ( iTextSize == 0 ) {
		for ( size_t i = 0; i < iPatSize; i++ ) {
			if ( sPattern[i] != '*' ) { return FALSE; }
		}
		return TRUE;
	}
	
	// 贪婪匹配算法
	size_t t = 0;           // 文本位置
	size_t p = 0;           // 模式位置
	size_t starP = (size_t)-1;   // 最近的 * 在模式中的位置
	size_t starT = 0;       // 遇到 * 时文本的位置
	
	while ( t < iTextSize ) {
		if ( p < iPatSize && sPattern[p] == '*' ) {
			// 记录 * 的位置，先假定它匹配 0 个字符
			starP = p;
			starT = t;
			p++;
		} else if ( p < iPatSize && sPattern[p] == '?' ) {
			// ? 匹配一个完整的 UTF-8 字符
			int charLen = xrtCharLenU8((unsigned char)sText[t]);
			// 检查剩余长度是否足够
			if ( t + charLen > iTextSize ) {
				// 字符不完整，尝试回溯
				if ( starP == (size_t)-1 ) { return FALSE; }
				p = starP + 1;
				starT += xrtCharLenU8((unsigned char)sText[starT]);
				t = starT;
			} else {
				t += charLen;
				p++;
			}
		} else {
			// 普通字符匹配（内联字符比较）
			unsigned char c1 = (unsigned char)sText[t];
			unsigned char c2 = (unsigned char)sPattern[p];
			bool bMatch = (c1 == c2);
			if ( !bMatch && bCase ) {
				// 大小写不敏感：只对 ASCII 字母转换
				if ( c1 >= 'A' && c1 <= 'Z' ) { c1 += 32; }
				if ( c2 >= 'A' && c2 <= 'Z' ) { c2 += 32; }
				bMatch = (c1 == c2);
			}
			if ( p < iPatSize && bMatch ) {
				t++;
				p++;
			} else {
				// 匹配失败，回溯到上一个 *
				if ( starP == (size_t)-1 ) { return FALSE; }
				// 让 * 多匹配一个 UTF-8 字符
				p = starP + 1;
				starT += xrtCharLenU8((unsigned char)sText[starT]);
				t = starT;
				// 如果 starT 已超出文本范围，则匹配失败
				if ( starT > iTextSize ) { return FALSE; }
			}
		}
	}
	
	// 文本已匹配完，检查模式剩余部分是否全是 *
	while ( p < iPatSize && sPattern[p] == '*' ) {
		p++;
	}
	
	return p == iPatSize;
}



// ============================================================================
// 数值格式化函数
// ============================================================================

// 查表法: 十六进制字符表
static const char xrt_digit_table[] = "0123456789abcdef0123456789ABCDEF";

// 内部函数: 解析格式字符串
typedef struct {
	bool showSign;      // 显示正号
	bool thousands;     // 千分位
	bool percent;       // 百分比
	bool uppercase;     // 大写十六进制
	int base;           // 进制 (10, 16, 8, 2)
	int width;          // 前导零宽度
	int precision;      // 小数位数 (-1 表示未指定)
} XrtNumFmtOpts;

static inline void xrt_parse_format(str format, XrtNumFmtOpts* opts)
{
	opts->showSign = FALSE;
	opts->thousands = FALSE;
	opts->percent = FALSE;
	opts->uppercase = FALSE;
	opts->base = 10;
	opts->width = 0;
	opts->precision = -1;
	
	if ( format == NULL || *format == '\0' ) { return; }
	
	const char* p = format;
	while ( *p ) {
		char c = *p++;
		switch ( c ) {
			case '+': opts->showSign = TRUE; break;
			case ',': opts->thousands = TRUE; break;
			case '%': opts->percent = TRUE; break;
			case 'x': opts->base = 16; opts->uppercase = FALSE; break;
			case 'X': opts->base = 16; opts->uppercase = TRUE; break;
			case 'o': opts->base = 8; break;
			case 'b': case 'B': opts->base = 2; break;
			case '.':
				// 解析小数位数
				opts->precision = 0;
				while ( *p >= '0' && *p <= '9' ) {
					opts->precision = opts->precision * 10 + (*p++ - '0');
				}
				break;
			case '0':
				// 解析前导零宽度
				while ( *p >= '0' && *p <= '9' ) {
					opts->width = opts->width * 10 + (*p++ - '0');
				}
				break;
			default:
				if ( c >= '1' && c <= '9' ) {
					// 数字开头也解析为宽度
					opts->width = c - '0';
					while ( *p >= '0' && *p <= '9' ) {
						opts->width = opts->width * 10 + (*p++ - '0');
					}
				}
				break;
		}
	}
}

// 内部函数: uint64 转非十进制字符串（从 buffer 末尾往前写）
static inline char* xrt_u64_to_base(char* bufEnd, uint64 value, int base, bool upper)
{
	char* p = bufEnd;
	const char* digits = upper ? &xrt_digit_table[16] : xrt_digit_table;
	
	if ( base == 16 ) {
		do {
			*--p = digits[value & 0xF];
			value >>= 4;
		} while ( value );
	} else if ( base == 8 ) {
		do {
			*--p = '0' + (char)(value & 0x7);
			value >>= 3;
		} while ( value );
	} else {
		// 二进制
		do {
			*--p = '0' + (char)(value & 0x1);
			value >>= 1;
		} while ( value );
	}
	
	return p;
}

// 内部函数: 添加千分位分隔符
static inline int xrt_add_thousands(char* dst, const char* src, int srcLen)
{
	int commas = (srcLen - 1) / 3;  // 需要插入的逗号数量
	int totalLen = srcLen + commas;
	int pos = totalLen;
	int cnt = 0;
	
	for ( int i = srcLen - 1; i >= 0; i-- ) {
		dst[--pos] = src[i];
		if ( ++cnt == 3 && i > 0 ) {
			dst[--pos] = ',';
			cnt = 0;
		}
	}
	
	return totalLen;
}

// 整数格式化
XXAPI str xrtIntFormat(int64 value, str format)
{
	// 解析格式
	XrtNumFmtOpts opts;
	xrt_parse_format(format, &opts);
	
	// 处理符号
	bool negative = (value < 0);
	uint64 absVal = negative ? (uint64)(-(value + 1)) + 1 : (uint64)value;
	
	// 转换为字符串
	char tmpBuf[96];
	char* numStart;
	int numLen;
	
	if ( opts.base == 10 ) {
		// 十进制: 使用 xrtI64ToStr
		numLen = xrtI64ToStr(negative ? value : (int64)absVal, tmpBuf);
		numStart = tmpBuf;
		if ( negative ) {
			numStart++;  // 跳过负号
			numLen--;
		}
	} else {
		// 非十进制
		char* tmpEnd = tmpBuf + sizeof(tmpBuf);
		numStart = xrt_u64_to_base(tmpEnd, absVal, opts.base, opts.uppercase);
		numLen = (int)(tmpEnd - numStart);
		opts.showSign = FALSE;
		opts.thousands = FALSE;
		negative = FALSE;
	}
	
	// 计算所需总长度
	int signLen = (negative || opts.showSign) ? 1 : 0;
	int digitLen = numLen;
	if ( opts.thousands && digitLen > 3 ) {
		digitLen += (digitLen - 1) / 3;
	}
	int padLen = (opts.width > digitLen) ? (opts.width - digitLen) : 0;
	int totalLen = signLen + padLen + digitLen;
	
	// 分配缓冲区
	str buffer = xrtMalloc(totalLen + 1);
	if ( buffer == NULL ) { return xCore.sNull; }
	
	char* out = buffer;
	
	// 写入符号
	if ( signLen ) {
		*out++ = negative ? '-' : '+';
	}
	
	// 写入前导零
	while ( padLen-- > 0 ) { *out++ = '0'; }
	
	// 写入数字
	if ( opts.thousands && numLen > 3 ) {
		xrt_add_thousands(out, numStart, numLen);
		out += digitLen;
	} else {
		memcpy(out, numStart, numLen);
		out += numLen;
	}
	
	*out = '\0';
	return buffer;
}

// 浮点数格式化
XXAPI str xrtNumFormat(double value, str format)
{
	// 解析格式
	XrtNumFmtOpts opts;
	xrt_parse_format(format, &opts);
	
	// 处理百分比
	if ( opts.percent ) {
		value *= 100.0;
	}
	
	// 处理特殊值
	if ( value != value ) {  // NaN
		str ret = xrtMalloc(4);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "NaN", 4);
		return ret;
	}
	if ( value > 1e308 ) {
		str ret = xrtMalloc(4);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "Inf", 4);
		return ret;
	}
	if ( value < -1e308 ) {
		str ret = xrtMalloc(5);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "-Inf", 5);
		return ret;
	}
	
	// 处理符号
	bool negative = (value < 0);
	if ( negative ) { value = -value; }
	
	// 确定小数位数
	int precision = (opts.precision >= 0) ? opts.precision : 6;
	if ( precision > 15 ) { precision = 15; }
	
	// 四舍五入
	static const double roundTable[] = {
		0.5, 0.05, 0.005, 0.0005, 0.00005, 0.000005, 0.0000005, 0.00000005,
		0.000000005, 0.0000000005, 0.00000000005, 0.000000000005,
		0.0000000000005, 0.00000000000005, 0.000000000000005, 0.0000000000000005
	};
	value += roundTable[precision];
	
	// 分离整数部分和小数部分
	uint64 intPart = (uint64)value;
	double fracPart = value - (double)intPart;
	
	// 转换整数部分: 使用 xrtI64ToStr
	char tmpBuf[32];
	int intLen = xrtI64ToStr((int64)intPart, tmpBuf);
	char* intStart = tmpBuf;
	
	// 计算所需总长度
	int signLen = (negative || opts.showSign) ? 1 : 0;
	int intDigitLen = intLen;
	if ( opts.thousands && intDigitLen > 3 ) {
		intDigitLen += (intDigitLen - 1) / 3;
	}
	int fracLen = (precision > 0) ? (1 + precision) : 0;
	int percentLen = opts.percent ? 1 : 0;
	int totalLen = signLen + intDigitLen + fracLen + percentLen;
	
	// 分配缓冲区
	str buffer = xrtMalloc(totalLen + 1);
	if ( buffer == NULL ) { return xCore.sNull; }
	
	char* out = buffer;
	
	// 写入符号
	if ( negative ) {
		*out++ = '-';
	} else if ( opts.showSign ) {
		*out++ = '+';
	}
	
	// 写入整数部分
	if ( opts.thousands && intLen > 3 ) {
		xrt_add_thousands(out, intStart, intLen);
		out += intDigitLen;
	} else {
		memcpy(out, intStart, intLen);
		out += intLen;
	}
	
	// 写入小数部分
	if ( precision > 0 ) {
		*out++ = '.';
		for ( int i = 0; i < precision; i++ ) {
			fracPart *= 10.0;
			int digit = (int)fracPart;
			*out++ = '0' + digit;
			fracPart -= digit;
		}
	}
	
	// 写入百分号
	if ( opts.percent ) {
		*out++ = '%';
	}
	
	*out = '\0';
	return buffer;
}



// 字符串相似度（基于 Levenshtein 编辑距离，返回 0.0-1.0）
// 高性能优化：一维数组 O(min(m,n)) 空间，内联 min 计算
XXAPI double xrtStrSim(str s1, size_t len1, str s2, size_t len2)
{
	// 空指针检查
	if ( s1 == NULL ) { s1 = ""; len1 = 0; }
	if ( s2 == NULL ) { s2 = ""; len2 = 0; }
	
	// 自动计算长度
	if ( len1 == 0 ) { len1 = strlen(s1); }
	if ( len2 == 0 ) { len2 = strlen(s2); }
	
	// 快速路径：空字符串
	if ( len1 == 0 && len2 == 0 ) { return 1.0; }
	if ( len1 == 0 ) { return 0.0; }
	if ( len2 == 0 ) { return 0.0; }
	
	// 快速路径：完全相同
	if ( len1 == len2 && memcmp(s1, s2, len1) == 0 ) { return 1.0; }
	
	// 确保 s1 是较长的字符串（优化内存访问）
	if ( len1 < len2 ) {
		str ts = s1; s1 = s2; s2 = ts;
		size_t tl = len1; len1 = len2; len2 = tl;
	}
	
	// 分配一维 DP 数组（只需要较短字符串的长度+1）
	size_t dpSize = len2 + 1;
	int* dp = (int*)xrtMalloc(dpSize * sizeof(int));
	if ( dp == NULL ) { return 0.0; }
	
	// 初始化第一行：dp[j] = j
	for ( size_t j = 0; j <= len2; j++ ) {
		dp[j] = (int)j;
	}
	
	// DP 计算（行优先遍历，对缓存友好）
	for ( size_t i = 1; i <= len1; i++ ) {
		int prev = dp[0];  // 保存 dp[i-1][j-1]
		dp[0] = (int)i;    // dp[i][0] = i
		
		unsigned char c1 = (unsigned char)s1[i - 1];
		
		for ( size_t j = 1; j <= len2; j++ ) {
			int temp = dp[j];  // 保存当前值，作为下一次迭代的 prev
			
			if ( c1 == (unsigned char)s2[j - 1] ) {
				// 字符相同，无需操作
				dp[j] = prev;
			} else {
				// 字符不同，取 min(删除, 插入, 替换) + 1
				int del = dp[j];      // dp[i-1][j] + 1
				int ins = dp[j - 1];  // dp[i][j-1] + 1
				int rep = prev;       // dp[i-1][j-1] + 1
				
				// 内联 min3 计算
				int minVal = del;
				if ( ins < minVal ) { minVal = ins; }
				if ( rep < minVal ) { minVal = rep; }
				
				dp[j] = minVal + 1;
			}
			
			prev = temp;
		}
	}
	
	// 获取编辑距离
	int distance = dp[len2];
	xrtFree(dp);
	
	// 计算相似度：1 - distance / maxLen
	size_t maxLen = len1;  // len1 >= len2
	return 1.0 - (double)distance / (double)maxLen;
}



// 字符串约等于（使用 xCore 配置）
// iApproxStrMode=0: 通配符模式（使用 xrtStrLike，s2 为模式串）
// iApproxStrMode=1: 相似度模式（使用 xrtStrSim 和 fApproxStrTol 阈值）
XXAPI bool xrtStrApprox(str s1, size_t len1, str s2, size_t len2)
{
	if ( xCore.iApproxStrMode == XRT_STR_APPROX_LIKE ) {
		// 通配符模式
		return xrtStrLike(s1, len1, s2, len2, xCore.bApproxStrCase);
	} else {
		// 相似度模式
		double threshold = xCore.fApproxStrTol;
		if ( threshold <= 0.0 || threshold > 1.0 ) {
			threshold = 0.95;  // 无效阈值使用默认值
		}
		double sim = xrtStrSim(s1, len1, s2, len2);
		return sim >= threshold;
	}
}

