


/*
	
	MIT License
	
	Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
	
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
	
*/



#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>



#ifndef XXRTL_CORE
	#define XXRTL_CORE
	
	
	
	// basic type define
	typedef unsigned char* u8str;
	typedef unsigned short* u16str;
	typedef unsigned int* u32str;
	typedef unsigned char* binary;
	typedef u8str str;
	
	typedef char i8;
	typedef char int8;
	typedef unsigned char u8;
	typedef unsigned char uint8;
	typedef short i16;
	typedef short int16;
	typedef unsigned short u16;
	typedef unsigned short uint16;
	typedef unsigned int uint;
	typedef int i32;
	typedef int int32;
	typedef unsigned int u32;
	typedef unsigned int uint32;
	typedef long long i64;
	typedef long long int64;
	typedef unsigned long long u64;
	typedef unsigned long long uint64;
	// long = auto 32 / 64 bit integer
	typedef unsigned long ulong;
	
	typedef float f32;
	typedef float float32;
	typedef double f64;
	typedef double float64;
	
	typedef long long curr;
	
	typedef void* ptr;
	typedef intptr_t intptr;
	typedef uintptr_t uintptr;
	
	typedef int64 xtime;
	
	/*
	#ifndef bool
		typedef int bool;
	#endif
	#ifndef true
		#define true -1
	#endif
	#ifndef false
		#define false 0
	#endif
	*/
	#ifndef TRUE
		#define TRUE 1
	#endif
	#ifndef FALSE
		#define FALSE 0
	#endif
	#ifndef null
		#define null 0
	#endif
	
	#ifdef BUILD_DLL
		#define XXAPI	__declspec(dllexport)
	#else
		#define XXAPI
	#endif
	
	
	
	/* ------------------------------------ 线程局部存储 (TLS) ------------------------------------ */
	
	// TLS 关键字定义 ( 跨平台支持 )
	#if defined(_WIN32) || defined(_WIN64)
		#if defined(__TINYC__) || defined(BUILD_DLL)
			// TCC 不支持 __declspec(thread)
			// Windows DLL 不支持静态 TLS (__thread / __declspec(thread))，必须使用 Windows TLS API
			#define XRT_USE_WIN_TLS_API 1
		#else
			#define XRT_TLS __declspec(thread)
		#endif
	#else
		#define XRT_TLS __thread
	#endif
	
	// PCG 随机数状态结构 ( 用于 TLS )
	typedef struct {
		uint64 state;
		uint64 inc;
	} xrt_pcg32_t;
	
	// 线程局部数据结构 ( 存储每个线程独立的数据 )
	typedef struct {
		
		// 初始化标志
		int bInit;
		
		// 临时性返回数据 (可以改变，用于多个返回值的情况做临时存储)
		str sRet;
		int64 iRet;
		double nRet;
		
		// 错误信息
		str LastError;
		int __pri_FreeError;
		
		// 环形临时内存（固定 32 个临时内存循环使用和释放）
		ptr TempMem[32];
		uint32 TempMemIdx;
		
		// 随机数生成器状态 ( 每个线程独立 )
		xrt_pcg32_t Rand32;
		xrt_pcg32_t Rand64Low;
		xrt_pcg32_t Rand64High;
		
	} xrtThreadLocal;
	
	// 获取当前线程的 TLS 数据
	XXAPI xrtThreadLocal* xrtGetTLS();
	
	// 初始化当前线程的 TLS ( 主线程在 xrtInit 中自动调用 )
	XXAPI void xrtInitTLS();
	
	// 释放当前线程的 TLS ( 主线程在 xrtUnit 中自动调用 )
	XXAPI void xrtUnitTLS();
	
	
	
	/* ------------------------------------ 全局数据 ------------------------------------ */
	
	// 全局
	typedef struct {
		
		// 是否已经初始化过
		int bInit;
		
		// 全局数据 (不可改变)
		str sNull;
		
		// [已废弃 - 保留兼容] 临时性全局数据，请使用 xrtGetTLS() 获取线程安全版本
		str sRet;
		int64 iRet;
		double nRet;
		
		// [已废弃 - 保留兼容] 错误信息，请使用 xrtGetTLS() 获取线程安全版本
		str LastError;
		int __pri_FreeError;
		void (*OnError)(str sError);
		
		// 高精度时钟频率单位
		#if defined(_WIN32) || defined(_WIN64)
			uint64 Frequency;
		#endif
		
		// 本机 IP 地址 ( 用于生成 XID )
		uint LocalAddr;
		
		// 应用信息
		str AppFile;
		str AppPath;
		
		// [已废弃 - 保留兼容] 环形临时内存，请使用 xrtGetTLS() 获取线程安全版本
		ptr TempMem[32];
		uint32 TempMemIdx;
		
		// 内存函数
		ptr (*malloc)(size_t iSize);
		ptr (*calloc)(size_t iNum, size_t iSize);
		ptr (*realloc)(ptr pMem, size_t iSize);
		void (*free)(ptr pMem);
		
	} xrtGlobalData;
	
	// 全局数据
	XXAPI extern xrtGlobalData xCore;
	
	
	
	// 初始化 xCore
	XXAPI xrtGlobalData* xrtInit();
	
	// 释放 xCore
	XXAPI void xrtUnit();
	
	
	
	/* ------------------------------------ 基础功能补充 ------------------------------------ */
	
	// 内存查找
	#if defined(_WIN32) || defined(_WIN64)
		XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize);
	#endif
	
	// 获取字符串长度 ( 补充 utf16 和 utf32 支持 )
	XXAPI size_t u16len(u16str sText);
	XXAPI size_t u32len(u32str sText);
	
	
	
	/* ------------------------------------ Base 函数库 ------------------------------------ */
	
	// 申请内存
	XXAPI ptr xrtMalloc(size_t iSize);
	
	// 申请类内存
	XXAPI ptr xrtCalloc(size_t iNum, size_t iSize);
	
	// 重新申请内存
	XXAPI ptr xrtRealloc(ptr pMem, size_t iSize);
	
	// 释放内存（ 会先判断是否为 null ）
	XXAPI void xrtFree(ptr pmem);
	
	// 申请无需主动释放的临时内存
	XXAPI ptr xrtTempMemory(size_t iSize);
	
	// 释放所有临时内存
	XXAPI void xrtFreeTempMemory();
	
	// 设置错误
	XXAPI void xrtSetError(str sError, bool bFree);
	XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree);
	XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree);
	
	// 清除错误
	XXAPI void xrtClearError();
	
	
	
	/* ------------------------------------ Charset 函数库 ------------------------------------ */
	
	#define XRT_CP_AUTO				-2				// 自动识别字符集（ 可自动识别是否为 utf8，自动识别失败则使用 XRT_CP_BINARY ）
	#define XRT_CP_BINARY			-1				// 二进制文件
	#define XRT_CP_OEM				0				// 本机 OEM 字符集 ( windows为OEM多字节编码，linux固定为utf8 )
	#define XRT_CP_UTF8				65001			// UTF8
	#define XRT_CP_UTF16			1200			// UTF16
	#define XRT_CP_UTF16_BE			1201			// UTF16 big-endian
	#define XRT_CP_UTF32			65005			// UTF32
	#define XRT_CP_UTF32_BE			65006			// UTF32 big-endian
	
	#define XRT_CP_BOM				0x40000000		// with BOM
	#define XRT_MASK_BOM			0xBFFFFFFF		// mask BOM
	
	// utf-8 转 utf-16（ 需使用 xrtFree 释放 ）
	XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize);
	
	// utf-8 转 utf-32（ 需使用 xrtFree 释放 ）
	XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize);
	
	// utf-16 转 utf-8（ 需使用 xrtFree 释放 ）
	XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize);
	
	// utf-16 转 utf-32（ 需使用 xrtFree 释放 ）
	XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize);
	
	// utf-32 转 utf-8（ 需使用 xrtFree 释放 ）
	XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize);
	
	// utf-32 转 utf-16（ 需使用 xrtFree 释放 ）
	XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize);
	
	// utf-16 大端序和小端序转换 ( 需使用 xrtFree 释放 )
	XXAPI u16str xrtUTF16LEtoBE(u16str sText, size_t iSize, bool bSrcRevise);
	
	// utf-32 大端序和小端序转换 ( 需使用 xrtFree 释放 )
	XXAPI u32str xrtUTF32LEtoBE(u32str sText, size_t iSize, bool bSrcRevise);
	
	// 任意编码转换 ( 需使用 xrtFree 释放 )
	XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);
	
	// 是否为 utf-8 字符串
	XXAPI bool xrtIsUTF8(str sText, size_t iSize);
	
	// 猜测编码 ( 先判断 BOM，再判断是否为合法的 utf8 编码，再根据 \0 的长度推测是否为 utf32 或 utf16、OEM，猜测不出来时返回 binary )
	XXAPI int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);
	
	// 获取不同字符集的字符大小
	XXAPI int xrtGetCharSize(int iCP);
	
	
	
	/* ------------------------------------ OS 函数库 ------------------------------------ */
	
	// 运行程序
	XXAPI ptr xrtRun(str sPath, size_t iSize);
	
	// 打开文件（ 仅支持 windows 系统 ）
	XXAPI ptr xrtStart(str sPath, size_t iSize);
	
	// 运行程序并等待程序运行结束
	XXAPI int xrtChain(str sPath, size_t iSize);
	
	
	
	/* ------------------------------------ Math 函数库 ------------------------------------ */
	
	// 获取 32 位随机数
	XXAPI uint32 xrtRand32();
	
	// 设置 32 位随机数种子
	XXAPI void xrtSetRandSeed32(uint64 seed, uint64 seq);
	
	// 获取 64 位随机数
	XXAPI uint64 xrtRand64();
	
	// 设置 64 位随机数种子
	XXAPI void xrtSetRandSeed64(uint64 lowseed, uint64 lowseq, uint64 highseed, uint64 highseq);
	
	// 获取 32 位范围随机数
	XXAPI int xrtRandRange(int min, int max);
	
	
	
	/* ------------------------------------ String 函数库 ------------------------------------ */
	
	// 创建字符串副本（ 需使用 xrtFree 释放 ）
	XXAPI str xrtCopyStr(str sText, size_t iSize);
	XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
	XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
	XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize);
	
	// 比较字符串
	XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase);
	
	// 字符串转为小写（ bSrcRevise 为 false 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise);
	
	// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise);
	
	// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）
	XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	
	// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
	XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
	
	// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
	XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
	XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
	
	// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtFilterStr(str sText, size_t iSize, str sFilter, size_t iSubSize, bool bSrcRevise);
	
	// 字符串格式化（ 需使用 xrtFree 释放 ）
	XXAPI str xrtFormat(str sFormat, ...);
	
	// 字符串替换（ 需使用 xrtFree 释放 ）
	XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
	
	// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
	XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
	
	// 生成随机字符串（ 需使用 xrtFree 释放 ）
	XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen);
	
	// HEX 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtHexEncode(ptr pMem, size_t iSize);
	
	// HEX 解码（ 需使用 xrtFree 释放 ）
	XXAPI ptr xrtHexDecode(str pText, size_t iSize);
	
	// Base64 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);
	
	// Base64 解码（ 需使用 xrtFree 释放 ）
	XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable);
	
	
	
	/* ------------------------------------ Path 函数库 ------------------------------------ */
	
	// 通过路径获取文件名 + 扩展名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetNameExt(str sPath, size_t iSize);
	
	// 通过路径获取文件名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetName(str sPath, size_t iSize);
	
	// 通过路径获取扩展名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetExt(str sPath, size_t iSize);
	
	// 通过路径获取文件夹（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetDir(str sPath, size_t iSize);
	
	// 判断是否为绝对路径（Linux 系统以 / 开头为绝对路径，Windows系统含 : 为绝对路径）
	XXAPI bool xrtPathIsAbs(str sPath, size_t iSize);
	
	// 获取随机不存在的路径（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathRandom(str sHead, size_t iHeadSize, str sFoot, size_t iFootSize, size_t iLen);
	
	// 拼接路径（ 需要使用 xrtFree 释放内存 ）
	XXAPI str xrtPathJoin(uint iCount, ...);
	
	
	
	/* ------------------------------------ Time 函数库 ------------------------------------ */
	
	// 各种固定时间单位的数值
	#define XRT_TIME_MINUTE			60
	#define XRT_TIME_HOUR			3600
	#define XRT_TIME_DAY			86400
	#define XRT_TIME_YEAR			31536000
	#define XRT_TIME_LEAPYEAR		31622400
	#define XRT_TIME_400YEAR		12622780800				// 每隔 400 年有 97 个闰年 + 303 个平年
	#define XRT_TIME_19700101		62167219200
	
	// 时间单位
	#define XRT_TIME_INTERVAL_YEAR			1				// 年
	#define XRT_TIME_INTERVAL_MONTH			2				// 月
	#define XRT_TIME_INTERVAL_DAY			3				// 日
	#define XRT_TIME_INTERVAL_HOUR			4				// 时
	#define XRT_TIME_INTERVAL_MINUTE		5				// 分
	#define XRT_TIME_INTERVAL_SECOND		6				// 秒
	#define XRT_TIME_INTERVAL_WEEKDAY		7				// 星期
	#define XRT_TIME_INTERVAL_QUARTER		8				// 季度
	
	// 转换格式
	#define XRT_TIME_FORMAT_DATETIME		0
	#define XRT_TIME_FORMAT_DATE			1
	#define XRT_TIME_FORMAT_TIME			2
	
	// 获取高精度时钟 Tick ( 返回秒数，便于计算时间间隔 )
	XXAPI double xrtTimer();
	
	// 毫秒级延时
	XXAPI void xrtSleep(uint32 ms);
	
	// 判断是否为闰年
	XXAPI bool xrtIsLeapYear(int iYear);
	
	// 获取某年某月有多少天
	XXAPI int xrtDaysInMonth(int iYear, int iMonth);
	
	// 获取某年有多少天
	XXAPI int xrtDaysInYear(int iYear);
	
	// 构建时间
	XXAPI xtime xrtTimeSerial(int iHour, int iMinute, int iSecond);
	
	// 构建日期
	XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
	
	// 构建日期 + 时间
	XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
	
	// 获取时间中的秒
	XXAPI int xrtSecond(xtime iTime);
	
	// 获取时间中的分钟
	XXAPI int xrtMinute(xtime iTime);
	
	// 获取时间中的小时
	XXAPI int xrtHour(xtime iTime);
	
	// 获取时间中的日期
	XXAPI int xrtDay(xtime iTime);
	
	// 获取时间中的月份
	XXAPI int xrtMonth(xtime iTime);
	
	// 获取时间中的年份
	XXAPI int64 xrtYear(xtime iTime);
	
	// 获取时间中的星期
	XXAPI int xrtWeekday(xtime iTime);
	
	// 获取时间是当年的第几天
	XXAPI int xrtDayOfYear(xtime iTime);
	
	// 解码时间
	XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, int* pHour, int* pMinute, int* pSecond, int* pWeekday, int* pDayOfYear);
	
	// 获取当前日期 + 时间
	XXAPI xtime xrtNow();
	
	// 获取当前日期
	XXAPI xtime xrtDate();
	
	// 获取当前时间
	XXAPI xtime xrtTime();
	
	// 获取字符串格式的当前日期 + 时间（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtNowStr();
	
	// 获取字符串格式的当前日期（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtDateStr();
	
	// 获取字符串格式的当前时间（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtTimeStr();
	
	// 转换日期 + 时间为字符串（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtTimeToStr(xtime iTime, int iFormat);
	
	// 时间单位累加
	XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
	
	// 单位时间差计算（ 不支持 XRT_TIME_INTERVAL_WEEKDAY ）
	XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
	
	
	
	/* ------------------------------------ File 函数库 ------------------------------------ */
	
	// 文件对象
	typedef struct {
		union {
			ptr obj;			// windows 文件对象
			int idx;			// linux 文件句柄
		};
		int Charset;			// 文件字符集
		uint BOM;				// BOM大小
	} xfile_struct, *xfile;
	
	// 游标控制
	#define XRT_SEEK_SET		0
	#define XRT_SEEK_CUR		1
	#define XRT_SEEK_END		2
	
	// 打开文件 ( 需要使用 xrtClose 关闭文件 )
	XXAPI xfile xrtOpen(str sPath, int bReadOnly, int iCharset);
	
	// 关闭文件
	XXAPI void xrtClose(xfile objFile);
	
	// 设置游标位置
	XXAPI size_t xrtSeek(xfile objFile, long iOffset, int iMoveMethod);
	
	// 获取游标位置
	XXAPI size_t xrtTell(xfile objFile);
	
	// 获取文件末尾位置 ( 获取一打开文件的动态大小 )
	XXAPI size_t xrtGetEOF(xfile objFile);
	
	// 是否已经读取到文件末尾
	XXAPI bool xrtEOF(xfile objFile);
	
	// 设置文件末尾
	XXAPI bool xrtSetEOF(xfile objFile);
	
	// 从已打开的文件读取数据 ( iSize 为要读取的字节数，需要使用 xrtFree 释放内存 )
	XXAPI str xrtRead(xfile objFile, size_t iSize);
	
	// 向已打开的文件写入数据 ( iSize 为要写入的字节数 )
	XXAPI size_t xrtWrite(xfile objFile, str sText, size_t iSize);
	
	// 从已打开的文件读取二进制数据 ( 需要使用 xrtFree 释放内存 )
	XXAPI ptr xrtGet(xfile objFile, size_t iSize);
	
	// 向已打开的文件写入二进制数据
	XXAPI int xrtPut(xfile objFile, ptr pBuff, size_t iSize);
	
	// 向文件追加写入数据
	XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset);
	
	// 写入并覆盖文件内容
	XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
	
	// 读取文件的全部内容 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtFileReadAll(str sPath, int iCharset);
	
	// 写入并覆盖文件内容 ( 二进制 )
	XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize);
	
	// 读取文件的全部内容 ( 二进制，需要使用 xrtFree 释放内存 )
	XXAPI ptr xrtFileGetAll(str sPath);
	
	// 判断路径是否存在
	XXAPI bool xrtPathExists(str sPath);
	
	// 判断文件是否存在
	XXAPI bool xrtFileExists(str sPath);
	
	// 判断目录是否存在
	XXAPI bool xrtDirExists(str sPath);
	
	// 获取文件长度
	XXAPI size_t xrtFileGetSize(str sPath);
	
	// 设置文件长度
	XXAPI bool xrtFileSetSize(str sPath, size_t iSize);
	
	// 获取文件属性
	XXAPI int xrtFileGetAttr(str sPath);
	
	// 设置文件属性
	XXAPI bool xrtFileSetAttr(str sPath, int iAttr);
	
	// 获取文件最后一次访问时间
	XXAPI int64 xrtFileGetAccessTime(str sPath);
	
	// 获取文件修改时间
	XXAPI int64 xrtFileGetChangeTime(str sPath);
	
	// 复制文件
	XXAPI bool xrtFileCopy(str sSrc, str sDst, bool bReWrite);
	
	// 移动文件（重命名）
	XXAPI bool xrtFileMove(str sSrc, str sDst, bool bReWrite);
	
	// 删除文件
	XXAPI bool xrtFileDelete(str sPath);
	
	// 扫描文件夹 ( 返回文件数量 )
	XXAPI int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
	
	// 创建文件夹
	XXAPI bool xrtDirCreate(str sPath);
	
	// 创建多级文件夹
	XXAPI bool xrtDirCreateAll(str sPath);
	
	// 复制文件夹 ( 返回操作的文件数量 )
	XXAPI int xrtDirCopy(str sSrc, str sDst, bool bReWrite);
	
	// 移动文件夹 ( 返回操作的文件数量 )
	XXAPI int xrtDirMove(str sSrc, str sDst, bool bReWrite);
	
	// 删除文件夹 ( 返回操作的文件数量 )
	XXAPI int xrtDirDelete(str sPath);
	
	
	
	/* ------------------------------------ Thread 函数库 ------------------------------------ */
	
	// 线程状态
	#define XRT_THREAD_STOPPED		0			// 已停止
	#define XRT_THREAD_RUNNING		1			// 运行中
	#define XRT_THREAD_SUSPENDED	2			// 已挂起
	
	// 等待超时返回值
	#define XRT_WAIT_OK				0			// 等待成功
	#define XRT_WAIT_TIMEOUT		1			// 等待超时
	#define XRT_WAIT_ERROR			-1			// 等待错误
	
	// 线程数据结构
	typedef struct {
		ptr Handle;								// 线程句柄
		uint32 TID;								// 线程ID
		uint32 (*Proc)(ptr param);				// 用户回调函数
		ptr Param;								// 用户参数
		volatile int StopFlag;					// 停止信号标志
	} xthread_struct, *xthread;
	
	// 互斥体数据结构
	typedef struct {
		ptr Handle;								// 互斥体句柄
	} xmutex_struct, *xmutex;
	
	// 信号量数据结构
	typedef struct {
		ptr Handle;								// 信号量句柄
	} xsem_struct, *xsem;
	
	// 条件变量数据结构
	typedef struct {
		ptr Handle;								// 条件变量句柄
	} xcond_struct, *xcond;
	
	/* ---------- 线程管理 ---------- */
	
	// 创建线程
	XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
	
	// 销毁线程对象（不终止线程，仅释放管理结构）
	XXAPI void xrtThreadDestroy(xthread pThread);
	
	// 等待线程结束
	XXAPI void xrtThreadWait(xthread pThread);
	
	// 等待线程结束（带超时，毫秒）
	XXAPI int xrtThreadWaitTimeout(xthread pThread, uint32 iTimeout);
	
	// 发送停止信号（线程需要主动检查 xrtThreadShouldStop）
	XXAPI void xrtThreadStop(xthread pThread);
	
	// 检查是否应该停止（线程内调用）
	XXAPI bool xrtThreadShouldStop(xthread pThread);
	
	// 强制终止线程（危险操作，可能导致资源泄漏）
	XXAPI bool xrtThreadKill(xthread pThread);
	
	// 挂起线程
	XXAPI bool xrtThreadSuspend(xthread pThread);
	
	// 恢复线程
	XXAPI bool xrtThreadResume(xthread pThread);
	
	// 获取线程状态
	XXAPI int xrtThreadGetState(xthread pThread);
	
	// 获取线程退出码
	XXAPI uint32 xrtThreadGetExitCode(xthread pThread);
	
	// 获取当前线程ID
	XXAPI uint32 xrtThreadGetCurrentId();
	
	// 让出当前线程的时间片
	XXAPI void xrtThreadYield();
	
	/* ---------- 互斥体 ---------- */
	
	// 创建互斥体
	XXAPI xmutex xrtMutexCreate();
	
	// 销毁互斥体
	XXAPI void xrtMutexDestroy(xmutex pMutex);
	
	// 初始化互斥体（对自维护结构体指针使用）
	XXAPI void xrtMutexInit(xmutex pMutex);
	
	// 释放互斥体（对自维护结构体指针使用）
	XXAPI void xrtMutexUnit(xmutex pMutex);
	
	// 锁定互斥体（阻塞）
	XXAPI void xrtMutexLock(xmutex pMutex);
	
	// 尝试锁定互斥体（非阻塞）
	XXAPI bool xrtMutexTryLock(xmutex pMutex);
	
	// 解锁互斥体
	XXAPI void xrtMutexUnlock(xmutex pMutex);
	
	/* ---------- 信号量 ---------- */
	
	// 创建信号量
	XXAPI xsem xrtSemCreate(uint32 iInitValue, uint32 iMaxValue);
	
	// 销毁信号量
	XXAPI void xrtSemDestroy(xsem pSem);
	
	// 初始化信号量（对自维护结构体指针使用）
	XXAPI void xrtSemInit(xsem pSem, uint32 iInitValue, uint32 iMaxValue);
	
	// 释放信号量（对自维护结构体指针使用）
	XXAPI void xrtSemUnit(xsem pSem);
	
	// 等待信号量（阻塞，计数减1）
	XXAPI void xrtSemWait(xsem pSem);
	
	// 尝试等待信号量（非阻塞）
	XXAPI bool xrtSemTryWait(xsem pSem);
	
	// 等待信号量（带超时，毫秒）
	XXAPI int xrtSemWaitTimeout(xsem pSem, uint32 iTimeout);
	
	// 释放信号量（计数加1）
	XXAPI bool xrtSemPost(xsem pSem);
	
	// 释放信号量（计数加N）
	XXAPI bool xrtSemPostMultiple(xsem pSem, uint32 iCount);
	
	/* ---------- 条件变量 ---------- */
	
	// 创建条件变量
	XXAPI xcond xrtCondCreate();
	
	// 销毁条件变量
	XXAPI void xrtCondDestroy(xcond pCond);
	
	// 初始化条件变量（对自维护结构体指针使用）
	XXAPI void xrtCondInit(xcond pCond);
	
	// 释放条件变量（对自维护结构体指针使用）
	XXAPI void xrtCondUnit(xcond pCond);
	
	// 等待条件变量（需要先锁定互斥体）
	XXAPI void xrtCondWait(xcond pCond, xmutex pMutex);
	
	// 等待条件变量（带超时，毫秒）
	XXAPI int xrtCondWaitTimeout(xcond pCond, xmutex pMutex, uint32 iTimeout);
	
	// 唤醒一个等待的线程
	XXAPI void xrtCondSignal(xcond pCond);
	
	// 唤醒所有等待的线程
	XXAPI void xrtCondBroadcast(xcond pCond);
	
	
	
	/* ------------------------------------ Hash 函数库 ------------------------------------ */
	
	/*
		Hash32 - nmhash32x [Ver2.0, Update : 2024/10/18 from https://github.com/rurban/smhasher]
			使用协议注意事项：
				BSD 2-Clause 协议
				允许个人使用、商业使用
				复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
	*/
	
	// 默认 seed
	#define HASH32_SEED		0
	
	// 计算 32 位哈希值
	XXAPI uint32 xrtHash32_WithSeed(ptr key, size_t len, uint32 seed);
	XXAPI uint32 xrtHash32(ptr key, size_t len);
	
	/*
		Hash64 - rapidhash [Ver1.0, Update : 2024/10/18 from https://github.com/Nicoshev/rapidhash]
			使用协议注意事项：
				BSD 2-Clause 协议
				允许个人使用、商业使用
				复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
	*/
	
	// 默认 seed
	#define HASH64_SEED		(0xbdd89aa982704029ull)
	
	// 计算 64 位哈希值
	XXAPI uint64 xrtHash64_WithSeed(ptr key, size_t len, uint64 seed);
	XXAPI uint64 xrtHash64(ptr key, size_t len);
	XXAPI uint64 xrtHash64_Micro_WithSeed(ptr key, size_t len, uint64 seed);
	XXAPI uint64 xrtHash64_Micro(ptr key, size_t len);
	XXAPI uint64 xrtHash64_Nano_WithSeed(ptr key, size_t len, uint64 seed);
	XXAPI uint64 xrtHash64_Nano(ptr key, size_t len);
	
	
	
	/* ------------------------------------ Network 函数库 ------------------------------------ */
	
	// 获取本机 IP ( 需使用 xrtFree 释放 )
	str xrtGetLocalIP();
	
	// 获取本机 IP ( 返回 uint32 )
	uint32 xrtGetLocalRawIP();
	
	// 获取本机 MAC 地址 ( 需使用 xrtFree 释放 )
	str xrtGetLocalMAC();
	
	// 获取本机名称 ( 需使用 xrtFree 释放 )
	str xrtGetLocalName();
	
	
	
	/* ------------------------------------ XID 函数库 ------------------------------------ */
	
	// XID 数据结构 ( 192 bit )
	typedef struct {
		xtime Time;				// 当前时间戳
		int32 Addr;				// 本机 IP 地址
		int32 Tick;				// CPU 时钟 ( 低 32 位 )
		int64 Rand;				// 随机数
	} xid_struct, *xid;
	
	// XID 转 字符串 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtEncodeXID(xid pXID);
	
	// 字符串 转 XID ( 需要使用 xrtFree 释放内存 )
	XXAPI xid xrtDecodeXID(str sXID);
	
	// 获取 XID ( 需要使用 xrtFree 释放内存 )
	XXAPI xid xrtMakeXID();
	
	// 获取 XID 字符串 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtMakeXIDS();
	
	// 比较两个 XID 是否相同
	XXAPI bool xrtCompXID(xid pXID1, xid pXID2);
	
	
	
	/* ------------------------------------ Buffer 函数库 ------------------------------------ */
	
	// 内容类型
	#define XBUF_BINARY 0						// 二进制
	#define XBUF_ANSI 1							// ANSI 字符串
	#define XBUF_UTF8 1							// UTF8 字符串
	#define XBUF_UTF16 2						// UTF16 字符串
	#define XBUF_UTF32 4						// UTF32 字符串
	
	// 默认增量长度
	#define XBUFFER_ALLOC_STEP 0x10000
	
	// 内存缓冲区管理单元数据结构
	typedef struct {
		char* Buffer;							// 内存缓冲区
		uint32 Length;							// 内存长度
		uint32 AllocLength;						// 已申请内存长度
		uint32 AllocStep;						// 预分配内存步长
	} xbuffer_struct, *xbuffer;
	
	// 创建内存缓冲区管理器
	XXAPI xbuffer xrtBufferCreate(uint32 iStep);
	
	// 销毁内存缓冲区管理器
	XXAPI void xrtBufferDestroy(xbuffer pBuf);
	
	// 初始化缓冲区管理器（对自维护结构体指针使用）
	XXAPI void xrtBufferInit(xbuffer pBuf, uint32 iStep);
	
	// 释放缓冲区管理器（对自维护结构体指针使用）
	XXAPI void xrtBufferUnit(xbuffer pBuf);
	
	// 分配内存
	XXAPI bool xrtBufferMalloc(xbuffer pBuf, uint32 iCount);
	
	// 中间添加数据（可以复制或者开辟新的数据区，不会自动将新开辟的数据区填充 \0）
	XXAPI bool xrtBufferInsert(xbuffer pBuf, uint32 iPos, ptr pData, uint32 iSize, uint32 bStrMode);
	
	// 末尾添加数据
	XXAPI bool xrtBufferAppend(xbuffer pBuf, ptr pData, uint32 iSize, uint32 bStrMode);
	
	// 清空管理单元
	#define xrtBufferClear xrtBufferUnit
	
	
	
	/* ------------------------------------ Point Array 函数库 ------------------------------------ */
	
	/*
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 默认步长
	#define XPARRAY_PREASSIGNSTEP	256
	
	// 指针数组内存管理器数据结构
	typedef struct {
		ptr* Memory;							// 管理器内存指针
		uint32 Count;							// 管理器中存在多少成员
		uint32 AllocCount;						// 已经申请的结构数量
		uint32 AllocStep;						// 预分配内存步长
	} xparray_struct, *xparray;
	
	// 创建指针内存管理器
	XXAPI xparray xrtPtrArrayCreate();
	
	// 销毁指针内存管理器
	XXAPI void xrtPtrArrayDestroy(xparray pObject);
	
	// 初始化内存管理器（对自维护结构体指针使用）
	XXAPI void xrtPtrArrayInit(xparray pObject);
	
	// 释放内存管理器（对自维护结构体指针使用）
	XXAPI void xrtPtrArrayUnit(xparray pObject);
	
	// 分配内存
	XXAPI bool xrtPtrArrayMalloc(xparray pObject, uint32 iCount);
	
	// 中间插入成员(0为头部插入，pObject->Count为末尾插入)
	XXAPI uint32 xrtPtrArrayInsert(xparray pObject, uint32 iPos, ptr pVal);
	
	// 末尾添加成员
	XXAPI uint32 xrtPtrArrayAppend(xparray pObject, ptr pVal);
	
	// 添加成员，自动查找空隙（替换为 NULL 的值）
	XXAPI uint32 xrtPtrArrayAddAlt(xparray pObject, ptr pVal);
	
	// 交换成员
	XXAPI bool xrtPtrArraySwap(xparray pObject, uint32 iPosA, uint32 iPosB);
	
	// 删除成员
	XXAPI bool xrtPtrArrayRemove(xparray pObject, uint32 iPos, uint32 iCount);
	
	// 删除所有成员
	#define xrtPtrArrayRemoveAll xrtPtrArrayUnit
	
	// 清空管理器
	#define xrtPtrArrayClear xrtPtrArrayUnit
	
	// 获取成员指针
	XXAPI ptr xrtPtrArrayGet(xparray pObject, uint32 iPos);
	XXAPI ptr xrtPtrArrayGet_Unsafe(xparray pObject, uint32 iPos);
	static inline ptr xrtPtrArrayGet_Inline(xparray pObject, uint32 iPos)
	{
		return pObject->Memory[iPos - 1];
	}
	
	// 设置成员指针
	XXAPI bool xrtPtrArraySet(xparray pObject, uint32 iPos, ptr pVal);
	XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, uint32 iPos, ptr pVal);
	static inline void xrtPtrArraySet_Inline(xparray pObject, uint32 iPos, ptr pVal)
	{
		pObject->Memory[iPos - 1] = pVal;
	}
	
	// 成员排序
	XXAPI bool xrtPtrArraySort(xparray pObject, ptr procCompar);
	
	
	
	/* ------------------------------------ Array 函数库 ------------------------------------ */
	
	/*
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 默认步长
	#define XARRAY_PREASSIGNSTEP	256
	
	// 结构体数组内存管理器数据结构
	typedef struct {
		str Memory;						// 管理器内存指针
		uint32 ItemLength;				// 成员占用内存长度
		uint32 Count;					// 管理器中存在多少成员
		uint32 AllocCount;				// 已经申请的结构数量
		uint32 AllocStep;				// 预分配内存步长
	} xarray_struct, *xarray;
	
	// 创建数组
	XXAPI xarray xrtArrayCreate(uint32 iItemLength);
	
	// 销毁数组
	XXAPI void xrtArrayDestroy(xarray pArr);
	
	// 初始化数组的数据结构 ( 用于内嵌数组的对象使用 )
	XXAPI void xrtArrayInit(xarray pArr, uint32 iItemLength);
	
	// 释放数组的数据结构 ( 但不会释放数组结构体本身的内存，用于内嵌数组的对象使用 )
	XXAPI void xrtArrayUnit(xarray pArr);
	
	// 分配内存
	XXAPI bool xrtArrayAlloc(xarray pArr, uint32 iCount);
	
	// 中间插入成员
	XXAPI uint32 xrtArrayInsert(xarray pArr, uint32 iPos, uint32 iCount);
	
	// 末尾添加成员
	XXAPI uint32 xrtArrayAppend(xarray pArr, uint32 iCount);
	
	// 交换成员
	XXAPI bool xrtArraySwap(xarray pArr, uint32 iPosA, uint32 iPosB);
	
	// 删除成员
	XXAPI bool xrtArrayRemove(xarray pArr, uint32 iPos, uint32 iCount);
	
	// 删除所有成员
	#define xrtArrayRemoveAll xrtArrayUnit
	
	// 清空管理器
	#define xrtArrayClear xrtArrayUnit
	
	// 获取成员数据指针
	XXAPI ptr xrtArrayGet(xarray pArr, uint32 iPos);
	XXAPI ptr xrtArrayGet_Unsafe(xarray pArr, uint32 iPos);
	static inline ptr xrtArrayGet_Inline(xarray pArr, uint32 iPos)
	{
		return &(pArr->Memory[(iPos - 1) * pArr->ItemLength]);
	}
	
	// 成员排序
	XXAPI bool xrtArraySort(xarray pArr, ptr procCompar);
	
	
	
	/* ------------------------------------ BSMM 函数库 ------------------------------------ */
	
	/*
		Blocks Struct Memory Management [块结构内存管理器]
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 内存指针单向链表数据结构
	typedef struct MemPtr_LLNode {
		ptr Ptr;
		struct MemPtr_LLNode* Next;
	} MemPtr_LLNode;
	
	// 数据块结构内存管理器数据结构
	typedef struct {
		uint32 ItemLength;			// 成员占用内存长度
		uint32 Count;					// 管理器中存在多少成员
		xparray_struct PageMMU;				// 内存页管理器
		MemPtr_LLNode* LL_Free;				// 已释放的内存块链表
	} xbsmm_struct, *xbsmm;
	
	// 创建数据块结构内存管理器
	XXAPI xbsmm xrtBsmmCreate(uint32 iItemLength);
	
	// 销毁数据块结构内存管理器
	XXAPI void xrtBsmmDestroy(xbsmm objBSMM);
	
	// 初始化数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Create 功能类似）
	XXAPI void xrtBsmmInit(xbsmm objBSMM, uint32 iItemLength);
	
	// 释放数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Destroy 功能类似）
	XXAPI void xrtBsmmUnit(xbsmm objBSMM);
	
	// 申请结构体内存
	XXAPI ptr xrtBsmmAlloc(xbsmm objBSMM);
	
	// 释放结构体内存
	XXAPI void xrtBsmmFree(xbsmm objBSMM, ptr p);
	
	// 获取成员指针（非特殊需求不建议使用）
	static inline ptr xrtBsmmGetPtr_Inline(xbsmm objBSMM, uint32 iIdx)
	{
		uint32 iBlock = iIdx >> 8;
		uint32 iPos = iIdx & 0xFF;
		str pBlock = xrtPtrArrayGet_Inline(&objBSMM->PageMMU, iBlock + 1);
		if ( pBlock ) {
			return &pBlock[iPos * objBSMM->ItemLength];
		} else {
			return NULL;
		}
	}
	
	
	
	/* ------------------------------------ Memory Unit 函数库 ------------------------------------ */
	
	// 内存固定的前置数据（用于识别内存是哪个管理器分配的）
	typedef struct {
		uint32 ItemFlag;
	} MMU_Value, *MMU_ValuePtr;
	
	// MMU有效ID区间掩码
	#define MMU_FLAG_MASK				0x3FFFFFFF
	
	// 结构体使用状态标记
	#define MMU_FLAG_USE				0x80000000
	
	// GC回收标记位
	#define MMU_FLAG_GC					0x40000000
	
	// 非内存管理器管理的内存
	#define MMU_FLAG_EXT				0xBFFFFFFF
	
	// GC标记
	#define xrtMemUnitGC_Mark(p) (((MMU_ValuePtr)((void*)p - sizeof(MMU_Value)))->ItemFlag |= MMU_FLAG_GC)
	
	// 数据管理单元数据结构
	typedef struct {
		uint8 FreeList[256];						// 已释放成员列表
		uint32 ItemLength;							// 成员占用内存长度
		uint16 Count;								// 成员数量
		uint8 FreeCount;							// 已释放成员数量
		uint8 FreeOffset;							// 首个已释放成员在列表中的偏移位置
		uint32 Flag;								// 值的 Flag 前缀（由上级管理器下发，0-255 区间会被 idx 覆盖）
		char Memory[];								// 数据
	} xmemunit_struct, *xmemunit;
	
	// 创建内存管理单元（iItemLength会自动增加4个字节用于确定内存位置和所属的管理器单元编号）
	XXAPI xmemunit xrtMemUnitCreate(uint32 iItemLength);
	
	// 销毁内存管理单元
	#define xrtMemUnitDestroy xrtFree
	
	// 从内存管理单元中申请一个元素
	static inline ptr xrtMemUnitAlloc_Inline(xmemunit objUnit)
	{
		uint8 idx = objUnit->Count;
		// 优先复用已释放的数据
		if ( objUnit->FreeCount > 0 ) {
			idx = objUnit->FreeList[objUnit->FreeOffset];
			objUnit->FreeOffset++;
			objUnit->FreeCount--;
		}
		objUnit->Count++;
		MMU_ValuePtr v = (MMU_ValuePtr)&(objUnit->Memory[objUnit->ItemLength * idx]);
		v->ItemFlag = objUnit->Flag | idx;
		return (ptr)&v[1];
	}
	XXAPI ptr xrtMemUnitAlloc(xmemunit objUnit);
	
	// 释放内存管理单元中某个元素（FreeIdx不会清空 ItemFlag，建议由调用方负责清空）
	static inline void xrtMemUnitFreeIdx_Inline(xmemunit objUnit, uint8 idx)
	{
		objUnit->FreeList[(objUnit->FreeOffset + objUnit->FreeCount) & 0xFF] = idx;
		objUnit->Count--;
		if ( objUnit->Count ) {
			objUnit->FreeCount++;
		} else {
			objUnit->FreeCount = 0;
			objUnit->FreeOffset = 0;
		}
	}
	XXAPI bool xrtMemUnitFreeIdx(xmemunit objUnit, uint8 idx);
	static inline void xrtMemUnitFree_Inline(xmemunit objUnit, ptr obj)
	{
		MMU_ValuePtr v = obj - 4;
		unsigned char idx = v->ItemFlag & 0xFF;
		v->ItemFlag = 0;
		xrtMemUnitFreeIdx_Inline(objUnit, idx);
	}
	XXAPI bool xrtMemUnitFree(xmemunit objUnit, ptr obj);
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI int xrtMemUnitGC(xmemunit objUnit, bool bFreeMark);
	
	
	
	/* ------------------------------------ Fixed-Size Memory Pool 函数库 ------------------------------------ */
	
	// 内存管理器链表结构
	typedef struct MMU_LLNode {
		uint32 Flag;
		xmemunit objMMU;
		struct MMU_LLNode* Prev;
		struct MMU_LLNode* Next;
	} MMU_LLNode;
	
	// 256步进内存管理器数据结构
	typedef struct {
		uint32 ItemLength;					// 成员占用内存长度
		xbsmm_struct arrMMU;						// MMU 阵列
		MMU_LLNode* LL_Idle;						// 有空闲的内存管理单元链表首元素 (优先分配内存的单元)
		MMU_LLNode* LL_Full;						// 满载的内存管理单元链表首元素 (不会从这些单元中分配内存)
		MMU_LLNode* LL_Null;						// 缓存的全空内存管理单元 (备用单元，最多只留一个)
		MMU_LLNode* LL_Free;						// 已释放的内存管理单元 Flag 链表首元素 (申请新单元优先从这里找)
	} xfsmempool_struct, *xfsmempool;
	
	// 创建内存管理器
	XXAPI xfsmempool xrtFSMemPoolCreate(uint32 iItemLength);
	
	// 销毁内存管理器
	XXAPI void xrtFSMemPoolDestroy(xfsmempool objMM);
	
	// 初始化内存管理器（对自维护结构体指针使用）
	XXAPI void xrtFSMemPoolInit(xfsmempool objMM, uint32 iItemLength);
	
	// 释放内存管理器（对自维护结构体指针使用）
	XXAPI void xrtFSMemPoolUnit(xfsmempool objMM);
	
	// 从内存管理器中申请一块内存
	XXAPI ptr xrtFSMemPoolAlloc(xfsmempool objMM);
	
	// 将内存管理器申请的内存释放掉
	XXAPI void xrtFSMemPoolFree(xfsmempool objMM, ptr p);
	
	// 将一块内存标记为使用中
	#define xrtFSMemPoolGC_Mark	xrtMemUnitGC_Mark
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI void xrtFSMemPoolGC(xfsmempool objMM, bool bFreeMark);
	
	
	
	/* ------------------------------------ Stack 函数库 ------------------------------------ */
	
	// 结构体静态栈数据结构
	typedef struct {
		union {
			char* Memory;					// 栈数据内存 - 结构体
			ptr* PtrMem;					// 栈数据内存 - 指针
		};
		uint32 ItemLength;			// 栈成员占用内存长度
		uint32 MaxCount;				// 栈最大可以容纳多少成员（栈深度）
		uint32 Count;					// 栈中存在多少成员（栈顶位置）
	} xstack_struct, *xstack;
	
	// 创建结构体静态栈
	XXAPI xstack xrtStackCreate(uint32 iMaxCount, uint32 iItemLength);
	
	// 销毁结构体静态栈
	#define xrtStackDestroy xrtFree
	
	// 压栈
	XXAPI ptr xrtStackPush(xstack objSTK);
	XXAPI uint32 xrtStackPushData(xstack objSTK, ptr pData);
	XXAPI uint32 xrtStackPushPtr(xstack objSTK, ptr pVal);
	
	// 出栈
	XXAPI ptr xrtStackPop(xstack objSTK);
	XXAPI ptr xrtStackPopPtr(xstack objSTK);
	
	// 获取栈顶对象
	XXAPI ptr xrtStackTop(xstack objSTK);
	XXAPI ptr xrtStackTopPtr(xstack objSTK);
	
	// 获取任意位置对象
	XXAPI ptr xrtStackGetPos(xstack objSTK, uint32 iPos);
	XXAPI ptr xrtStackGetPos_Unsafe(xstack objSTK, uint32 iPos);
	XXAPI ptr xrtStackGetPosPtr(xstack objSTK, uint32 iPos);
	XXAPI ptr xrtStackGetPosPtr_Unsafe(xstack objSTK, uint32 iPos);
	
	
	
	/* ------------------------------------ Dynamic Stack 函数库 ------------------------------------ */
	
	/*
		结构体动态栈，结构体内存256个递增，栈最大深度不固定
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 结构体动态栈数据结构
	typedef struct {
		uint32 ItemLength;					// 栈成员占用内存长度
		uint32 Count;							// 栈中存在多少成员（栈顶位置）
		xparray_struct MMU;							// MMU 管理器
	} xdynstack_struct, *xdynstack;
	
	// 创建结构体动态栈
	XXAPI xdynstack xrtDynStackCreate(uint32 iItemLength);
	
	// 销毁结构体动态栈
	XXAPI void xrtDynStackDestroy(xdynstack objSTK);
	
	// 初始化结构体动态栈（对自维护结构体指针使用）
	XXAPI void xrtDynStackInit(xdynstack objSTK, uint32 iItemLength);
	
	// 释放结构体动态栈（对自维护结构体指针使用）
	XXAPI void xrtDynStackUnit(xdynstack objSTK);
	
	// 压栈
	XXAPI ptr xrtDynStackPush(xdynstack objSTK);
	XXAPI uint32 xrtDynStackPushData(xdynstack objSTK, ptr pData);
	XXAPI uint32 xrtDynStackPushPtr(xdynstack objSTK, ptr pVal);
	
	// 出栈
	XXAPI ptr xrtDynStackPop(xdynstack objSTK);
	XXAPI ptr xrtDynStackPopPtr(xdynstack objSTK);
	
	// 获取栈顶对象
	XXAPI ptr xrtDynStackTop(xdynstack objSTK);
	XXAPI ptr xrtDynStackTopPtr(xdynstack objSTK);
	
	// 获取任意位置对象
	XXAPI ptr xrtDynStackGetPos(xdynstack objSTK, uint32 iPos);
	XXAPI ptr xrtDynStackGetPos_Unsafe(xdynstack objSTK, uint32 iPos);
	XXAPI ptr xrtDynStackGetPosPtr(xdynstack objSTK, uint32 iPos);
	XXAPI ptr xrtDynStackGetPosPtr_Unsafe(xdynstack objSTK, uint32 iPos);
	
	
	
	/* ------------------------------------ Linked List Base 函数库 ------------------------------------ */
	
	// 双向链表节点基础定义
	typedef struct LList_NodeBase {
		struct LList_NodeBase* Prev;
		struct LList_NodeBase* Next;
	} xllistnode_struct, *xllistnode;
	
	// 链表对象数据结构
	typedef struct {
		xllistnode FirstNode;
		xllistnode LastNode;
		uint32 Count;
	} xllistbase_struct, *xllistbase;
	
	// 初始化链表
	#define xrtLLB_Init(o) (o)->FirstNode = NULL; (o)->LastNode = NULL; (o)->Count = 0
	
	// 释放链表
	#define xrtLLB_Unit xrtLLB_Init
	
	// 节点前插入 (objNode为空则插入到FirstNode之前)
	XXAPI void xrtLLB_InsertPrev(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode);
	
	// 节点后插入 (objNode为空则插入到LastNode之后)
	XXAPI void xrtLLB_InsertNext(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode);
	
	// 删除节点
	XXAPI void xrtLLB_Remove(xllistbase objLLB, xllistnode objNode);
	
	// 删除所有成员
	#define xrtLLB_RemoveAll LLB_Unit
	
	// 清空管理器
	#define xrtLLB_Clear LLB_Unit
	
	
	
	/* ------------------------------------ Linked List 函数库 ------------------------------------ */
	
	// 链表对象数据结构
	typedef struct {
		xllistnode FirstNode;
		xllistnode LastNode;
		uint32 Count;
		xfsmempool_struct objMM;
	} xllist_struct, *xllist;
	
	// 创建链表
	XXAPI xllist xrtLListCreate(uint32 iItemLength);
	
	// 销毁链表
	XXAPI void xrtLListDestroy(xllist objLL);
	
	// 初始化链表（对自维护结构体指针使用）
	XXAPI void xrtLListInit(xllist objLL, uint32 iItemLength);
	
	// 释放链表（对自维护结构体指针使用）
	XXAPI void xrtLListUnit(xllist objLL);
	
	// 节点前插入 (objNode为空则插入到FirstNode之前)
	XXAPI xllistnode xrtLListInsertPrev(xllist objLL, xllistnode objNode);
	
	// 节点后插入 (objNode为空则插入到LastNode之后)
	XXAPI xllistnode xrtLListInsertNext(xllist objLL, xllistnode objNode);
	
	// 删除节点
	XXAPI void xrtLListRemove(xllist objLL, xllistnode objNode);
	
	// 删除所有成员
	#define xrtLListRemoveAll xrtLListUnit
	
	// 清空管理器
	#define xrtLListClear xrtLListUnit
	
	
	
	/* ------------------------------------ AVLTree Base 函数库 ------------------------------------ */
	
	// AVL树最大高度
	#define AVLTree_MAX_HEIGHT  48
	
	// AVL树节点基础定义
	typedef struct xavltnode_struct {
		struct xavltnode_struct* left;
		struct xavltnode_struct* right;
		int height;
	} xavltnode_struct, *xavltnode;
	
	// AVL树对象数据结构
	typedef struct {
		xavltnode RootNode;
		uint32 Count;
	} xavltbase_struct, *xavltbase;
	
	// 比较回调函数
	typedef int (*AVLTree_CompProc)(ptr pNode, ptr pKey);
	
	// 遍历回调函数
	typedef bool (*AVLTree_EachProc)(ptr pNode, ptr pArg);
	
	// 获取 xavltnode 对象
	#define xrtAVLTreeGetNodeBase(p) ((xavltnode)((ptr)p - sizeof(xavltnode_struct)))
	
	// 获取 xavltnode 对应的数据段
	#define xrtAVLTreeGetNodeData(p) ((ptr)(&p[1]))
	
	// 获取根节点数据段
	#define xrtAVLTreeGetRootData(obj) xrtAVLTreeGetNodeData(obj->RootNode)
	
	// 初始化 AVLTree
	#define xrtAVLTB_Init(o) (o)->RootNode = NULL; (o)->Count = 0
	
	// 释放 AVLTree
	#define xrtAVLTB_Unit xrtAVLTB_Init
	
	// 向 AVLTree 中插入节点
	XXAPI xavltnode xrtAVLTB_Insert(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey, xavltnode pNewNode);
	
	// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
	XXAPI xavltnode xrtAVLTB_Remove(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey);
	
	// 在 AVLTree 中查找节点
	XXAPI xavltnode xrtAVLTB_Search(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey);
	
	// 删除所有成员
	#define xrtAVLTB_RemoveAll xrtAVLTB_Unit
	
	// 清空管理器
	#define xrtAVLTB_Clear xrtAVLTB_Unit
	
	// 遍历 AVLTree 所有节点
	XXAPI bool xrtAVLTB_WalkRecuProc(xavltnode root, AVLTree_EachProc procEach, ptr pArg);
	XXAPI bool xrtAVLTB_WalkExRecuProc(xavltnode root, AVLTree_EachProc procPre, AVLTree_EachProc procIn, AVLTree_EachProc procPost, ptr pArg);
	#define xrtAVLTB_Walk(obj, p, a) xrtAVLTB_WalkRecuProc(obj->RootNode, (ptr)p, (ptr)a)
	#define xrtAVLTB_WalkEx(obj, p1, p2, p3, a) xrtAVLTB_WalkExRecuProc(obj->RootNode, (ptr)p1, (ptr)p2, (ptr)p3, (ptr)a)
	
	
	
	/* ------------------------------------ AVLTree 函数库 ------------------------------------ */
	
	// 键释放回调函数 ( 如果 key 中有额外需要释放的值时使用 )
	typedef void (*AVLTree_FreeProc)(ptr objTree, ptr pNode);
	
	// AVL树对象数据结构
	typedef struct xavltree_struct {
		xavltnode RootNode;
		uint32 Count;
		struct xavltree_struct* Parent;
		AVLTree_CompProc CompProc;
		AVLTree_FreeProc FreeProc;
		xfsmempool_struct objMM;
		xavltnode NodeCache;
	} xavltree_struct, *xavltree;
	
	// 创建 AVLTree
	XXAPI xavltree xrtAVLTreeCreate(uint32 iItemLength, AVLTree_CompProc procComp);
	
	// 销毁 AVLTree
	XXAPI void xrtAVLTreeDestroy(xavltree objAVLT);
	
	// 初始化 AVLTree（对自维护结构体指针使用，和 AVLTree_Create 功能类似）
	XXAPI void xrtAVLTreeInit(xavltree objAVLT, uint32 iItemLength, AVLTree_CompProc procComp);
	
	// 释放 AVLTree（对自维护结构体指针使用，和 AVLTree_Destroy 功能类似）
	XXAPI void xrtAVLTreeUnit(xavltree objAVLT);
	
	// 向 AVLTree 中插入节点，返回数据段指针（如果值已经存在，则会返回已存在的数据段指针）
	XXAPI ptr xrtAVLTreeInsert(xavltree objAVLT, ptr pKey, bool* bNew);
	
	// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
	XXAPI bool xrtAVLTreeRemove(xavltree objAVLT, ptr pKey);
	
	// 在 AVLTree 中查找节点
	XXAPI ptr xrtAVLTreeSearch(xavltree objAVLT, ptr pKey);
	
	// 删除所有成员
	#define xrtAVLTreeRemoveAll xrtAVLTreeUnit
	
	// 清空管理器
	#define xrtAVLTreeClear xrtAVLTreeUnit
	
	// 遍历 AVLTree 所有节点
	#define xrtAVLTreeWalk xrtAVLTB_Walk
	#define xrtAVLTreeWalkEx xrtAVLTB_WalkEx
	
	
	
	/* ------------------------------------ Memory Pool 函数库 ------------------------------------ */
	
	// MP256 or MP64K 大内存结构体前置结构
	typedef struct {
		uint32 Index;							// BigMM 的块索引
		uint32 Flag;							// 符合 MM256 标准的 Flag
	} MP_MemHead;
	
	// MP256 or MP64K 大内存信息链表结构体（实际返回的内存地址相当于 Ptr + sizeof(MP_MemHead)）
	typedef struct MP_BigInfoLL {
		uint32 Size;							// 申请内存的大小，可有可无（可开发辅助功能，如泄漏检测）
		ptr Ptr;									// 指针地址，使用 mmu_malloc 返回的地址
		struct MP_BigInfoLL* Next;					// 下一个链表节点（用于释放链表）
	} MP_BigInfoLL;
	
	// 单个长度区间的内存管理器结构
	typedef struct FSB_Item {
		uint32 MinLength;						// 支持最小的内存长度
		uint32 MaxLength;						// 支持最大的内存长度
		MMU_LLNode* LL_Idle;						// 空闲的 MMU 内存管理单元链表 (优先分配内存的单元)
		MMU_LLNode* LL_Full;						// 满载的 MMU 内存管理单元链表 (不会从这些单元中分配内存)
		MMU_LLNode* LL_Null;						// 全空的 MMU 内存管理单元 (备用单元，最多只留一个)
		MMU_LLNode* LL_Free;						// 已释放的 MMU 内存管理单元链表 (申请新单元优先从这里找)
		struct FSB_Item* left;						// 左子树
		struct FSB_Item* right;						// 右子树
	} FSB_Item;
	
	// 256步进内存池数据结构
	typedef struct {
		FSB_Item* FSB_Memory;						// fixed-size-blocks 内存（MP256_Create参数为 1 或 2 时自动创建，否则需要手动创建，不为空会调用 mmu_free 释放）
		FSB_Item* FSB_RootNode;						// fixed-size-blocks 二叉树（固定大小区块内存管理器阵列）
		xbsmm_struct arrMMU;						// MMU 阵列
		xbsmm_struct BigMM;							// 大内存数组
		MP_BigInfoLL* LL_BigFree;					// 大内存已释放的内存块链表
	} xmempool_struct, *xmempool;
	
	// 创建内存池
	XXAPI xmempool xrtMemPoolCreate(int iCustom);
	
	// 销毁内存池
	XXAPI void xrtMemPoolDestroy(xmempool objMP);
	
	// 初始化内存池（对自维护结构体指针使用，和 MP256_Create 功能类似）
	XXAPI void xrtMemPoolInit(xmempool objMP, int iCustom);
	
	// 释放内存池（对自维护结构体指针使用，和 MP256_Destroy 功能类似）
	XXAPI void xrtMemPoolUnit(xmempool objMP);
	
	// 从内存池中申请一块内存
	XXAPI ptr xrtMemPoolAlloc(xmempool objMP, uint32 iSize);
	
	// 将内存池申请的内存释放掉
	XXAPI void xrtMemPoolFree(xmempool objMP, ptr ptr);
	
	// 将一块内存标记为使用中
	#define xrtMemPoolGC_Mark	xrtMemUnitGC_Mark
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark);
	
	
	
	/* ------------------------------------ Dict 函数库 ------------------------------------ */
	
	// 字典 Key 数据结构
	typedef struct {
		ptr Key;
		uint32 KeyLen;
		uint32 Hash;
	} Dict_Key;
	
	// 字典对象数据结构
	typedef struct {
		xavltree_struct AVLT;
		xmempool MP;
	} xdict_struct, *xdict;
	
	// 字典遍历回调函数
	typedef bool (*Dict_EachProc)(Dict_Key* pKey, ptr pVal, ptr pArg);
	
	// 创建哈希表
	XXAPI xdict xrtDictCreate(uint32 iItemLength);
	
	// 销毁哈希表
	XXAPI void xrtDictDestroy(xdict objHT);
	
	// 初始化哈希表（对自维护结构体指针使用，和 AVLHT32_Create 功能类似）
	XXAPI void xrtDictInit(xdict objHT, uint32 iItemLength);
	
	// 释放哈希表（对自维护结构体指针使用，和 AVLHT32_Destroy 功能类似）
	XXAPI void xrtDictUnit(xdict objHT);
	
	// 设置值
	static inline ptr xrtDictSetWithKey(xdict objHT, Dict_Key* objKey, bool* bNewRet)
	{
		bool bNew;
		Dict_Key* pNode = xrtAVLTreeInsert(&objHT->AVLT, objKey, &bNew);
		if ( pNode ) {
			if ( bNewRet ) {
				*bNewRet = bNew;
			}
			if ( bNew ) {
				uint32 iKeyLen = objKey->KeyLen;
				if ( objHT->MP ) {
					pNode->Key = xrtMemPoolAlloc(objHT->MP, iKeyLen + 1);
				} else {
					pNode->Key = xrtMalloc(iKeyLen + 1);
				}
				pNode->KeyLen = iKeyLen;
				pNode->Hash = objKey->Hash;
				memcpy(pNode->Key, objKey->Key, iKeyLen);
				((char*)pNode->Key)[iKeyLen] = 0;
			}
			return &pNode[1];
		}
		return NULL;
	}
	XXAPI ptr xrtDictSet(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet);
	
	// 设置值 - 当值为 ptr 时直接修改指针内容
	XXAPI bool xrtDictSetPtr(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal);
	
	// 获取值
	static inline ptr xrtDictGetWithKey(xdict objHT, Dict_Key* objKey)
	{
		Dict_Key* pNode = xrtAVLTreeSearch(&objHT->AVLT, objKey);
		if ( pNode ) {
			return &pNode[1];
		}
		return NULL;
	}
	XXAPI ptr xrtDictGet(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 获取值 - 当值为 ptr 时直接获取指针内容
	XXAPI ptr xrtDictGetPtr(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 删除值
	XXAPI bool xrtDictRemove(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 删除值，当值为 ptr 时返回 ptr
	XXAPI ptr xrtDictRemovePtr(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 判断值是否存在
	XXAPI bool xrtDictExists(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 删除所有成员
	#define xrtDictRemoveAll xrtDictUnit
	
	// 清空管理器
	#define xrtDictClear xrtDictUnit
	
	// 获取表内元素数量
	XXAPI uint32 xrtDictCount(xdict objHT);
	
	// 遍历表元素
	XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, ptr pArg);
	
	
	
	/* ------------------------------------ List 函数库 ------------------------------------ */
	
	// 列表对象数据结构
	typedef struct {
		xavltree_struct AVLT;
	} xlist_struct, *xlist;
	
	// 列表遍历回调函数
	typedef bool (*List_EachProc)(int64 pKey, ptr pVal, ptr pArg);
	
	// 创建列表
	XXAPI xlist xrtListCreate(uint32 iItemLength);
	
	// 销毁列表
	XXAPI void xrtListDestroy(xlist objList);
	
	// 初始化列表（对自维护结构体指针使用）
	XXAPI void xrtListInit(xlist objList, uint32 iItemLength);
	
	// 释放列表（对自维护结构体指针使用）
	XXAPI void xrtListUnit(xlist objList);
	
	// 设置值
	XXAPI ptr xrtListSet(xlist objList, int64 iKey, bool* bNewRet);
	
	// 设置值 - 当值为 ptr 时直接修改指针内容
	XXAPI bool xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal);
	
	// 获取值
	XXAPI ptr xrtListGet(xlist objList, int64 iKey);
	
	// 获取值 - 当值为 ptr 时直接获取指针内容
	XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey);
	
	// 删除值
	XXAPI bool xrtListRemove(xlist objList, int64 iKey);
	
	// 删除值，当值为 ptr 时返回 ptr
	XXAPI ptr xrtListRemovePtr(xlist objList, int64 iKey);
	
	// 判断值是否存在
	XXAPI bool xrtListExists(xlist objList, int64 iKey);
	
	// 删除所有成员
	#define xrtListRemoveAll xrtListUnit
	
	// 清空管理器
	#define xrtListClear xrtListUnit
	
	// 获取表内元素数量
	XXAPI uint32 xrtListCount(xlist objList);
	
	// 遍历表元素
	XXAPI void xrtListWalk(xlist objList, List_EachProc procEach, ptr pArg);
	
	
	
	/* ------------------------------------ Value 函数库 ------------------------------------ */
	
	// 数据类型 - 主类型
	#define XVO_DT_EMPTY			0				// 不存在的数据
	#define XVO_DT_NULL				1				// null
	#define XVO_DT_BOOL				2				// bool : true | false
	#define XVO_DT_INT				3				// 整数（int64）
	#define XVO_DT_FLOAT			4				// 浮点数（double）
	#define XVO_DT_TEXT				5				// 字符串
	#define XVO_DT_TIME				6				// 时间
	#define XVO_DT_POINT			7				// 指针
	#define XVO_DT_FUNC				8				// 函数
	#define XVO_DT_ARRAY			9				// 数组
	#define XVO_DT_LIST				10				// 列表
	#define XVO_DT_COLL				11				// 集合
	#define XVO_DT_TABLE			12				// 表
	#define XVO_DT_STRUCT			13				// 结构体
	#define XVO_DT_OBJECT			14				// 对象
	#define XVO_DT_CUSTOM			15				// 自定义
	
	// Value 标准数据类 [ 16 Byte ]
	typedef struct xvalue_struct {
		uint32 Type:4;
		uint32 Reserve:1;
		uint32 IsStatic:1;
		uint32 RefCount:26;
		uint32 Size;
		union {
			bool vBool;
			int64 vInt;
			double vFloat;
			str vText;
			xtime vTime;
			ptr vPoint;
			struct xvalue_struct* (*vFunc)(struct xvalue_struct* varENV, struct xvalue_struct* varParam);
			xparray vArray;
			xlist vList;
			xavltree vColl;
			xdict vTable;
			ptr vStruct;
			ptr vObject;
			ptr vCustom;
		};
	} xvalue_struct, *xvalue;
	
	// 函数指针类型定义
	typedef xvalue (*xfunction)(xvalue pENV, xvalue arrParam);
	
	// Custom 类 [ 16 bytes ]
	struct xcustom_struct {
		void (*construct)(xvalue var);
		void (*destruct)(xvalue var);
		int (*set)(xvalue var, str key, xvalue val);
		xvalue (*get)(xvalue var, str key);
		xvalue (*call)(xvalue var, str key, xvalue param);
		ptr value;
	};
	
	// 引用计数操作
	XXAPI void xvoAddRef(xvalue pVal);
	XXAPI void xvoUnref(xvalue pVal);
	static inline void xvoAddRef_Inline(xvalue pVal)
	{
		if ( pVal->RefCount >= 0x3FFFFFF ) {
			// 引用计数太多，就转为静态值
			pVal->IsStatic = 1;
		} else {
			pVal->RefCount++;
		}
	}
	
	// 创建值
	XXAPI xvalue xvoCreateNull();
	XXAPI xvalue xvoCreateBool(bool bVal);
	XXAPI xvalue xvoCreateInt(int64 iVal);
	XXAPI xvalue xvoCreateFloat(double fVal);
	XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);
	XXAPI xvalue xvoCreateTime(xtime tVal);
	XXAPI xvalue xvoCreateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
	XXAPI xvalue xvoCreatePoint(ptr point);
	XXAPI xvalue xvoCreateFunc(xfunction pFunc);
	XXAPI xvalue xvoCreateArray();
	XXAPI xvalue xvoCreateList();
	XXAPI xvalue xvoCreateColl();
	XXAPI xvalue xvoCreateTable();
	XXAPI xvalue xvoCreateStruct(uint32 iSize);
	XXAPI xvalue xvoCreateObject(uint32 iSize);
	XXAPI xvalue xvoCreateCustom(ptr pObj);
	
	// 读取值
	XXAPI bool xvoGetBool(xvalue pVal);
	XXAPI int64 xvoGetInt(xvalue pVal);
	XXAPI double xvoGetFloat(xvalue pVal);
	XXAPI str xvoGetText(xvalue pVal);
	XXAPI xtime xvoGetTime(xvalue pVal);
	XXAPI ptr xvoGetPoint(xvalue pVal);
	XXAPI xfunction xvoGetFunc(xvalue pVal);
	XXAPI xparray xvoGetArray(xvalue pVal);
	XXAPI xlist xvoGetList(xvalue pVal);
	XXAPI xavltree xvoGetColl(xvalue pVal);
	XXAPI xdict xvoGetTable(xvalue pVal);
	XXAPI ptr xvoGetStruct(xvalue pVal);
	XXAPI ptr xvoGetObject(xvalue pVal);
	XXAPI ptr xvoGetCustom(xvalue pVal);
	
	// Array 读数据
	XXAPI xvalue xvoArrayGetValue(xvalue pArr, uint32 index);
	#define xvoArrayGetBool(pArr, index)														xvoGetBool(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetInt(pArr, index)															xvoGetInt(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetFloat(pArr, index)														xvoGetFloat(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetText(pArr, index)														xvoGetText(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetTime(pArr, index)														xvoGetTime(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetPoint(pArr, index)														xvoGetPoint(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetFunc(pArr, index)														xvoGetFunc(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetArray(pArr, index)														xvoGetArray(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetList(pArr, index)														xvoGetList(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetColl(pArr, index)														xvoGetColl(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetTable(pArr, index)														xvoGetTable(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetStruct(pArr, index)														xvoGetStruct(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetObject(pArr, index)														xvoGetObject(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetCustom(pArr, index)														xvoGetCustom(xvoArrayGetValue(pArr, index))
	
	// Array 追加数据
	XXAPI bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);
	#define xvoArrayAppendNull(pArr)															xvoArrayAppendValue(pArr, xvoCreateNull(), TRUE)
	#define xvoArrayAppendBool(pArr, bVal)														xvoArrayAppendValue(pArr, xvoCreateBool(bVal), TRUE)
	#define xvoArrayAppendInt(pArr, iVal)														xvoArrayAppendValue(pArr, xvoCreateInt(iVal), TRUE)
	#define xvoArrayAppendFloat(pArr, fVal)														xvoArrayAppendValue(pArr, xvoCreateFloat(fVal), TRUE)
	#define xvoArrayAppendText(pArr, sVal, iSize, bColloc)										xvoArrayAppendValue(pArr, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoArrayAppendTime(pArr, tVal)														xvoArrayAppendValue(pArr, xvoCreateTime(tVal), TRUE)
	#define xvoArrayAppendTimeSerial(pArr, iYear, iMonth, iDay, iHour, iMinute, iSecond)		xvoArrayAppendValue(pArr, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoArrayAppendPoint(pArr, pVal)														xvoArrayAppendValue(pArr, xvoCreatePoint(pVal), TRUE)
	#define xvoArrayAppendFunc(pArr, func)														xvoArrayAppendValue(pArr, xvoCreateFunc(func), TRUE)
	#define xvoArrayAppendArray(pArr)															xvoArrayAppendValue(pArr, xvoCreateArray(), TRUE)
	#define xvoArrayAppendList(pArr)															xvoArrayAppendValue(pArr, xvoCreateList(), TRUE)
	#define xvoArrayAppendColl(pArr)															xvoArrayAppendValue(pArr, xvoCreateColl(), TRUE)
	#define xvoArrayAppendTable(pArr)															xvoArrayAppendValue(pArr, xvoCreateTable(), TRUE)
	#define xvoArrayAppendStruct(pArr, size)													xvoArrayAppendValue(pArr, xvoCreateStruct(size), TRUE)
	#define xvoArrayAppendObject(pArr, size)													xvoArrayAppendValue(pArr, xvoCreateObject(size), TRUE)
	#define xvoArrayAppendCustom(pArr, point)													xvoArrayAppendValue(pArr, xvoCreateCustom(point), TRUE)
	
	// Array 插入操作
	XXAPI bool xvoArrayInsertValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
	#define xvoArrayInsertNull(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateNull(), TRUE)
	#define xvoArrayInsertBool(pArr, idx, bVal)													xvoArrayInsertValue(pArr, idx, xvoCreateBool(bVal), TRUE)
	#define xvoArrayInsertInt(pArr, idx, iVal)													xvoArrayInsertValue(pArr, idx, xvoCreateInt(iVal), TRUE)
	#define xvoArrayInsertFloat(pArr, idx, fVal)												xvoArrayInsertValue(pArr, idx, xvoCreateFloat(fVal), TRUE)
	#define xvoArrayInsertText(pArr, idx, sVal, iSize, bColloc)									xvoArrayInsertValue(pArr, idx, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoArrayInsertTime(pArr, idx, tVal)													xvoArrayInsertValue(pArr, idx, xvoCreateTime(tVal), TRUE)
	#define xvoArrayInsertTimeSerial(pArr, idx, iYear, iMonth, iDay, iHour, iMinute, iSecond)	xvoArrayInsertValue(pArr, idx, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoArrayInsertPoint(pArr, idx, pVal)												xvoArrayInsertValue(pArr, idx, xvoCreatePoint(pVal), TRUE)
	#define xvoArrayInsertFunc(pArr, idx, func)													xvoArrayInsertValue(pArr, idx, xvoCreateFunc(func), TRUE)
	#define xvoArrayInsertArray(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateArray(), TRUE)
	#define xvoArrayInsertList(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateList(), TRUE)
	#define xvoArrayInsertColl(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateColl(), TRUE)
	#define xvoArrayInsertTable(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateTable(), TRUE)
	#define xvoArrayInsertStruct(pArr, idx, size)												xvoArrayInsertValue(pArr, idx, xvoCreateStruct(size), TRUE)
	#define xvoArrayInsertObject(pArr, idx, size)												xvoArrayInsertValue(pArr, idx, xvoCreateObject(size), TRUE)
	#define xvoArrayInsertCustom(pArr, idx, point)												xvoArrayInsertValue(pArr, idx, xvoCreateCustom(point), TRUE)
	
	// Array 修改操作
	XXAPI bool xvoArraySetValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
	#define xvoArraySetNull(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateNull(), TRUE)
	#define xvoArraySetBool(pArr, idx, bVal)													xvoArraySetValue(pArr, idx, xvoCreateBool(bVal), TRUE)
	#define xvoArraySetInt(pArr, idx, iVal)														xvoArraySetValue(pArr, idx, xvoCreateInt(iVal), TRUE)
	#define xvoArraySetFloat(pArr, idx, fVal)													xvoArraySetValue(pArr, idx, xvoCreateFloat(fVal), TRUE)
	#define xvoArraySetText(pArr, idx, sVal, iSize, bColloc)									xvoArraySetValue(pArr, idx, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoArraySetTime(pArr, idx, tVal)													xvoArraySetValue(pArr, idx, xvoCreateTime(tVal), TRUE)
	#define xvoArraySetTimeSerial(pArr, idx, iYear, iMonth, iDay, iHour, iMinute, iSecond)		xvoArraySetValue(pArr, idx, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoArraySetPoint(pArr, idx, pVal)													xvoArraySetValue(pArr, idx, xvoCreatePoint(pVal), TRUE)
	#define xvoArraySetFunc(pArr, idx, func)													xvoArraySetValue(pArr, idx, xvoCreateFunc(func), TRUE)
	#define xvoArraySetArray(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateArray(), TRUE)
	#define xvoArraySetList(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateList(), TRUE)
	#define xvoArraySetColl(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateColl(), TRUE)
	#define xvoArraySetTable(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateTable(), TRUE)
	#define xvoArraySetStruct(pArr, idx, size)													xvoArraySetValue(pArr, idx, xvoCreateStruct(size), TRUE)
	#define xvoArraySetObject(pArr, idx, size)													xvoArraySetValue(pArr, idx, xvoCreateObject(size), TRUE)
	#define xvoArraySetCustom(pArr, idx, point)													xvoArraySetValue(pArr, idx, xvoCreateCustom(point), TRUE)
	
	// Array 合并
	XXAPI bool xvoArrayMerge(xvalue pArr1, xvalue pArr2);
	
	// Array 操作
	XXAPI bool xvoArraySwap(xvalue pArr, uint32 index1, uint32 index2);
	XXAPI bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count);
	XXAPI uint32 xvoArrayItemCount(xvalue pArr);
	XXAPI bool xvoArrayClear(xvalue pArr);
	XXAPI bool xvoArrayAlloc(xvalue pArr, uint32 count);
	XXAPI bool xvoArraySort(xvalue pArr, ptr proc);
	
	// List 读数据
	XXAPI xvalue xvoListGetValue(xvalue pList, int64 index);
	#define xvoListGetBool(pList, index)														xvoGetBool(xvoListGetValue(pList, index))
	#define xvoListGetInt(pList, index)															xvoGetInt(xvoListGetValue(pList, index))
	#define xvoListGetFloat(pList, index)														xvoGetFloat(xvoListGetValue(pList, index))
	#define xvoListGetText(pList, index)														xvoGetText(xvoListGetValue(pList, index))
	#define xvoListGetTime(pList, index)														xvoGetTime(xvoListGetValue(pList, index))
	#define xvoListGetPoint(pList, index)														xvoGetPoint(xvoListGetValue(pList, index))
	#define xvoListGetFunc(pList, index)														xvoGetFunc(xvoListGetValue(pList, index))
	#define xvoListGetArray(pList, index)														xvoGetArray(xvoListGetValue(pList, index))
	#define xvoListGetList(pList, index)														xvoGetList(xvoListGetValue(pList, index))
	#define xvoListGetColl(pList, index)														xvoGetColl(xvoListGetValue(pList, index))
	#define xvoListGetTable(pList, index)														xvoGetTable(xvoListGetValue(pList, index))
	#define xvoListGetStruct(pList, index)														xvoGetStruct(xvoListGetValue(pList, index))
	#define xvoListGetObject(pList, index)														xvoGetObject(xvoListGetValue(pList, index))
	#define xvoListGetCustom(pList, index)														xvoGetCustom(xvoListGetValue(pList, index))
	
	// List 写数据
	XXAPI bool xvoListSetValue(xvalue pList, int64 index, xvalue pVal, bool bColloc);
	#define xvoListSetNull(pList, idx)															xvoListSetValue(pList, idx, xvoCreateNull(), TRUE)
	#define xvoListSetBool(pList, idx, bVal)													xvoListSetValue(pList, idx, xvoCreateBool(bVal), TRUE)
	#define xvoListSetInt(pList, idx, iVal)														xvoListSetValue(pList, idx, xvoCreateInt(iVal), TRUE)
	#define xvoListSetFloat(pList, idx, fVal)													xvoListSetValue(pList, idx, xvoCreateFloat(fVal), TRUE)
	#define xvoListSetText(pList, idx, sVal, iSize, bColloc)									xvoListSetValue(pList, idx, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoListSetTime(pList, idx, tVal)													xvoListSetValue(pList, idx, xvoCreateTime(tVal), TRUE)
	#define xvoListSetTimeSerial(pList, idx, iYear, iMonth, iDay, iHour, iMinute, iSecond)		xvoListSetValue(pList, idx, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoListSetPoint(pList, idx, pVal)													xvoListSetValue(pList, idx, xvoCreatePoint(pVal), TRUE)
	#define xvoListSetFunc(pList, idx, func)													xvoListSetValue(pList, idx, xvoCreateFunc(func), TRUE)
	#define xvoListSetArray(pList, idx)															xvoListSetValue(pList, idx, xvoCreateArray(), TRUE)
	#define xvoListSetList(pList, idx)															xvoListSetValue(pList, idx, xvoCreateList(), TRUE)
	#define xvoListSetColl(pList, idx)															xvoListSetValue(pList, idx, xvoCreateColl(), TRUE)
	#define xvoListSetTable(pList, idx)															xvoListSetValue(pList, idx, xvoCreateTable(), TRUE)
	#define xvoListSetStruct(pList, idx, size)													xvoListSetValue(pList, idx, xvoCreateStruct(size), TRUE)
	#define xvoListSetObject(pList, idx, size)													xvoListSetValue(pList, idx, xvoCreateObject(size), TRUE)
	#define xvoListSetCustom(pList, idx, point)													xvoListSetValue(pList, idx, xvoCreateCustom(point), TRUE)
	
	// List 合并
	XXAPI bool xvoListMerge(xvalue pList1, xvalue pList2, bool bReWrite);
	
	// List 操作
	XXAPI bool xvoListExists(xvalue pList, int64 index);
	XXAPI bool xvoListRemove(xvalue pList, int64 index);
	XXAPI uint32 xvoListItemCount(xvalue pList);
	XXAPI bool xvoListClear(xvalue pList);
	XXAPI bool xvoListSetParent(xvalue pList, xvalue pParentList);
	
	// Coll Key 数据结构
	typedef struct {
		uint64 Hash;
		xvalue Value;
	} Coll_Key;
	
	// 构建 Coll_Key
	#if defined(__x86_64__) || defined(_M_X64)
		// 64 bit
		#define MAKE_COLL_KEY(k, v) if ( v->Type == XVO_DT_TEXT ) { uint64 iHash = xrtHash64(v->vText, v->Size); k.Hash = ((uint64)v->Type << 60) | ((uint64)v->Size << 28) | (iHash & 0xFFFFFFF); } else if ( v->Type == XVO_DT_BOOL ) { k.Hash = ((uint64)v->Type << 60) | v->vBool; } else if ( v->Type == XVO_DT_NULL ) { k.Hash = (uint64)v->Type << 60; } else { k.Hash = ((uint64)v->Type << 60) | (v->vInt & 0xFFFFFFFFFFFFFFF); } k.Value = v;
	#elif defined(__i386__) || defined(_M_IX86)
		// 32 bit
		#define MAKE_COLL_KEY(k, v) if ( v->Type == XVO_DT_TEXT ) { uint32 iHash = xrtHash32(v->vText, v->Size); k.Hash = ((uint64)v->Type << 60) | ((uint64)v->Size << 28) | (iHash & 0xFFFFFFF); } else if ( v->Type == XVO_DT_BOOL ) { k.Hash = ((uint64)v->Type << 60) | v->vBool; } else if ( v->Type == XVO_DT_NULL ) { k.Hash = (uint64)v->Type << 60; } else { k.Hash = ((uint64)v->Type << 60) | (v->vInt & 0xFFFFFFFFFFFFFFF); } k.Value = v;
	#endif
	
	// Coll 内联写数据
	static inline bool xvoCollSetValueWithKey(xavltree pColl, Coll_Key* objKey, bool bColloc)
	{
		bool bNew;
		Coll_Key* pNode = xrtAVLTreeInsert(pColl, objKey, &bNew);
		if ( pNode ) {
			if ( bNew ) {
				pNode->Hash = objKey->Hash;
				pNode->Value = objKey->Value;
				if ( bColloc == FALSE ) {
					xvoAddRef_Inline(objKey->Value);
				}
			} else {
				if ( bColloc ) {
					xvoUnref(objKey->Value);
				}
			}
			return TRUE;
		}
		return FALSE;
	}
	
	// Coll 写数据
	XXAPI bool xvoCollSetValue(xvalue pColl, xvalue pVal, bool bColloc);
	#define xvoCollSetNull(pList)																xvoCollSetValue(pList, xvoCreateNull(), TRUE)
	#define xvoCollSetBool(pList, bVal)															xvoCollSetValue(pList, xvoCreateBool(bVal), TRUE)
	#define xvoCollSetInt(pList, iVal)															xvoCollSetValue(pList, xvoCreateInt(iVal), TRUE)
	#define xvoCollSetFloat(pList, fVal)														xvoCollSetValue(pList, xvoCreateFloat(fVal), TRUE)
	#define xvoCollSetText(pList, sVal, iSize, bColloc)											xvoCollSetValue(pList, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoCollSetTime(pList, tVal)															xvoCollSetValue(pList, xvoCreateTime(tVal), TRUE)
	#define xvoCollSetTimeSerial(pList, iYear, iMonth, iDay, iHour, iMinute, iSecond)			xvoCollSetValue(pList, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoCollSetPoint(pList, pVal)														xvoCollSetValue(pList, xvoCreatePoint(pVal), TRUE)
	#define xvoCollSetFunc(pList, func)															xvoCollSetValue(pList, xvoCreateFunc(func), TRUE)
	#define xvoCollSetCustom(pList, point)														xvoCollSetValue(pList, xvoCreateCustom(point), TRUE)
	
	// Coll 获取差集 [ pSelf 集合相对 pColl 集合不存在的元素 ]
	XXAPI xvalue xvoCollDifference(xvalue pSelf, xvalue pColl);
	
	// Coll 获取对称差集 [ 两个集合中不重复的元素 ]
	XXAPI xvalue xvoCollSymmetricDifference(xvalue pSelf, xvalue pColl);
	
	// Coll 获取交集 [ pSelf 集合相对 pColl 集合存在的元素 ]
	XXAPI xvalue xvoCollIntersection(xvalue pSelf, xvalue pColl);
	
	// Coll 获取并集 [ 合并两个集合，返回和一个新的集合 ]
	XXAPI xvalue xvoCollUnion(xvalue pSelf, xvalue pColl);
	
	// Coll 合并集合 [ 将 pColl 中的元素并入 pSelf ]
	XXAPI bool xvoCollMerge(xvalue pSelf, xvalue pColl);
	
	// Coll 操作
	XXAPI bool xvoCollExists(xvalue pColl, xvalue pVal);
	XXAPI bool xvoCollRemove(xvalue pColl, xvalue pVal);
	XXAPI uint32 xvoCollItemCount(xvalue pColl);
	XXAPI bool xvoCollClear(xvalue pColl);
	XXAPI bool xvoCollSetParent(xvalue pColl, xvalue pParentColl);
	
	// Table 读数据
	XXAPI xvalue xvoTableGetValue(xvalue pTbl, str key, uint32 kl);
	#define xvoTableGetBool(pTbl, key, kl)														xvoGetBool(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetInt(pTbl, key, kl)														xvoGetInt(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetFloat(pTbl, key, kl)														xvoGetFloat(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetText(pTbl, key, kl)														xvoGetText(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetTime(pTbl, key, kl)														xvoGetTime(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetPoint(pTbl, key, kl)														xvoGetPoint(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetFunc(pTbl, key, kl)														xvoGetFunc(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetArray(pTbl, key, kl)														xvoGetArray(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetList(pTbl, key, kl)														xvoGetList(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetColl(pTbl, key, kl)														xvoGetColl(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetTable(pTbl, key, kl)														xvoGetTable(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetStruct(pTbl, key, kl)													xvoGetStruct(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetObject(pTbl, key, kl)													xvoGetObject(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetCustom(pTbl, key, kl)													xvoGetCustom(xvoTableGetValue(pTbl, key, kl))
	
	// Table 写数据
	XXAPI bool xvoTableSetValue(xvalue pTbl, str key, uint32 kl, xvalue pVal, bool bColloc);
	#define xvoTableSetNull(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateNull(), TRUE)
	#define xvoTableSetBool(pTbl, key, kl, bVal)												xvoTableSetValue(pTbl, key, kl, xvoCreateBool(bVal), TRUE)
	#define xvoTableSetInt(pTbl, key, kl, iVal)													xvoTableSetValue(pTbl, key, kl, xvoCreateInt(iVal), TRUE)
	#define xvoTableSetFloat(pTbl, key, kl, fVal)												xvoTableSetValue(pTbl, key, kl, xvoCreateFloat(fVal), TRUE)
	#define xvoTableSetText(pTbl, key, kl, sVal, iSize, bColloc)								xvoTableSetValue(pTbl, key, kl, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoTableSetTime(pTbl, key, kl, tVal)												xvoTableSetValue(pTbl, key, kl, xvoCreateTime(tVal), TRUE)
	#define xvoTableSetTimeSerial(pTbl, key, kl, iYear, iMonth, iDay, iHour, iMinute, iSecond)	xvoTableSetValue(pTbl, key, kl, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoTableSetPoint(pTbl, key, kl, pVal)												xvoTableSetValue(pTbl, key, kl, xvoCreatePoint(pVal), TRUE)
	#define xvoTableSetFunc(pTbl, key, kl, func)												xvoTableSetValue(pTbl, key, kl, xvoCreateFunc(func), TRUE)
	#define xvoTableSetArray(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateArray(), TRUE)
	#define xvoTableSetList(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateList(), TRUE)
	#define xvoTableSetColl(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateColl(), TRUE)
	#define xvoTableSetTable(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateTable(), TRUE)
	#define xvoTableSetStruct(pTbl, key, kl, size)												xvoTableSetValue(pTbl, key, kl, xvoCreateStruct(size), TRUE)
	#define xvoTableSetObject(pTbl, key, kl, size)												xvoTableSetValue(pTbl, key, kl, xvoCreateObject(size), TRUE)
	#define xvoTableSetCustom(pTbl, key, kl, point)												xvoTableSetValue(pTbl, key, kl, xvoCreateCustom(point), TRUE)
	
	// Table 合并
	XXAPI bool xvoTableMerge(xvalue pTbl1, xvalue pTbl2, bool bReWrite);
	
	// Table 操作
	XXAPI bool xvoTableExists(xvalue pTbl, str key, uint32 kl);
	XXAPI bool xvoTableRemove(xvalue pTbl, str key, uint32 kl);
	XXAPI uint32 xvoTableItemCount(xvalue pTbl);
	XXAPI bool xvoTableClear(xvalue pTbl);
	XXAPI bool xvoTableSetParent(xvalue pTbl, xvalue pParentTable);
	
	// 类型操作
	XXAPI bool xvoIsNull(xvalue pVal);
	XXAPI int xvoType(xvalue pVal);
	#define xvoArrayItemType(pArr, index)														xvoType(xvoArrayGetValue(pArr, index))
	#define xvoListItemType(pList, index)														xvoType(xvoListGetValue(pList, index))
	#define xvoTableItemType(pTbl, key, kl)														xvoType(xvoTableGetValue(pTbl, key, kl))
	
	// 获取数据长度
	XXAPI uint32 xvoGetSize(xvalue pVal);
	#define xvoArrayItemSize(pArr, index)														xvoGetSize(xvoArrayGetValue(pArr, index))
	#define xvoListItemSize(pList, index)														xvoGetSize(xvoListGetValue(pList, index))
	#define xvoTableItemSize(pTbl, key, kl)														xvoGetSize(xvoTableGetValue(pTbl, key, kl))
	
	// 浅拷贝
	XXAPI xvalue xvoCopy(xvalue pVal);
	
	// 深拷贝
	XXAPI xvalue xvoDeepCopy(xvalue pVal);
	
	// 输出 xte Value 的结构和值
	XXAPI void xvoPrintValue(xvalue objVal, int iLevel, int iMode, int64 iKey, str sKey);
	
	
	
	/* ------------------------------------ JNUM 函数库 ------------------------------------ */
	
	// 数值类型
	typedef enum {
		JNUM_NULL,
		JNUM_BOOL,
		JNUM_INT,
		JNUM_HEX,
		JNUM_LINT,
		JNUM_LHEX,
		JNUM_DOUBLE,
	} jnum_type_t;
	
	// 数值
	typedef union {
		bool vbool;
		int32_t vint;
		uint32_t vhex;
		int64_t vlint;
		uint64_t vlhex;
		double vdbl;
	} jnum_value_t;
	
	// 数字转字符串
	XXAPI int xrtI32ToStr(int32_t num, char* buffer);
	XXAPI int xrtI64ToStr(int64_t num, char* buffer);
	XXAPI int xrtU32ToStr(uint32_t num, char* buffer);
	XXAPI int xrtU64ToStr(uint64_t num, char* buffer);
	XXAPI int xrtNumToStr(double num, char* buffer);
	
	// 解析数字字符串
	XXAPI int xrtParseNum(const char *str, jnum_type_t *type, jnum_value_t *value);
	
	// 解析字符串
	static inline int xrtParseNumSkipSpace(const char *str, jnum_type_t *type, jnum_value_t *value)
	{
		const char *s = str;
		while (1) {
			switch (*s) {
			case '\b': case '\f': case '\n': case '\r': case '\t': case '\v': case ' ': ++s; break;
			default: goto next;
			}
		}
	next:
		return (int)(xrtParseNum(s, type, value) + (s - str));
	}
	
	// 字符串转数字
	XXAPI int32_t xrtStrToI32(const char* pStr);
	XXAPI int64_t xrtStrToI64(const char* pStr);
	XXAPI uint32_t xrtStrToU32(const char* pStr);
	XXAPI uint64_t xrtStrToU64(const char* pStr);
	XXAPI double xrtStrToNum(const char* pStr);
	
	
	
	/* ------------------------------------ JSON 函数库 ------------------------------------ */
	
	// 链表节点
	struct json_list {
		struct json_list *next;
	};
	
	// json对象的类型
	typedef enum {
		JSON_NULL = 0,              /* It doesn't has value variable: null */
		JSON_BOOL,                  /* Its value variable is vbool: true, false */
		JSON_INT,                   /* Its value variable is vint */
		JSON_HEX,                   /* Its value variable is vhex */
		JSON_LINT,                  /* Its value variable is vlint */
		JSON_LHEX,                  /* Its value variable is vlhex */
		JSON_DOUBLE,                /* Its value variable is vdbl */
		JSON_STRING,                /* Its value variable is vstr */
		JSON_ARRAY,                 /* Its value variable is head */
		JSON_OBJECT                 /* Its value variable is head */
	} json_type_t;
	
	// json对象的键或字符串类型的值的信息（LJSON使用此结构就知道了字符串长度，可以加快数据处理）
	typedef struct {
		uint32_t type:4;			// json对象的类型，只在作为json对象的键时才有效
		uint32_t escaped:1;			// 是否含转义字符
		uint32_t alloced:1;			// 是否是在堆中分配，只在SAX接口中有效
		uint32_t reserved:2;		// 保留位
		uint32_t len:24;			// 字符串长度
	} json_strinfo_t;
	
	// 带信息的字符串（LJSON使用此结构就知道了字符串长度，可以加快数据处理）
	typedef struct {
		char *str;					// 字符串数据
		json_strinfo_t info;		// 字符串信息
	} json_string_t;
	
	// json数字类型的值（LJSON支持长整数和十六进制数）
	typedef union {
		bool vbool;
		int32_t vint;
		uint32_t vhex;
		int64_t vlint;
		uint64_t vlhex;
		double vdbl;
	} json_number_t;
	
	// json对象的值（LJSON使用union管理对象的值从而节省内存空间）
	typedef union {
		json_number_t vnum;			// 数字类型的值
		char *vstr;					// 字符串类型的值
		struct json_list head;		// 集合对象的子节点挂载的链表头，指向最后一个元素(非空时)或自己(空时)
	} json_value_t;
	
	// json对象（LJSON使用更紧凑的内存结构以节省内存）
	typedef struct {
		struct json_list list;		// 链表节点，指向下一个对象或父对象的链表头
		char *key;					// json对象的键值，只有JSON_OBJECT的子对象才有键值
		json_strinfo_t ikey;		// key字符串信息(含json类型)
		json_strinfo_t istr;		// value.str字符串信息
		json_value_t value;			// json对象的值
	} json_object;
	
	// SAX APIs中指示JSON_ARRAY或JSON_OBJECT的开始和结束（集合类型是括号包起来的，JSON_SAX_START表示左边括号, JSON_SAX_FINISH指示右边括号）
	typedef enum {
		JSON_SAX_START = 0,
		JSON_SAX_FINISH
	} json_sax_cmd_t;
	
	// json对象的值（LJSON使用union管理对象的值从而节省内存空间）
	typedef union {
		json_number_t vnum;			// 数字类型的值
		json_string_t vstr;			// 字符串类型的值
		json_sax_cmd_t vcmd;		// SAX APIs指示集合对象的开始和结束
	} json_sax_value_t;
	
	// SAX解析器传递给回调函数的描述（LJSON SAX解析将维护用于状态机的深度信息。）
	typedef struct {
		int total;					// 数组深度的大小
		int index;					// JSON类型和键的当前索引
		json_string_t *array;		// 存储JSON对象类型和键的JSON深度信息
		json_sax_value_t value;		// json对象值
	} json_sax_parser_t;
	
	// SAX的回调函数（返回 `JSON_SAX_PARSE_CONTINUE` 表示继续解析；返回 `JSON_SAX_PARSE_STOP` 表示停止解析）
	typedef enum {
		JSON_SAX_PARSE_CONTINUE = 0,
		JSON_SAX_PARSE_STOP
	} json_sax_ret_t;
	typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);
	
	// 获取json_string_t的信息（str含有 `"  \  \b  \f  \n  \r  \t  \v` 字符时会设置escaped）
	XXAPI json_strinfo_t xrtJsonGetStringInfo(const char *str, const json_strinfo_t *orig);
	
	// 更新json_string_t的信息（jstr->info.len是0时数据才会更新）
	static inline void xrtJsonUpdateStringInfo(json_string_t *jstr)
	{
		if (!jstr->info.len)
			jstr->info = xrtJsonGetStringInfo(jstr->str, &jstr->info);
	}
	
	// 从字符串进行SAX解析（失败返回-1；成功返回0）
	XXAPI int xrtJsonParseSAX(str text, size_t str_len, json_sax_cb_t cb);
	
	// 打印缓冲
	typedef struct {
		size_t size;
		char *p;
	} json_print_ptr_t;
	
	// 打印参数设置
	typedef struct {
		size_t str_len;
		size_t plus_size;
		size_t item_size;
		int item_total;
		bool format_flag;
		json_print_ptr_t *ptr;
	} json_print_choice_t;
	
	// SAX打印句柄（实际就是 `json_sax_print_t` 的指针）
	typedef ptr json_sax_print_hd;
	
	// 启动SAX打印器；失败返回NULL，成功返回指针（SAX打印器的句柄）
	XXAPI json_sax_print_hd xrtJsonPrintStart(json_print_choice_t *choice);
	
	// 启动格式化的SAX打印器，输出到字符串
	static inline json_sax_print_hd json_sax_print_format_start(int item_total, json_print_ptr_t *ptr)
	{
		json_print_choice_t choice;

		memset(&choice, 0, sizeof(choice));
		choice.format_flag = true;
		choice.item_total = item_total;
		choice.ptr = ptr;
		return xrtJsonPrintStart(&choice);
	}
	
	// 启动压缩的SAX打印器，输出到字符串
	static inline json_sax_print_hd json_sax_print_unformat_start(int item_total, json_print_ptr_t *ptr)
	{
		json_print_choice_t choice;

		memset(&choice, 0, sizeof(choice));
		choice.format_flag = false;
		choice.item_total = item_total;
		choice.ptr = ptr;
		return xrtJsonPrintStart(&choice);
	}
	
	// SAX打印json对象
	XXAPI int xrtJsonPrintValue(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);
	static inline int xrtJsonPrintNull(json_sax_print_hd handle, json_string_t *jkey) { return xrtJsonPrintValue(handle, JSON_NULL, jkey, NULL); }
	static inline int xrtJsonPrintBool(json_sax_print_hd handle, json_string_t *jkey, bool value) { return xrtJsonPrintValue(handle, JSON_BOOL, jkey, &value); }
	static inline int xrtJsonPrintInt(json_sax_print_hd handle, json_string_t *jkey, int32_t value) { return xrtJsonPrintValue(handle, JSON_INT, jkey, &value); }
	static inline int xrtJsonPrintHex(json_sax_print_hd handle, json_string_t *jkey, uint32_t value) { return xrtJsonPrintValue(handle, JSON_HEX, jkey, &value); }
	static inline int xrtJsonPrintInt64(json_sax_print_hd handle, json_string_t *jkey, int64_t value) { return xrtJsonPrintValue(handle, JSON_LINT, jkey, &value); }
	static inline int xrtJsonPrintHex64(json_sax_print_hd handle, json_string_t *jkey, uint64_t value) { return xrtJsonPrintValue(handle, JSON_LHEX, jkey, &value); }
	static inline int xrtJsonPrintDouble(json_sax_print_hd handle, json_string_t *jkey, double value) { return xrtJsonPrintValue(handle, JSON_DOUBLE, jkey, &value); }
	static inline int xrtJsonPrintString(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value) { return xrtJsonPrintValue(handle, JSON_STRING, jkey, value); }
	static inline int xrtJsonPrintArray(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return xrtJsonPrintValue(handle, JSON_ARRAY, jkey, &value); }
	static inline int xrtJsonPrintObject(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return xrtJsonPrintValue(handle, JSON_OBJECT, jkey, &value); }
	
	#define xrtJsonPrintArrayNull(handle)         xrtJsonPrintNull  (handle, NULL, NULL)
	#define xrtJsonPrintArrayStart(handle, jkey)  xrtJsonPrintArray (handle, jkey, JSON_SAX_START)
	#define xrtJsonPrintArrayFinish(handle)       xrtJsonPrintArray (handle, NULL, JSON_SAX_FINISH)
	
	#define xrtJsonPrintObjectNull(handle, jkey)  xrtJsonPrintNull  (handle, jkey, NULL)
	#define xrtJsonPrintObjectStart(handle, jkey) xrtJsonPrintObject(handle, jkey, JSON_SAX_START)
	#define xrtJsonPrintObjectFinish(handle)      xrtJsonPrintObject(handle, NULL, JSON_SAX_FINISH)
	
	// 结束SAX打印器
	XXAPI char* xrtJsonPrintFinish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr);
	
	// 解析 JSON
	XXAPI xvalue xrtParseJSON(str sText, size_t iSize);
	XXAPI xvalue xrtParseJSON_File(str sFile);
	
	// 将 xvalue 转换为 JSON
	XXAPI str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);
	XXAPI int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);
	
	
	
	/* ------------------------------------ Template 函数库 ------------------------------------ */
	
	// 最大支持参数数量
	#define XTE_PARAM_MAXCOUNT		6
	
	// Token 定义编号
	#define XTE_TK_TEXT				0				// 文本内容
	#define XTE_TK_COMMEN			1				// 注释				{! * }
	#define XTE_TK_VAR				0x100			// 代入变量			{$ * : *}			参数：为 NULL 默认值
	#define XTE_TK_NUM				0x101			// 代入数字变量		{% * : *}			参数：格式
	#define XTE_TK_TIME				0x102			// 代入时间变量		{& * : *}			参数：格式
	#define XTE_TK_BOOL				0x103			// 根据逻辑代入值		{? * : * : * }		参数：为真时的值和为假时的值
	#define XTE_TK_ARR				0x104			// 代入数组			{* * : *}			参数：套用子模板
	#define XTE_TK_PROC				0x105			// 代入函数或流程		{@ * : * ...}		参数：函数参数列表
	#define XTE_TK_SUBTEMPLATE		0x106			// 代入子模板			{= * : * }			参数：子模板环境变量名
	#define XTE_TK_SYMBOL			0xFFFF			// 预定义符号			{# * : * ...}		参数：语句参数列表
	#define XTE_MODE_BLOCK			0xFFFE			// 特殊符号，表示进入数据块采集模式，以 {#end} 结尾
	
	// Token 扩展编号
	#define XTE_TK_INCLUDE			0x10000			// 引用外部文件
	#define XTE_TK_DEFINE			0x10001			// 定义子模板
	#define XTE_TK_SCRIPT			0x10002			// 脚本块
	#define XTE_TK_IF				0x20000			// 判断语句
	#define XTE_TK_ELSEIF			0x20001			// 判断语句
	#define XTE_TK_ELSE				0x20002			// 判断语句
	#define XTE_TK_FOR				0x30000			// 循环语句
	#define XTE_TK_FOREACH			0x30001			// 迭代循环语句
	#define XTE_TK_END				0xFFFFFF		// 语句结束
	#define XTE_TK_USER				0x1000000		// 大于这个编号的，XTE模板后续更新不会使用，可以安全的用于扩展
	
	#define XTE_IDTPE_DEFAULT		0				// 
	#define XTE_IDTPE_BLOCK			1
	
	// Ident Info 数据结构（用于定义标识符）
	typedef struct {
		char* Ident;								// 标识符
		uint32 TokenIndex;					// 对应的 Token 编号
		unsigned short Type;						// 0 = 单语句、1 = 独立语句块(以 {#end} 结尾)
		unsigned short Size;						// 标识符长度
		unsigned short MinParamCount;				// 最小参数数量
		unsigned short MaxParamCount;				// 最大参数数量
		uint32 Hash;							// 标识符哈希值
	} XTE_IdentInfo_Struct, *XTE_IdentInfo;
	
	// Token Item 数据结构
	typedef struct {
		uint32 Type;							// Token 定义编号
		char* Text;									// 关联文本
		size_t Size;								// 关联文本长度
		uint32 ParamCount;					// 参数数量
		char* ParamText[XTE_PARAM_MAXCOUNT];		// 参数文本
		uint32 ParamSize[XTE_PARAM_MAXCOUNT];	// 参数长度
		XTE_IdentInfo IdentInfo;					// 标识符语句对应的标识符信息结构体指针
		uint32 RefLine;						// 语句在源文件中所在行
		uint32 RefLinePos;					// 语句在源文件中所在行的位置
		uint32 RefPos;						// 语句在源文件中所在的位置
		uint32 RefSize;						// 语句在源文件中的长度
	} XTE_TokenItem_Struct, *XTE_TokenItem;
	
	// Token List 数据结构
	typedef struct {
		int Success;								// 解析是否成功
		int ErrorCode;								// 错误代码（0=成功）
		const char* ErrorDesc;						// 错误描述
		uint32 ErrorLine;						// 错误行号
		uint32 ErrorLinePos;					// 错误行位置
		uint32 ErrorPos;						// 错误位置
		uint32 ErrorRefLine;					// 出错参考行
		uint32 ErrorRefLinePos;				// 出错参考行位置
		uint32 ErrorRefPos;					// 错误参考位置
		xarray_struct Tokens;						// Token 列表
	} XTE_TokenList_Struct, *XTE_TokenList;
	
	// xTemplate Engine Lite 数据结构
	typedef struct {
		int Success;								// 解析是否成功
		int ErrorCode;								// 错误代码（0=成功）
		const char* ErrorDesc;						// 错误描述
		uint32 ErrorLine;						// 错误行号
		uint32 ErrorLinePos;					// 错误行位置
		uint32 ErrorPos;						// 错误位置
		uint32 ErrorRefLine;					// 出错参考行
		uint32 ErrorRefLinePos;				// 出错参考行位置
		uint32 ErrorRefPos;					// 错误参考位置
		xarray Tokens;								// Token 列表
		xparray_struct Actions;						// 编译后的动作列表
		xdict_struct SubTemplates;					// 子模板列表（哈希表）
	} XTE_LiteStruct, *XTE_LiteObject;
	
	// 创建关键字列表（失败返回 NULL）
	XXAPI xarray xteCreateIdentList();
	
	// 销毁关键字列表
	XXAPI void xteDestroyIdentList(xarray objList);
	
	// 添加一个关键字到列表
	XXAPI int xteAddIdentToList(xarray objList, char* sID, uint32 iSize, uint32 iIndex, uint32 iType, uint32 iMinParamCount, uint32 iMaxParamCount);
	
	// 释放 XTE_TokenList
	XXAPI void xteLexerFree(XTE_TokenList arrToken);
	
	// 解析模板文件为 Token 列表
	XXAPI XTE_TokenList xteLexer(char* sText, size_t iSize, xarray objIdentList, char* sBracket);
	
	// 将 XTE_TokenList 转换为 XTE_LiteObject（XTE_TokenList将被释放）
	XXAPI XTE_LiteObject xteParseFromTokenList(XTE_TokenList objToks);
	
	// 解析返回语法列表
	XXAPI XTE_LiteObject xteParse(char* sText, size_t iSize, char* sBracket);
	
	// 释放 XTE_LiteObject 对象
	XXAPI void xteParseFree(XTE_LiteObject objLite);
	
	// 根据 XTE_LiteObject 模板对象生成文档
	XXAPI char* xteMakeActions(xparray arrAction, XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblRoot, xvalue tblENV, xdict tblInclude, size_t* pRetSize);
	XXAPI char* xteMake(XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblENV, xdict tblInclude, size_t* pRetSize);
	
	
	
#endif


