


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
		#include <ws2tcpip.h>
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
#include "lib/avltree_base.h"
#include "lib/avltree.h"
#include "lib/mempool.h"
#include "lib/dict.h"
#include "lib/list.h"
#include "lib/value.h"
#include "lib/jnum.h"
#include "lib/json.h"
#include "lib/template.h"



// 初始化 xCore
XXAPI xrtGlobalData* xrtInit()
{
	if ( xCore.bInit ) {
		return &xCore;
	}
	
	// 初始化数据
	xCore.bInit = TRUE;
	xCore.sNull = (str)&sNullValue;
	xCore.LastError = xCore.sNull;
	xCore.__pri_FreeError = FALSE;
	xCore.OnError = NULL;
	
	// 初始化内存函数
	xCore.malloc = malloc;
	xCore.calloc = calloc;
	xCore.realloc = realloc;
	xCore.free = free;
	
	// 初始化环形临时内存
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
	
	// 初始化随机数生成器
	uint64 iTick = 0;
	#if defined(_WIN32) || defined(_WIN64)
		iTick = GetTickCount64() ^ (uint64)GetCurrentProcessId();
	#else
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		iTick = ((uint64)timer.tv_sec << 30) | timer.tv_nsec;
		iTick ^= (uint64)getpid();
	#endif
	xrtRandSeed(&xCore.rand32, iTick, 0xda3e39cb94b95bdbULL ^ iTick);
	xrtRandSeed(&xCore.rand64_low, iTick * 0x5851f42d4c957f2dULL, 0xda3e39cb94b95bdbULL);
	xrtRandSeed(&xCore.rand64_high, iTick ^ 0x14057b7ef767814fULL, 0x14057b7ef767814fULL);
	
	// 初始化约等于配置（整数容差万分之一、小数容差0.01、时间容差 10 秒、字符串相似度95%）
	xCore.iApproxIntMode = XRT_APPROX_PERCENT;
	xCore.fApproxIntTol = 0.0001;
	xCore.iApproxNumMode = XRT_APPROX_DIFF;
	xCore.fApproxNumTol = 0.01;
	xCore.iApproxTimeTol = 10;
	xCore.iApproxStrMode = XRT_STR_APPROX_SIM;  // 默认相似度模式
	xCore.fApproxStrTol = 0.95;                 // 默认95%相似度
	xCore.bApproxStrCase = FALSE;               // 默认区分大小写
	
	// 获取程序文件名和路径
	#if defined(_WIN32) || defined(_WIN64)
		u16str sTemp = malloc(4096 * sizeof(wchar_t));
		int iSize = GetModuleFileNameW(NULL, sTemp, 4096);
		size_t iRetSize = 0;
		xCore.AppFile = xrtUTF16to8(sTemp, iSize, &iRetSize);
		free(sTemp);
		xCore.AppPath = xrtPathGetDir(xCore.AppFile, iRetSize);
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
			xCore.AppPath = xrtPathGetDir(xCore.AppFile, iSize);
		}
	#endif
	
	// 初始化 socket
	#if defined(_WIN32) || defined(_WIN64)
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	#endif
	
	// 获取本机 IP
	xCore.LocalAddr = xrtGetLocalRawIP();
	
	// 初始化模板引擎
	xte_private_init();
	
	return &xCore;
}



// 释放 xCore
XXAPI void xrtUnit()
{
	if ( xCore.bInit ) {
		// 清理模板引擎
		xte_private_unit();
		// 释放应用路径
		xrtFree(xCore.AppFile);
		xCore.AppFile = xCore.sNull;
		xrtFree(xCore.AppPath);
		xCore.AppPath = xCore.sNull;
		
		// 释放错误描述
		if ( xCore.__pri_FreeError && xCore.LastError ) {
			xrtFree(xCore.LastError);
			xCore.LastError = xCore.sNull;
			xCore.__pri_FreeError = FALSE;
		}
		
		// 释放环形临时内存
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
				xrtInit();
			} else if ( fdwReason == DLL_PROCESS_DETACH ) {
				//当进程卸载dll时调用dllMain
				xrtUnit();
			}
			return (TRUE);
		}
	#else
	#endif
	
	
	
#endif


