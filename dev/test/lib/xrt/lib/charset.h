


// utf8 字符长度码表
static char BytesExtraTableUTF8[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};



// utf-8 转 utf-16（ 需使用 xrtFree 释放 ）
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	// 计算数据长度和转换长度
	size_t iPos = 0;
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			char iExtraBytes = BytesExtraTableUTF8[sText[iSize]];
			if ( iExtraBytes < 3 ) {
				// 小于等于 3 字节的 utf8 字符会被编码为 2 字节的 utf16 字符
				iPos++;
			} else if ( iExtraBytes == 3 ) {
				// 4 字节的 utf8 会被编码为 4 字节的 utf16 字符
				iPos += 2;
			} else {
				// 超过 4 字节的 utf8 会被替换为 FFFD 替换码点（ 超出 utf16 支持的范围 ）
				iPos++;
			}
			iSize += iExtraBytes + 1;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			char iExtraBytes = BytesExtraTableUTF8[sText[i]];
			if ( iExtraBytes < 3 ) {
				// 小于等于 3 字节的 utf8 字符会被编码为 2 字节的 utf16 字符
				iPos++;
			} else if ( iExtraBytes == 3 ) {
				// 4 字节的 utf8 会被编码为 4 字节的 utf16 字符
				iPos += 2;
			} else {
				// 超过 4 字节的 utf8 会被替换为 FFFD 替换码点（ 超出 utf16 支持的范围 ）
				iPos++;
			}
			i += iExtraBytes;
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	// 申请所需内存
	u16str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return (u16str)xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		char iExtraBytes = BytesExtraTableUTF8[sText[i]];
		if ( iExtraBytes == 0 ) {
			// ASCII 兼容字符
			sRet[iPos++] = sText[i];
		} else if ( iExtraBytes == 1 ) {
			// 双字节字符
			sRet[iPos++] = ((sText[i] & 0b00011111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 2 ) {
			// 三字节字符
			sRet[iPos++] = ((sText[i] & 0b00001111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 3 ) {
			// 四字节字符
			if ( sText[i] & 0b00000100 ) {
				// 超出 utf16 支持的范围，使用替换字符 FFFD 代替
				sRet[iPos++] = 0xFFFD;
				i += iExtraBytes;
			} else {
				uint32 c = ((sText[i] & 0b00000011) << 18) | ((sText[++i] & 0b00111111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
				if ( c < 0x10000 ) {
					// 原则上不会进入这个分支，除非遇到错误的编码
					sRet[iPos++] = c;
				} else {
					c -= 0x10000;
					sRet[iPos++] = 0b1101100000000000 | ((c & 0b11111111110000000000) >> 10);
					sRet[iPos++] = 0b1101110000000000 | (c & 0b00000000001111111111);
				}
			}
		} else if ( iExtraBytes == 4 ) {
			// 五字节字符（ 超出 utf16 支持的范围，使用替换字符 FFFD 代替 ）
			sRet[iPos++] = 0xFFFD;
			i += iExtraBytes;
		} else if ( iExtraBytes == 5 ) {
			// 五字节字符（ 超出 utf16 支持的范围，使用替换字符 FFFD 代替 ）
			sRet[iPos++] = 0xFFFD;
			i += iExtraBytes;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-8 转 utf-32（ 需使用 xrtFree 释放 ）
XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	// 计算数据长度和转换长度
	size_t iPos = 0;
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			iPos++;
			iSize += BytesExtraTableUTF8[sText[iSize]] + 1;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			iPos++;
			i += BytesExtraTableUTF8[sText[i]];
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	// 申请所需内存
	u32str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return (u32str)xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		char iExtraBytes = BytesExtraTableUTF8[sText[i]];
		if ( iExtraBytes == 0 ) {
			// ASCII 兼容字符
			sRet[iPos++] = sText[i];
		} else if ( iExtraBytes == 1 ) {
			// 双字节字符
			sRet[iPos++] = ((sText[i] & 0b00011111) << 6) | (sText[++i] & 0x3F);
		} else if ( iExtraBytes == 2 ) {
			// 三字节字符
			sRet[iPos++] = ((sText[i] & 0b00001111) << 12) | ((sText[++i] & 0x3F) << 6) | (sText[++i] & 0x3F);
		} else if ( iExtraBytes == 3 ) {
			// 四字节字符
			sRet[iPos++] = ((sText[i] & 0b00000111) << 18) | ((sText[++i] & 0x3F) << 12) | ((sText[++i] & 0x3F) << 6) | (sText[++i] & 0x3F);
		} else if ( iExtraBytes == 4 ) {
			// 五字节字符
			sRet[iPos++] = ((sText[i] & 0b00000011) << 24) | ((sText[++i] & 0x3F) << 18) | ((sText[++i] & 0x3F) << 12) | ((sText[++i] & 0x3F) << 6) | (sText[++i] & 0x3F);
		} else if ( iExtraBytes == 5 ) {
			// 六字节字符
			sRet[iPos++] = ((sText[i] & 0b00000001) << 30) | ((sText[++i] & 0x3F) << 24) | ((sText[++i] & 0x3F) << 18) | ((sText[++i] & 0x3F) << 12) | ((sText[++i] & 0x3F) << 6) | (sText[++i] & 0x3F);
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 转 utf-8（ 需使用 xrtFree 释放 ）
XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			uint16 iChar = sText[iSize];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				if ( (sText[iSize + 1] & 0b1111110000000000) == 0b1101110000000000 ) {
					iPos += 4;
				} else {
					// 错误的代理对，使用替换字符 EFBFBD 代替
					iPos += 3;
				}
				iSize += 2;
			} else if ( iChar <= 0x7F ) {
				iPos++;
				iSize++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
				iSize++;
			} else {
				iPos += 3;
				iSize++;
			}
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint16 iChar = sText[i];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				if ( (sText[++i] & 0b1111110000000000) == 0b1101110000000000 ) {
					iPos += 4;
				} else {
					// 错误的代理对，使用替换字符 EFBFBD 代替
					iPos += 3;
				}
			} else if ( iChar <= 0x7F ) {
				iPos++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
			} else {
				iPos += 3;
			}
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	// 申请所需内存
	u8str sRet = xrtMalloc(iPos + 1);
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint16 iChar = sText[i];
		if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
			uint16 iNext = sText[++i];
			if ( (iNext & 0b1111110000000000) == 0b1101110000000000 ) {
				uint32 cp = (((iChar & 0x3FF) << 10) | (iNext & 0x3FF)) + 0x10000;
				sRet[iPos++] = 0xF0 | ((cp >> 18) & 0x7);
				sRet[iPos++] = 0x80 | ((cp >> 12) & 0x3F);
				sRet[iPos++] = 0x80 | ((cp >> 6) & 0x3F);
				sRet[iPos++] = 0x80 | (cp & 0x3F);
			} else {
				// 错误的代理对，使用替换字符 EFBFBD 代替
				sRet[iPos++] = 0xEF;
				sRet[iPos++] = 0xBF;
				sRet[iPos++] = 0xBD;
			}
		} else if ( iChar <= 0x7F ) {
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x7FF ) {
			sRet[iPos++] = 0xC0 | ((iChar & 0x7C0) >> 6);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else {
			sRet[iPos++] = 0xE0 | ((iChar & 0xF000) >> 12);
			sRet[iPos++] = 0x80 | ((iChar & 0xFC0) >> 6);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 转 utf-32（ 需使用 xrtFree 释放 ）
XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			if ( (sText[iSize] & 0b1111110000000000) == 0b1101100000000000 ) {
				iSize += 2;
			} else {
				iSize++;
			}
			iPos++;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint16 iChar = sText[i];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				i++;
			}
			iPos++;
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	// 申请所需内存
	u32str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return (u32str)xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint16 iChar = sText[i];
		if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
			uint16 iNext = sText[++i];
			if ( (iNext & 0b1111110000000000) == 0b1101110000000000 ) {
				sRet[iPos++] = (((iChar & 0x3FF) << 10) | (iNext & 0x3FF)) + 0x10000;
			} else {
				// 错误的代理对，使用替换字符 FFFD 代替
				sRet[iPos++] = 0xFFFD;
			}
		} else {
			sRet[iPos++] = iChar;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-32 转 utf-8（ 需使用 xrtFree 释放 ）
XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			uint32 iChar = sText[iSize++];
			if ( iChar <= 0x7F ) {
				iPos++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
			} else if ( iChar <= 0xFFFF ) {
				iPos += 3;
			} else if ( iChar <= 0x1FFFFF ) {
				iPos += 4;
			} else if ( iChar <= 0x3FFFFFF ) {
				iPos += 5;
			} else if ( iChar <= 0x7FFFFFFF ) {
				iPos += 6;
			}
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint32 iChar = sText[i];
			if ( iChar <= 0x7F ) {
				iPos++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
			} else if ( iChar <= 0xFFFF ) {
				iPos += 3;
			} else if ( iChar <= 0x1FFFFF ) {
				iPos += 4;
			} else if ( iChar <= 0x3FFFFFF ) {
				iPos += 5;
			} else if ( iChar <= 0x7FFFFFFF ) {
				iPos += 6;
			}
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	// 申请所需内存
	u8str sRet = xrtMalloc(iPos + 1);
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint32 iChar = sText[i];
		if ( iChar <= 0x7F ) {
			// ASCII 兼容字符
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x7FF ) {
			// 双字节字符
			sRet[iPos++] = 0xC0 | ((iChar >> 6) & 0x1F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0xFFFF ) {
			// 三字节字符
			sRet[iPos++] = 0xE0 | ((iChar >> 12) & 0xF);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x1FFFFF ) {
			// 四字节字符
			sRet[iPos++] = 0xF0 | ((iChar >> 18) & 0x7);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x3FFFFFF ) {
			// 五字节字符
			sRet[iPos++] = 0xF8 | ((iChar >> 24) & 0x3);
			sRet[iPos++] = 0x80 | ((iChar >> 18) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x7FFFFFFF ) {
			// 六字节字符
			sRet[iPos++] = 0xFC | ((iChar >> 30) & 0x1);
			sRet[iPos++] = 0x80 | ((iChar >> 24) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 18) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-32 转 utf-16 ( 需使用 xrtFree 释放 )
XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			if ( sText[iSize] <= 0xFFFF ) {
				iPos++;
			} else if ( sText[iSize] <= 0x10FFFF ) {
				iPos += 2;
			} else {
				iPos++;
			}
			iSize++;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint32 iChar = sText[i];
			if ( iChar <= 0xFFFF ) {
				iPos++;
			} else if ( iChar <= 0x10FFFF ) {
				iPos += 2;
			} else {
				iPos++;
			}
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	// 申请所需内存
	u16str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) {
		xCore.iRet = 0;
		return (u16str)xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint32 iChar = sText[i];
		if ( iChar <= 0xFFFF ) {
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x10FFFF ) {
			iChar -= 0x10000;
			sRet[iPos++] = 0b1101100000000000 | ((iChar & 0b11111111110000000000) >> 10);
			sRet[iPos++] = 0b1101110000000000 | (iChar & 0b00000000001111111111);
		} else {
			// 超出 utf16 支持的范围，使用替换字符 FFFD 代替
			sRet[iPos++] = 0xFFFD;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 大端序和小端序转换 ( 需使用 xrtFree 释放 )
XXAPI u16str xrtUTF16LEtoBE(u16str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return (u16str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u16len(sText); }
	if ( iSize == 0 ) { return (u16str)xCore.sNull; }
	u16str sRet;
	if ( bSrcRevise ) {
		sRet = sText;
	} else {
		sRet = xrtCopyStrU16(sText, iSize);
	}
	for ( int i = 0; i < iSize; i++ ) {
		sRet[i] = ((sRet[i] & 0xFF) << 8) | ((sRet[i] >> 8) & 0xFF);
	}
	return sRet;
}



// utf-32 大端序和小端序转换 ( 需使用 xrtFree 释放 )
XXAPI u32str xrtUTF32LEtoBE(u32str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return (u32str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u32len(sText); }
	if ( iSize == 0 ) { return (u32str)xCore.sNull; }
	u32str sRet;
	if ( bSrcRevise ) {
		sRet = sText;
	} else {
		sRet = xrtCopyStrU32(sText, iSize);
	}
	for ( int i = 0; i < iSize; i++ ) {
		sRet[i] = ((sRet[i] >> 24) & 0xFF) | ((sRet[i] >> 8) & 0xFF00) | ((sRet[i] << 8) & 0xFF0000) | ((sRet[i] << 24) & 0xFF000000);
	}
	return sRet;
}



// 任意编码转换 ( 需使用 xrtFree 释放 )
XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	// 非 windows 平台 ( 仅支持 utf-8、utf-16、utf-32 三种编码互相转换 [ 含 LE、BE 字节序 ]，oem 编码固定为 utf-8 )
	#if defined(_WIN32) || defined(_WIN64)
	#else
		if ( iInCP == XRT_CP_OEM ) { iInCP = XRT_CP_UTF8; }
		if ( iOutCP == XRT_CP_OEM ) { iOutCP = XRT_CP_UTF8; }
	#endif
	// 如果编码相同，返回副本
	if ( iInCP == iOutCP ) {
		if ( (iInCP == XRT_CP_UTF16) || (iInCP == XRT_CP_UTF16_BE) ) {
			return xrtCopyStrU16(sText, iSize);
		} else if ( (iInCP == XRT_CP_UTF32) || (iInCP == XRT_CP_UTF32_BE) ) {
			return xrtCopyStrU32(sText, iSize);
		} else {
			return xrtCopyStr(sText, iSize);
		}
	}
	// 需要转换编码 - 排列组合 20 种情况 ( 内置支持的编码转换组合 )
	if ( iInCP == XRT_CP_UTF8 ) {
		if ( iOutCP == XRT_CP_UTF16 ) {
			return xrtUTF8to16(sText, iSize);
		} else if ( iOutCP == XRT_CP_UTF32 ) {
			return xrtUTF8to32(sText, iSize);
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			u16str sRet = xrtUTF8to16(sText, iSize);
			xrtUTF16LEtoBE(sRet, xCore.iRet, TRUE);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			u32str sRet = xrtUTF8to32(sText, iSize);
			xrtUTF32LEtoBE(sRet, xCore.iRet, TRUE);
			return sRet;
		}
	} else if ( iInCP == XRT_CP_UTF16 ) {
		if ( iOutCP == XRT_CP_UTF8 ) {
			return xrtUTF16to8(sText, iSize);
		} else if ( iOutCP == XRT_CP_UTF32 ) {
			return xrtUTF16to32(sText, iSize);
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			return xrtUTF16LEtoBE(sText, iSize, FALSE);
		} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			u32str sRet = xrtUTF16to32(sText, iSize);
			xrtUTF32LEtoBE(sRet, xCore.iRet, TRUE);
			return sRet;
		}
	} else if ( iInCP == XRT_CP_UTF32 ) {
		if ( iOutCP == XRT_CP_UTF8 ) {
			return xrtUTF32to8(sText, iSize);
		} else if ( iOutCP == XRT_CP_UTF16 ) {
			return xrtUTF32to16(sText, iSize);
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			u16str sRet = xrtUTF32to16(sText, iSize);
			xrtUTF16LEtoBE(sRet, xCore.iRet, TRUE);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			return xrtUTF32LEtoBE(sText, iSize, FALSE);
		}
	} else if ( iInCP == XRT_CP_UTF16_BE ) {
		if ( iOutCP == XRT_CP_UTF8 ) {
			u16str sTemp = xrtUTF16LEtoBE(sText, iSize, FALSE);
			str sRet = xrtUTF16to8(sTemp, iSize);
			xrtFree(sTemp);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF32 ) {
			u16str sTemp = xrtUTF16LEtoBE(sText, iSize, FALSE);
			u32str sRet = xrtUTF16to32(sTemp, iSize);
			xrtFree(sTemp);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16 ) {
			return xrtUTF16LEtoBE(sText, iSize, FALSE);
		} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			u16str sTemp = xrtUTF16LEtoBE(sText, iSize, FALSE);
			u32str sRet = xrtUTF16to32(sTemp, iSize);
			xrtFree(sTemp);
			xrtUTF32LEtoBE(sRet, xCore.iRet, TRUE);
			return sRet;
		}
	} else if ( iInCP == XRT_CP_UTF32_BE ) {
		if ( iOutCP == XRT_CP_UTF8 ) {
			u32str sTemp = xrtUTF32LEtoBE(sText, iSize, FALSE);
			str sRet = xrtUTF32to8(sTemp, iSize);
			xrtFree(sTemp);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16 ) {
			u32str sTemp = xrtUTF32LEtoBE(sText, iSize, FALSE);
			u16str sRet = xrtUTF32to16(sTemp, iSize);
			xrtFree(sTemp);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			u32str sTemp = xrtUTF32LEtoBE(sText, iSize, FALSE);
			u16str sRet = xrtUTF32to16(sTemp, iSize);
			xrtFree(sTemp);
			xrtUTF16LEtoBE(sRet, xCore.iRet, TRUE);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF32 ) {
			return xrtUTF32LEtoBE(sText, iSize, FALSE);
		}
	}
	// 内置方案无法满足时的扩展方案
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案 - 调用 Win32SDK 转换 - 五种排列组合优化
		if ( iInCP == XRT_CP_UTF16 ) {
			// UTF16 转 多字节
			if ( iSize == 0 ) { iSize = u16len(sText); }
			if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
			size_t iRet = WideCharToMultiByte(iOutCP, 0, sText, iSize, NULL, 0, NULL, NULL);
			if ( iRet == 0 ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			str sRet = xrtMalloc(iRet + 1);
			if ( sRet == NULL ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			iRet = WideCharToMultiByte(iOutCP, 0, sText, iSize, sRet, iRet, NULL, NULL);
			sRet[iRet] = 0;
			return sRet;
		} else if ( iInCP == XRT_CP_UTF16_BE ) {
			// UTF16 BE 转 多字节
			if ( iSize == 0 ) { iSize = u16len(sText); }
			if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
			u16str sTemp = xrtUTF16LEtoBE(sText, iSize, FALSE);
			size_t iRet = WideCharToMultiByte(iOutCP, 0, sTemp, iSize, NULL, 0, NULL, NULL);
			if ( iRet == 0 ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			str sRet = xrtMalloc(iRet + 1);
			if ( sRet == NULL ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			iRet = WideCharToMultiByte(iOutCP, 0, sTemp, iSize, sRet, iRet, NULL, NULL);
			xrtFree(sTemp);
			sRet[iRet] = 0;
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16 ) {
			// 多字节 转 UTF16
			if ( iSize == 0 ) { iSize = strlen(sText); }
			if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
			size_t iRet = MultiByteToWideChar(iInCP, 0, sText, iSize, NULL, 0);
			if ( iRet == 0 ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			u16str sRet = xrtMalloc((iRet + 1) * sizeof(unsigned short));
			if ( sRet == NULL ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			iRet = MultiByteToWideChar(iInCP, 0, sText, iSize, sRet, iRet);
			sRet[iRet] = 0;
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			// 多字节 转 UTF16 BE
			if ( iSize == 0 ) { iSize = strlen(sText); }
			if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
			size_t iRet = MultiByteToWideChar(iInCP, 0, sText, iSize, NULL, 0);
			if ( iRet == 0 ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			u16str sRet = xrtMalloc((iRet + 1) * sizeof(unsigned short));
			if ( sRet == NULL ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			iRet = MultiByteToWideChar(iInCP, 0, sText, iSize, sRet, iRet);
			sRet[iRet] = 0;
			xrtUTF16LEtoBE(sRet, iRet, TRUE);
			return sRet;
		} else {
			// 多字节 转 多字节
			if ( iSize == 0 ) { iSize = strlen(sText); }
			if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
			// 先转换为 utf16
			size_t iRetW = MultiByteToWideChar(iInCP, 0, sText, iSize, NULL, 0);
			if ( iRetW == 0 ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			u16str sRetW = xrtMalloc((iRetW + 1) * sizeof(unsigned short));
			if ( sRetW == NULL ) {
				xCore.iRet = 0;
				return xCore.sNull;
			}
			iRetW = MultiByteToWideChar(iInCP, 0, sText, iSize, sRetW, iRetW);
			sRetW[iRetW] = 0;
			// 再转换为最终编码
			size_t iRet = WideCharToMultiByte(iOutCP, 0, sRetW, iRetW, NULL, 0, NULL, NULL);
			if ( iRet == 0 ) {
				xCore.iRet = 0;
				xrtFree(sRetW);
				return xCore.sNull;
			}
			str sRet = xrtMalloc(iRet + 1);
			if ( sRet == NULL ) {
				xCore.iRet = 0;
				xrtFree(sRetW);
				return xCore.sNull;
			}
			iRet = WideCharToMultiByte(iOutCP, 0, sRetW, iRetW, sRet, iRet, NULL, NULL);
			xrtFree(sRetW);
			sRet[iRet] = 0;
			return sRet;
		}
	#else
		// 其他平台方案 - 暂无 ( 可使用 libiconv 等库，但是太大了 )
	#endif
	// 无法处理的编码转换组合
	xrtSetError("Unsupported charset !", FALSE);
	xCore.iRet = 0;
	return xCore.sNull;
}



// 是否为 utf-8 字符串
XXAPI bool xrtIsUTF8(str sText, size_t iSize)
{
	// NULL 返回 FALSE，空字符串返回 TRUE
	if ( sText == NULL ) { return FALSE; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return TRUE; }
	// 检测是否符合标准
	for ( int i = 0; i < iSize; i++ ) {
		// 遇到 \0、FE、FF 直接返回 FALSE
		if ( (sText[i] == 0) || (sText[i] == 0xFE) || (sText[i] == 0xFF) ) {
			return FALSE;
		}
		// 检查多字节字符是否已 0b10 开头
		char iExtraBytes = BytesExtraTableUTF8[sText[i]];
		if ( iExtraBytes ) {
			for ( int j = 0; (j < iExtraBytes) && (i < iSize); j++ ) {
				if ( (sText[++i] & 0b11000000) != 0b10000000 ) {
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}



// 猜测编码 ( 先判断 BOM，再判断是否为合法的 utf8 编码，再根据 \0 的长度推测是否为 utf32 或 utf16、OEM，猜测不出来时返回 binary )
XXAPI int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM)
{
	if ( sText == NULL ) { return XRT_CP_BINARY; }
	if ( iSize == 0 ) { return XRT_CP_BINARY; }
	u8str sPtr = sText;
	int bNoUTF8 = FALSE;		// 是否不符合 UTF8 标准
	int bNoUTF16LE = FALSE;		// 是否不符合 UTF16 标准
	int bNoUTF16BE = FALSE;		// 是否不符合 UTF16 BE 标准
	int bNoUTF32LE = FALSE;		// 是否不符合 UTF32 标准
	int bNoUTF32BE = FALSE;		// 是否不符合 UTF32 BE 标准
	int iNullBE = 0;			// UTF16 大端序方向 \0 的数量
	int iNullLE = 0;			// UTF16 小端序方向 \0 的数量
	int iNullSize = 0;			// 遇到连续 \0 的次数
	int iMaxNull = 0;			// 最多连续 \0 的数量
	// 通过 BOM 判断字符串编码
	if ( bBOM ) {
		if ( iSize >= 3 ) {
			if ( (sPtr[0] == 0xEF) && (sPtr[1] == 0xBB) && (sPtr[2] == 0xBF) ) {
				return XRT_CP_UTF8 | XRT_CP_BOM;
			}
		}
		if ( iSize >= 4 ) {
			if ( (sPtr[0] == 0xFF) && (sPtr[1] == 0xFE) && (sPtr[2] == 0x00) && (sPtr[3] == 0x00) ) {
				return XRT_CP_UTF32 | XRT_CP_BOM;
			}
			if ( (sPtr[0] == 0x00) && (sPtr[1] == 0x00) && (sPtr[2] == 0xFE) && (sPtr[3] == 0xFF) ) {
				return XRT_CP_UTF32_BE | XRT_CP_BOM;
			}
		}
		if ( iSize >= 2 ) {
			if ( (sPtr[0] == 0xFF) && (sPtr[1] == 0xFE) ) {
				return XRT_CP_UTF16 | XRT_CP_BOM;
			}
			if ( (sPtr[0] == 0xFE) && (sPtr[1] == 0xFF) ) {
				return XRT_CP_UTF16_BE | XRT_CP_BOM;
			}
		}
	}
	// 开始推测字符串编码
	for ( int i = 0; i < iSize; i++ ) {
		// 检测 utf-8 不可能出现的字符
		if ( (sPtr[i] == 0xFE) || (sPtr[i] == 0xFF) ) {
			bNoUTF8 = TRUE;
		}
		// 检测 utf-8 多字符编码是否正确
		if ( bNoUTF8 == FALSE ) {
			char iExtraBytes = BytesExtraTableUTF8[sPtr[i]];
			for ( int j = 1; (j <= iExtraBytes) && ((i + j) < iSize); j++ ) {
				if ( (sPtr[i + j] & 0b11000000) != 0b10000000 ) {
					bNoUTF8 = TRUE;
					break;
				}
			}
		}
		// 检测 utf-16 代理区是否合规
		if ( (i & 1) == 0 ) {
			if ( (i + 2) < iSize ) {
				if ( bNoUTF16BE == FALSE ) {
					if ( (sPtr[i] & 0b11111100) == 0b11011000 ) {
						if ( (sPtr[i + 2] & 0b11111100) != 0b11011100 ) {
							bNoUTF16BE = TRUE;
						}
					}
				}
				if ( bNoUTF16LE == FALSE ) {
					if ( (sPtr[i] & 0b11111100) == 0b11011100 ) {
						if ( (sPtr[i + 2] & 0b11111100) != 0b11011000 ) {
							bNoUTF16LE = TRUE;
						}
					}
				}
			}
		}
		// 检测是否符合 utf-32 范围 ( 0x10FFFF 以内‌ )
		if ( (i & 3) == 0 ) {
			if ( (i + 3) < iSize ) {
				if ( bNoUTF32BE == FALSE ) {
					uint32 c = (sPtr[i] << 24) | (sPtr[i + 1] << 16) | (sPtr[i + 2] << 8) | sPtr[i + 3];
					if ( c > 0x10FFFF ) {
						bNoUTF32BE = TRUE;
					}
				}
				if ( bNoUTF32LE == FALSE ) {
					uint32 c = (sPtr[i + 3] << 24) | (sPtr[i + 2] << 16) | (sPtr[i + 1] << 8) | sPtr[i];
					if ( c > 0x10FFFF ) {
						bNoUTF32LE = TRUE;
					}
				}
			}
		}
		// 统计连续 \0 长度
		if ( sPtr[i] == 0 ) {
			iNullSize++;
			if ( i & 1 ) {
				iNullLE++;		// LE 英文字符特征 : X0
			} else {
				iNullBE++;		// BE 英文字符特征 : 0X
			}
		} else {
			if ( iNullSize > iMaxNull ) {
				iMaxNull = iNullSize;
			}
			iNullSize = 0;
		}
	}
	// 根据条件推测编码
	if ( iMaxNull > 3 ) {
		return XRT_CP_BINARY;
	} else if ( iMaxNull > 1 ) {
		if ( bNoUTF32LE == FALSE ) {
			return XRT_CP_UTF32;
		} else if ( bNoUTF32BE == FALSE ) {
			return XRT_CP_UTF32_BE;
		} else {
			return XRT_CP_BINARY;
		}
	} else if ( iMaxNull == 1 ) {
		if ( bNoUTF16LE == FALSE ) {
			if ( bNoUTF16BE == FALSE ) {
				// 当无法区分到底是 utf16 BE 还是 LE 时，根据 \0 出现较多的方向判断
				if ( iNullBE > iNullLE ) {
					return XRT_CP_UTF16_BE;
				} else {
					return XRT_CP_UTF16;
				}
			}
			return XRT_CP_UTF16;
		} else if ( bNoUTF16BE == FALSE ) {
			return XRT_CP_UTF16_BE;
		} else if ( bNoUTF32LE == FALSE ) {
			return XRT_CP_UTF32;
		} else if ( bNoUTF32BE == FALSE ) {
			return XRT_CP_UTF32_BE;
		} else {
			return XRT_CP_BINARY;
		}
	} else {
		if ( bNoUTF8 == FALSE ) {
			return XRT_CP_UTF8;
		} else {
			#if defined(_WIN32) || defined(_WIN64)
				return XRT_CP_OEM;
			#else
				return XRT_CP_BINARY;
			#endif
		}
	}
}



// 获取不同字符集的字符大小
XXAPI int xrtGetCharSize(int iCP)
{
	iCP &= XRT_MASK_BOM;
	if ( (iCP == XRT_CP_UTF16) || (iCP == XRT_CP_UTF16_BE) ) {
		return 2;
	} else if ( (iCP == XRT_CP_UTF32) || (iCP == XRT_CP_UTF32_BE) ) {
		return 4;
	} else {
		return 1;
	}
}


