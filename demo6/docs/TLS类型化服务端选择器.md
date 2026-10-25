# TLS 类型化服务端选择器

本批将服务端身份选择接入 `TlsSession`（sans-I/O），复用 XRT 的握手与身份实现。
这不是 TLS、五组标准库或发布产物的整体验收。原生暂存身份交接所需的
`xrtTlsServerChoiceSetIdentityV1` 已在前一批定点同步；本批不改变两边时间 ABI。

## 语言接口

```text
func choose(TlsServerRequest request) as TlsServerChoice?
    return TlsServerChoice(identity, uint64(123)).withProtocolIndex(0)
end

auto context = TlsServerContext(parameters, choose)
auto session = TlsSession(context)
```

也可由静态身份上下文调用 `withSelector(choose)`。`hasSelector` 可查询是否配置了
选择器；`withParameters`、`withIdentity`、`withProtocols` 保留它。
仅有动态选择器时，读取上下文的 `identity` 产生 checked 状态错误。

`TlsServerRequest` 不能由用户构造，也不能修改。`serverName: bytes`、
`protocolCount: int`、`protocolAt(index): bytes` 来自一份独立的拥有快照：
不保留原生 ClientHello 的借用视图，完整保存 ALPN 的内嵌 NUL 和任意字节。
读取返回独立字节值；请求可在回调、上下文及 Session 结束后继续使用。

`TlsServerChoice(identity, cookie = 0)` 是不可变的拥有值，保留实际身份。
`identity`、`cookie`、`hasProtocolIndex` 可读取；`withProtocolIndex(index)` 返回新选择：

- 不指定索引时沿用 XRT 的自动 ALPN 选择。
- `0` 及正数选择**服务端上下文**协议列表中的索引，不是客户端请求列表索引。
- `-1` 表示不选择协议；它仍受上下文 `requiresProtocol` 合同约束。
- 小于 `-1` 或不能表示的索引立即 checked 拒绝；具体列表越界由握手检查。
- 自动选择值的 `protocolIndex` 不可读取，产生 checked 状态错误。

回调返回 `null` 表示主动拒绝握手。语言异常经 checked 外部边界传播，
不能被后续原生失败或清理错误替换。回调中的已捕获错误不应污染外部结果。
回调同步执行在实际驱动握手的线程，不自动跳转执行器或分配协程栈；
调用者应避免在其中进行阻塞工作。

## 拥有权与生成代码

编译器封闭验证精确的 `(TlsServerRequest) -> TlsServerChoice?` checked 签名，
使用通用单参数原生类回调 ABI，不把函数指针当成不透明 `point`。
Cgen 只提供静态类 schema、真实代码拥有者及字段/策略元数据，然后生成调用和清理。
它不包含 TLS 分发算法、请求复制、身份转换或特例补丁。

驻留适配器以普通 V2 闭包拥有实际目标闭包、独立字段名和请求类模块租约；
请求从已接受的租约派生，模块关闭后不重新申请执行准入。
服务端上下文与 Session 各有真实的拥有边，均纳入原有图检查。
握手到达 ready/failed 后，Session 在自身 entry gate 内释放选择器捕获；
已交付的请求拥有独立类租约，不因提前释放捕获而失效。
选择结果在临时语言对象销毁前经原生 setter 交接身份拥有权。

闭包清理可能执行用户清理，因此上下文析构、Session 析构和 `dispose`
不能谎称 `no-raise` 或 `nonblocking`。

## 当前边界与后续

此批只接受内存中的 `TlsSession` 选择器。`TlsListener`/dial 组合对象的动态
选择器绑定尚未接入，传入选择器会明确拒绝，不能静默退回静态身份。
下一批需以组合对象原生拥有图、静止检查及 checked 退役合同完成绑定；
不能借用源上下文或只延长回调指针寿命。

票据/恢复、组合传输 KeyUpdate 和双平台 shipping DLL/VFS/嵌入资源整体验收
仍属长期任务，不得由本批局部结果替代。

## 验证入口

`tools/check_tls_selector_source.py` 使用全量实际源码编译器对象，批量检查
精确签名负例、typed/lowered IR、生成 C 的 GCC O0/O2 严格编译、JIT、模块、
独立 AOT 与空程序裁剪。固定证书的 RSA/P256/P384/Ed25519 握手在内存中完成，
另检查 nullable 拒绝和语言首错传播。不会访问外部端点或开展畸形密码学测试。

`test/cases/xlir_native_class_callback.c` 检查两个独立 CodeOwner 的 Close、
拥有请求和 256 个分配失败前缀；`xlir_tls_selector_contract.c` 变异检查
14 项目标与两项精确回调上下文。`tools/check_tls_selector_catalog.py`
实例化完整实际拥有目录，检查两种精确策略的唯一性与完整目录的 OOM 回滚。
测试完成状态以独立哈希证据报告为准，
不能将测试入口的存在等同于已通过。

2026-10-25 本批实际 Windows/Linux 源码验收各通过 93 步，共 186 步：
各端包含 87 项精确回调/生成/执行检查与 6 项完整目录检查。
源码编译器分别由 1252/1251 个实际编译单元构建。
两端合计检查 1024 个回调分配前缀、232 个目录分配前缀（包括首次不命中边界），
并独立复核全部输入、日志和产物哈希。
证据位于 `F:/xlang-validation/five-groups-20261025/p6-tls-selector/next.json`；
它只接受本批 Session 选择器，明确不接受 Listener 动态绑定或 shipping。
