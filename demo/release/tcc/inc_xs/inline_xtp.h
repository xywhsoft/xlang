


// ==================== XTP 协议头文件 ====================
// 用于 TCC 动态脚本访问 XTP 协议功能



// XTP 报文头结构 (16 字节)
typedef struct {
	unsigned char HeadInfo[4];      // 包头标识，固定为 "xtp\1"
	unsigned int PackSize;          // 包总长度 (包含头部)
	unsigned short CmdSize;         // 命令文本长度
	unsigned short ParamCount;      // 参数数量
	unsigned int BodySize;          // Body 长度
} XTP_PackHeader;

// XTP 参数信息结构 (4 字节)
typedef struct {
	unsigned short KeySize;         // 参数名长度
	unsigned short ValSize;         // 参数值长度
} XTP_ParamInfo;

// XTP 消息结构 (传递给 EventProc 回调)
typedef struct {
	char* Cmd;                      // 命令名 (以 \0 结尾)
	unsigned short CmdSize;         // 命令名长度
	xdict Params;                   // 参数字典 (key -> value)
	unsigned short ParamCount;      // 参数数量
	char* Body;                     // Body 数据 (以 \0 结尾)
	unsigned int BodySize;          // Body 长度
} XTP_Message, *XTP_MessageObject;



// ==================== XTP 函数声明 ====================

// 发送 XTP 协议包
// 参数:
//   c           - 连接对象
//   sCmd        - 命令名
//   iCmdSize    - 命令名长度 (0 表示自动计算)
//   iParamCount - 参数数量
//   arrParam    - 参数名数组
//   arrValue    - 参数值数组
//   pBody       - Body 数据
//   iBodySize   - Body 长度 (0 表示自动计算字符串长度)
// 返回: TRUE=成功, FALSE=失败
int XTP_Send(struct mg_connection* c, str sCmd, size_t iCmdSize, 
             uint iParamCount, str* arrParam, str* arrValue, 
             ptr pBody, size_t iBodySize);

// 获取 XTP 消息的参数值
// 参数:
//   msg  - XTP 消息对象 (XTP_MessageObject)
//   sKey - 参数名
// 返回: 参数值字符串，不存在则返回 NULL
char* XTP_GetParam(void* msg, const char* sKey);


