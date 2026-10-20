# NetInterface 标准库接入

2026-10-20，P2 接口快照与本机信息查询批次。限定双平台源码验收完成；不据此关闭
网络 40 模块、P0/P1/P2 或五组长期任务，也不覆盖 shipping 产品。

## API 与原生功能归属

| xlang 入口 | 内容与 XRT 对应 |
|---|---|
| `net.interfaces()`、`NetInterfaceList()` | `xrtNetInterfaces` 的一致快照，语言自动释放 |
| 列表 `count()`、`length`、`get(index)`、`[index]` | 数量与独立拥有的条目；负数／越界 checked RangeError |
| `NetInterface.name()`、`displayName()` | 带准确长度、经过 UTF-8 验证的独立 string |
| `ipv4Index`、`ipv6Index`、`flags`、`mtu` | 只读整数元数据 |
| `hasFlag(mask)` 与六个具名布尔属性 | Up、Running、Loopback、Broadcast、PointToPoint、Multicast |
| `hardwareAddress()` | 原始硬件地址的独立 bytes，不假定固定六字节 |
| `addressCount()`、`address(index)` | 地址数量与独立的 NetInterfaceAddress |
| 地址条目的 `family`、`scope`、`prefixLength`、`prefixKnown` | 前缀未知为 255；已知 IPv4 ≤32、IPv6 ≤128 |
| 地址条目的 `text()`、`address()` | 数字文本和独立 NetAddress，端口为 0 |
| `net.interfaceIndex(name, family = 0)` | `xrtNetInterfaceIndex`；找不到／不唯一保留 XRT checked 错误 |
| `net.interfaceName(index, family = 0)` | `xrtNetInterfaceName` 的完整字符串出口，不交付截断结果 |
| `net.hostName()`、现有 `net.localName()` | `xrtNetHostName`；两种拼写直接调用同一驻留实现 |
| `net.localAddress(family = 0)`、`net.localIP(family = 0)` | XRT 偏好本机单播地址，分别返回 NetAddress／string |
| `net.localHardware()`、`net.localMAC()` | 原始 bytes／XRT 大写紧凑 HEX 文本 |

本批覆盖 `net_interface`、`net_interface_text` 的安全语言功能面，不机械公开
裸输出指针、预检容量、平台结构或重复的 C 分配变体。仅复用固定的 XRT 实现，
没有另造枚举、地址选择、名称比较或 HEX 算法。

`family` 选择器接受 0／4／6；接口索引必须为 1—4294967295。名字是完整的
控制字段，拒绝空名字和内嵌 NUL，不接受截断 size。`hasFlag` 判断所有指定
位是否存在，0 返回 true；负数或超出 uint32 的 mask 返回 false，保持只读
无抛错。缺少可用硬件地址时，便捷查询抛 LookupError，不冒充空字符串／bytes。

`localAddress/localIP` 是 XRT 的诊断展示偏好，不等于默认路由、公网地址或
服务监听策略。快照内元数据一致，多个全局查询之间不冻结操作系统状态。
名称／硬件地址变化导致缓冲增长时重新读取；适配层最多尝试八次，再 checked
报状态错误，不无限循环或返回半成品。

## 生命周期、效果与代码生成

列表拥有 XRT 原生快照；取出的接口使用一次分配复制该条目的名字、硬件地址
和地址数组，而不是复制整张列表或借用列表。地址条目及转换出的 NetAddress
也是独立副本。父列表／接口释放后仍可使用子结果；硬件 bytes 的修改不影响
快照。不增加共享 refcount、语言借用、裸句柄构造或 adopt。

三个资源都只包含原生 POD／字节存储，无语言子对象和回调，分别登记精确
leaf policy 与 typed ownership domain。缓存字段查询声明 pure、no-raise、
nonblocking；OS 查询和分配型入口保留 checked 错误和保守效果。

类和命名空间以驻留声明接入。长度、索引协议有独立且明确的 C 入口，复用
count/get 实现，不绕过原生类禁止重复绑定 C 符号的现有规则。Cgen 没有新增
任何这些库的实现函数体，也不通过 point→临时 Buffer→string 兜转。

驻留组件分开缓存快照、缓存文本、名称查询、本机便捷查询。只读主机名无需
创建接口类，纯数字 NetAddress 继续排除接口枚举；这些路径均不隐式启动
DNS、Resolver、Engine、TCP、UDP 或 TLS。沿用 XRT 原生模块的裁剪粒度。

## 同批压实的核心合同

1. 隐式 point 构造必须检查实际 native 字段的访问权，包含继承、protected、
   字段遮蔽；不再绕过 private。public 字段及显式授权的构造器仍按原规则。
2. 重载候选试探不能先发布不匹配回调的诊断；实际参数／赋值失败仍需诊断。
3. 函数类型构造统一使用 RETURN_TYPES，避免具名函数的 invocation LIST
   与 lambda／声明结构不同，导致相同签名不能参与泛型推导。没有改成宽松
   类型比较，也没有要求调用者绕行匿名函数或补新语法。
4. 运算／索引等 resolved-syntax 调用复用普通调用的物理返回类型查询和拥有
   转换事务；原生工厂先返回原始字段，再建立语言拥有者，随后清理接收者。
   不在后端对 NetInterface 特判或追溯源语义。

同步重生成静态标准库索引，并检查全部 996 个登记行在探测链内可达，保留
有意的重载组。三条旧 `__xrt_net` 裸指针登记及其依赖链已经移除。

## 验收口径

证据根：`F:/xlang-validation/five-groups-20261020/p2-net-interfaces`。
最终源码构建为 `source-win-r9`（634 编译单元）与 `source-linux-r8`（633）；
报告为 `interfaces-win-r7/results.json` 与 `interfaces-linux-r1/results.json`。
每端 49 阶段、3 个明确的语义／checked 拒绝，共 98 阶段；输入、日志和产物
哈希一致。它们证明当前源码驱动，不冒充仓库 shipping CLI／DLL／SO 验收。

真实本机元数据、JIT／module／独立 AOT、严格 C O0/O2、索引／长度／临时
父对象、具名 TaskPool 回调返回拥有快照及数字地址回归均通过。每端每种优化
还检查全部 996 个登记行可达、13 项访问权／回调语义，以及 374 个无效 IR
合同在 C/header 输出前拒绝。原生单测遍历 6 个复制／装箱分配故障前缀和实际
OS 快照分配前缀（Windows 3、Linux 2），验证独立副本、精确账本及 leaf policy。
独立 AOT 导入检查在 `seal.py` 中执行，结果另存 `sealed-*-imports.log`；不依赖
`xl.dll`／`xlc.dll` 或 `libxl.so`／`libxlc.so`。

子调用受控替身只用于原生适配单测的尺寸增长与重试上限；公共 JIT／module／
AOT 使用真实提供者。增量调试 driver 及早期失败／输入变化的轮次不计正式验收。
全程仅查询本机元数据，没有外部 DNS、连接、探测或网络安全性质专项。

复跑：`python tools/check_stdlib_net_interfaces.py --source-build <完整源码构建/build.json>
--out <新目录>`；Linux 另传匹配的 `--native-root`。
