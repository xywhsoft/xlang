


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>





#include "inline_xrt.h"
#include "inline_libtcc.h"
#include "inline_mongoose.h"
#include "inline_sqlite3.h"
#include "inline_xdo.h"
#include "inline_md4c.h"
#include "inline_xtp.h"





// 服务器类型定义
#define SPT_NONE		0			// 未知服务
#define SPT_HTTP		1			// HTTP 服务
#define SPT_MQTT		2			// MQTT 服务
#define SPT_WS			3			// WebSocket 服务
#define SPT_XTP			4			// XTP 服务
#define SPT_TCP			0x10000		// TCP 协议
#define SPT_UDP			0x10001		// UDP 协议
#define SPT_CUSTOM		-1			// 自定义服务（事件驱动
#define SPT_THREAD		-2			// 单开一条线程运行的特殊服务



// 语言类型定义
#define SLT_STATIC		0		// 静态页
#define SLT_C			1		// C 语言
#define SLT_LUA			2		// Lua（未来可能会支持的）
#define SLT_JS			3		// JavaScript（未来可能会支持的）



// 服务器结构体
typedef struct {
	str Name;											// 主机名称
	str Desc;											// 主机描述
	str Param;											// 启动参数
	str Host;											// 主机地址（域名）
	bool DebugMode;										// 输出额外的信息
	struct mg_str TLS_CA;								// 主机 TLS CA 证书路径
	struct mg_str TLS_Cert;								// 主机 TLS 证书路径
	struct mg_str TLS_Key;								// 主机 TLS 秘钥路径
	str Path;											// 主机根目录
	int DevLang;										// 开发语言
	str DevFile;										// 开发文件，动态开发工程的总入口点
	ptr JsonNode;										// 配置文件的 JSON 对象
	ptr DevObj;											// 开发语言上下文对象
	ptr ServiceInit;									// 服务启动前调用（函数不存在则不会调用）
	ptr ServiceStart;									// 服务启动（HTTP、MQTT等内置逻辑的服务不会调用此函数，自定义服务函数不存在则不会调用）
	ptr ServiceUnit;									// 服务启动后调用（函数不存在则不会调用）
	ptr EventProc;										// 服务器网络事件回调（函数不存在则不会调用）
	ptr RequestProc;									// HTTP 请求回调（函数不存在则不会调用）
	void (*XS_SetGlobalDate)(int idx, void* ptr);		// XS 传递全局数据回调函数
} XS_HostStruct, *XS_HostObject;
typedef struct {
	int Class;											// 服务器类型（HTTP、MQTT、Custom、Thread、等）
	str Name;											// 服务器名称
	str Desc;											// 服务器描述
	str Param;											// 启动参数
	str Addr;											// 绑定地址端口
	int EnableTLS;										// 是否启用 TLS
	str AddrTLS;										// TLS 绑定地址端口
	int EnableDefaultHost;								// 是否启用默认 Host
	XS_HostStruct DefaultHost;							// 默认 Host
	uint32 HostCount;									// Host 数量
	xarray Hosts;										// Host 列表
	xdict HostMap;										// Host 字典（用于快速定位 Host 数据结构）
	ptr JsonNode;										// 配置文件的 JSON 对象
	struct mg_connection* Conn;							// mongoose 连接对象
	struct mg_connection* ConnTLS;						// mongoose 连接对象 TLS
} XS_ServerStruct, *XS_ServerObject;





#include "inline_other.h"





// Mongoose 事件管理结构
struct mg_mgr* mgr;

// 	全局数据 - 服务器列表
xarray ServerList;



// 使用 extern 可能导致数据指针出现变化，原因不明，先用这样的方式传递全局数据
void XS_SetGlobalDate(int idx, void* ptr)
{
	if ( idx == 1 ) {
		mgr = ptr;
	} else if ( idx == 2 ) {
		ServerList = ptr;
	} else if ( idx == 3 ) {
		xCore = ptr;
	}
}


