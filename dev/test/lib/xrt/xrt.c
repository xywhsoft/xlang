


#include "xrt.h"



#if defined(_WIN32) || defined(_WIN64)
	#ifdef __TINYC__
		#include <winapi/winsock2.h>
		#include "windows.h"
		#include <winapi/shellapi.h>
		#include <winapi/iphlpapi.h>
		ULONGLONG GetTickCount64();
	#else
		#include <winsock2.h>
		#include "windows.h"
		#include <shellapi.h>
		#include <iphlpapi.h>
	#endif
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <wchar.h>
	#pragma comment (lib, "shell32")
	#pragma comment (lib, "Ws2_32")
	#pragma comment (lib, "IPHLPAPI")
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <net/if.h>
	#include <sys/ioctl.h>
	#include <errno.h>
	#include <wchar.h>
	#include <netdb.h>
	#include <dirent.h>
	#include <sys/wait.h>
#endif



// 全局数据
const int sNullValue = 0;
xrtGlobalData xCore = { FALSE };

// TLS 数据
#ifdef XRT_USE_WIN_TLS_API
	// TCC 使用 Windows TLS API
	static DWORD xrt_tls_key = TLS_OUT_OF_INDEXES;
#else
	// 其他编译器使用 TLS 关键字
	static XRT_TLS xrtThreadLocal* xrt_tls_data = NULL;
#endif

// 获取当前线程的 TLS 数据
XXAPI xrtThreadLocal* xrtGetTLS()
{
	#ifdef XRT_USE_WIN_TLS_API
		if ( xrt_tls_key == TLS_OUT_OF_INDEXES ) {
			return NULL;
		}
		xrtThreadLocal* tls = (xrtThreadLocal*)TlsGetValue(xrt_tls_key);
		if ( tls == NULL ) {
			// 线程未初始化 TLS，自动初始化
			xrtInitTLS();
			tls = (xrtThreadLocal*)TlsGetValue(xrt_tls_key);
		}
		return tls;
	#else
		if ( xrt_tls_data == NULL ) {
			// 线程未初始化 TLS，自动初始化
			xrtInitTLS();
		}
		return xrt_tls_data;
	#endif
}



// 引入补充依赖库
#include "lib/suplib.h"



// 引入子库
#include "lib/base.h"
#include "lib/charset.h"
#include "lib/os.h"
#include "lib/math.h"
#include "lib/string.h"
#include "lib/path.h"
#include "lib/time.h"
#include "lib/file.h"
#include "lib/thread.h"
#include "lib/hash.h"
#include "lib/network.h"
#include "lib/xid.h"
#include "lib/buffer.h"
#include "lib/array_point.h"
#include "lib/array.h"
#include "lib/bsmm.h"
#include "lib/memunit.h"
#include "lib/mempool_fs.h"
#include "lib/stack.h"
#include "lib/stack_dyn.h"
#include "lib/llist_base.h"
#include "lib/llist.h"
#include "lib/avltree_base.h"
#include "lib/avltree.h"
#include "lib/mempool.h"
#include "lib/dict.h"
#include "lib/list.h"
#include "lib/value.h"
#include "lib/jnum.h"
#include "lib/json.h"
#include "lib/template.h"



// TLS 随机数初始化辅助函数
static void xrt_pcg32_srandom(xrt_pcg32_t* rng, uint64 initstate, uint64 initseq)
{
	rng->state = 0U;
	rng->inc = (initseq << 1u) | 1u;
	// 运算一次
	uint64 oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
	rng->state += initstate;
	// 再运算一次
	oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
}

// 初始化当前线程的 TLS
XXAPI void xrtInitTLS()
{
	xrtThreadLocal* tls = NULL;
	
	#ifdef XRT_USE_WIN_TLS_API
		// TCC: 使用 Windows TLS API
		if ( xrt_tls_key == TLS_OUT_OF_INDEXES ) {
			return;
		}
		tls = (xrtThreadLocal*)TlsGetValue(xrt_tls_key);
		if ( tls != NULL ) {
			return; // 已经初始化
		}
		tls = (xrtThreadLocal*)malloc(sizeof(xrtThreadLocal));
		if ( tls == NULL ) {
			return;
		}
		TlsSetValue(xrt_tls_key, tls);
	#else
		// 其他编译器: 使用 TLS 关键字
		if ( xrt_tls_data != NULL ) {
			return; // 已经初始化
		}
		tls = (xrtThreadLocal*)malloc(sizeof(xrtThreadLocal));
		if ( tls == NULL ) {
			return;
		}
		xrt_tls_data = tls;
	#endif
	
	// 初始化 TLS 数据
	tls->bInit = TRUE;
	tls->sRet = xCore.sNull;
	tls->iRet = 0;
	tls->nRet = 0.0;
	tls->LastError = xCore.sNull;
	tls->__pri_FreeError = FALSE;
	
	// 初始化环形临时内存
	for ( int i = 0; i < 32; i++ ) {
		tls->TempMem[i] = NULL;
	}
	tls->TempMemIdx = 0;
	
	// 初始化随机数生成器 (使用线程ID和时间戳作为种子)
	uint64 iTick = 0;
	#if defined(_WIN32) || defined(_WIN64)
		iTick = GetTickCount64() ^ (uint64)GetCurrentThreadId();
	#else
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		iTick = ((uint64)timer.tv_sec << 30) | timer.tv_nsec;
		iTick ^= (uint64)pthread_self();
	#endif
	
	xrt_pcg32_srandom(&tls->Rand32, iTick, 0xda3e39cb94b95bdbULL ^ iTick);
	xrt_pcg32_srandom(&tls->Rand64Low, iTick * 0x5851f42d4c957f2dULL, 0xda3e39cb94b95bdbULL);
	xrt_pcg32_srandom(&tls->Rand64High, iTick ^ 0x14057b7ef767814fULL, 0x14057b7ef767814fULL);
}

// 释放当前线程的 TLS
XXAPI void xrtUnitTLS()
{
	xrtThreadLocal* tls = NULL;
	
	#ifdef XRT_USE_WIN_TLS_API
		if ( xrt_tls_key == TLS_OUT_OF_INDEXES ) {
			return;
		}
		tls = (xrtThreadLocal*)TlsGetValue(xrt_tls_key);
		if ( tls == NULL ) {
			return;
		}
	#else
		tls = xrt_tls_data;
		if ( tls == NULL ) {
			return;
		}
	#endif
	
	// 释放错误信息
	if ( tls->__pri_FreeError && tls->LastError && tls->LastError != xCore.sNull ) {
		free(tls->LastError);
		tls->LastError = xCore.sNull;
	}
	
	// 释放环形临时内存
	for ( int i = 0; i < 32; i++ ) {
		if ( tls->TempMem[i] ) {
			free(tls->TempMem[i]);
			tls->TempMem[i] = NULL;
		}
	}
	tls->TempMemIdx = 0;
	
	// 释放 TLS 结构
	#ifdef XRT_USE_WIN_TLS_API
		free(tls);
		TlsSetValue(xrt_tls_key, NULL);
	#else
		free(tls);
		xrt_tls_data = NULL;
	#endif
}



// 初始化 xCore
XXAPI xrtGlobalData* xrtInit()
{
	if ( xCore.bInit ) {
		return &xCore;
	}
	
	// 初始化 TLS 支持
	#ifdef XRT_USE_WIN_TLS_API
		xrt_tls_key = TlsAlloc();
		if ( xrt_tls_key == TLS_OUT_OF_INDEXES ) {
			return NULL; // TLS 初始化失败
		}
	#endif
	
	// 初始化数据
	xCore.bInit = TRUE;
	xCore.sNull = (str)&sNullValue;
	xCore.sRet = xCore.sNull;
	xCore.iRet = 0;
	xCore.nRet = 0.0;
	xCore.LastError = xCore.sNull;
	xCore.__pri_FreeError = FALSE;
	xCore.OnError = NULL;
	
	// 初始化内存函数
	xCore.malloc = malloc;
	xCore.calloc = calloc;
	xCore.realloc = realloc;
	xCore.free = free;
	
	// 初始化环形临时内存 (兼容性保留)
	for ( int i = 0; i < 32; i++ ) {
		xCore.TempMem[i] = NULL;
	}
	xCore.TempMemIdx = 0;
	
	// 初始化高精度时钟频率单位
	#if defined(_WIN32) || defined(_WIN64)
		LARGE_INTEGER QPF;
		if ( QueryPerformanceFrequency( &QPF ) ) {
			xCore.Frequency = QPF.QuadPart;
		} else {
			xCore.Frequency = 0;
		}
	#endif
	
	// 初始化主线程的 TLS
	xrtInitTLS();
	
	// 获取程序文件名和路径
	#if defined(_WIN32) || defined(_WIN64)
		u16str sTemp = malloc(4096 * sizeof(wchar_t));
		int iSize = GetModuleFileNameW(NULL, sTemp, 4096);
		xCore.AppFile = xrtUTF16to8(sTemp, iSize);
		free(sTemp);
		xCore.AppPath = xrtPathGetDir(xCore.AppFile, xCore.iRet);
	#else
		str sTemp = malloc(4096);
		ssize_t iSize = readlink("/proc/self/exe", sTemp, 4096);
		if ( iSize == -1 ) {
			// 无法读取程序路径
			free(sTemp);
			xCore.AppFile = xCore.sNull;
			xCore.AppPath = xCore.sNull;
		} else {
			xCore.AppFile = xrtCopyStr(sTemp, iSize);
			free(sTemp);
			xCore.AppPath = xrtPathGetDir(xCore.AppFile, xCore.iRet);
		}
	#endif
	
	// 初始化 socket
	#if defined(_WIN32) || defined(_WIN64)
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	#endif
	
	// 获取本机 IP
	xCore.LocalAddr = xrtGetLocalRawIP();
	
	return &xCore;
}



// 释放 xCore
XXAPI void xrtUnit()
{
	if ( xCore.bInit ) {
		// 释放主线程的 TLS
		xrtUnitTLS();
		
		// 释放 TLS 支持
		#ifdef XRT_USE_WIN_TLS_API
			if ( xrt_tls_key != TLS_OUT_OF_INDEXES ) {
				TlsFree(xrt_tls_key);
				xrt_tls_key = TLS_OUT_OF_INDEXES;
			}
		#endif
		
		// 释放应用路径
		xrtFree(xCore.AppFile);
		xCore.AppFile = xCore.sNull;
		xrtFree(xCore.AppPath);
		xCore.AppPath = xCore.sNull;
		
		// 释放错误描述 (兼容性保留)
		if ( xCore.__pri_FreeError && xCore.LastError ) {
			xrtFree(xCore.LastError);
			xCore.LastError = xCore.sNull;
			xCore.__pri_FreeError = FALSE;
		}
		
		// 释放环形临时内存 (兼容性保留)
		xrtFreeTempMemory();
		
		// 重置初始化标记
		xCore.bInit = FALSE;
		
		// 释放 socket
		#if defined(_WIN32) || defined(_WIN64)
			WSACleanup();
		#endif
	}
}



#ifdef DBUILD_DLL
	
	
	
	#if defined(_WIN32) || defined(_WIN64)
		BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
		{
			if ( fdwReason == DLL_PROCESS_ATTACH ) {
				//当进程加载dll时调用dllMain
				xCoreInit();
			} else if ( fdwReason == DLL_PROCESS_DETACH ) {
				//当进程卸载dll时调用dllMain
				xCoreUnit();
			} else if ( fdwReason == DLL_THREAD_ATTACH ) {
				//当线程加载dll时调用dllMain
				
			} else if ( fdwReason == DLL_THREAD_DETACH ) {
				//当线程卸载dll时调用dllMain
				xrtUnitTLS();  // 释放线程的 TLS 数据
			}
			return (TRUE);
		}
	#else
	#endif
	
	
	
#endif


