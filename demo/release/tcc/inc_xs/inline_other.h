


// ==================== TCC 状态机管理 ====================

// 动态创建 TCC 状态机（与 xs 主程序一样的配置）
// sWorkPath: 工作目录（会添加到 include 和 library 路径），可为 NULL
// 返回创建好的 TCCState，失败返回 NULL
TCCState* xsCreateTCC(const char* sWorkPath);

// 销毁 TCC 状态机
void xsDestroyTCC(TCCState* s);



// ==================== 工具函数 ====================

// SQL 字符串转义（防止 SQL 注入）
char* sql_escape(const char* str, size_t len);

// HTTP 响应（根据 mg_http_reply 修改而来，不会进行 printf 代入）
void http_reply(struct mg_connection* c, int code, const char* headers, const char* sBody, size_t iSize);

// 解析 cookies
xdict ParseCookies(struct mg_http_message* hm);

// 释放 Cookies 表
void FreeCookies(xdict tblCookies);

// 将函数映射到 TCC 执行环境
void ImportAll(TCCState* s);



// ==================== 热加载函数 ====================

// 热加载指定 Host 的脚本
// 参数:
//   objServer - 服务器对象
//   objHost   - Host 对象
// 返回: 0=成功, 负数=失败
//   -1=非 C 语言, -2=文件不存在, -3=TCC 创建失败, -4=编译失败, -5=重定位失败
//   -100=参数无效
int xsReloadHost(XS_ServerObject objServer, XS_HostObject objHost);

// 通过域名热加载 Host 的脚本
// 参数:
//   objServer - 服务器对象
//   sDomain   - 域名（Host 配置中的 host 字段）
// 返回: 0=成功, 负数=失败
//   -101=域名未找到
int xsReloadHostByDomain(XS_ServerObject objServer, const char* sDomain);

// 热加载 DefaultHost 的脚本
// 参数:
//   objServer - 服务器对象
// 返回: 0=成功, 负数=失败
//   -102=DefaultHost 未启用
int xsReloadDefaultHost(XS_ServerObject objServer);

// 热加载整个 Server 的所有 Host
// 参数:
//   objServer - 服务器对象
// 返回: 成功加载的 Host 数量，负数=失败
int xsReloadServer(XS_ServerObject objServer);


