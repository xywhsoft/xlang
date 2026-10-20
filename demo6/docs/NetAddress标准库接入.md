# NetAddress 标准库接入

2026-10-20：P2 地址基础批次已完成。它不是整个网络 40 模块或五组长期任务的完成声明。
复用仓库固定 XRT 的地址算法；没有修改原生 XRT，也没有在 Cgen 中增加实现函数体。

## 公开面

| 入口 | 含义 |
|---|---|
| `NetAddress()` | IPv4 未指定地址，端口 0 |
| `NetAddress.parse(ip, port = 0)` | 仅数字 IPv4／IPv6；不解析主机名或接口名 |
| `NetAddress.parseEndpoint(text, defaultPort = 0)` | 数字端点；IPv6 带端口使用 `[address]:port` |
| `NetAddress.any(family = 4, port = 0)` | 指定地址族的未指定地址 |
| `NetAddress.loopback(family = 4, port = 0)` | 指定地址族的回环地址 |
| `NetAddress(host, port = 0)`、`net.resolve(host, port)` | 单地址同步 DNS 解析；独立驻留组件 |
| `family`、`port`、`scope` | 只读字段，整数 |
| `text()`、`endpointText()` | 拥有的字符串，不暴露临时缓冲 |
| `bytes()` | 按网络顺序返回 4／16 字节的独立 `bytes` 值 |
| `equals(other)`、`sameIP(other)`、`compare(other)` | 内容相等、忽略端口比较、规范字段顺序比较 |
| `isUnspecified`、`isLoopback`、`isMulticast`、`isLinkLocal`、`isPrivate`、`isMapped` | XRT 的地址分类规则 |
| `unmap()`、`withPort(port)`、`withScope(scope)` | 返回新地址，不修改原地址 |

地址族只允许 `net.familyIPv4`（4）与 `net.familyIPv6`（6）。未指定族 0
只适用于 DNS 等选择器，不适用于创建具体地址。端口范围 0—65535；scope
范围 0—4294967295，IPv4 只接受 0。数值越界报 RangeError，非法族／文本报
checked 错误。控制字段完整处理，嵌入 NUL 被拒绝，不提供截断到部分主机名的 size。

数字 IPv6 scope 只接受十进制编号，不通过名字查接口；该规则不因产物是否
编入 NET_INTERFACE 而改变。`sameIP` 仍包含 IPv6 scope，只有端口被忽略。
`compare` 按族、地址字节、IPv6 scope、端口比较。普通语言对象 `==` 的既有
规则未改变；需要地址内容相等时显式使用 `equals`。

## 声明、效果和生命周期

- 标准库类仅有 private native handle。移除旧 `textNative` 裸指针入口，
  不提供裸句柄构造或 adopt；原有主机名构造仍为 DNS，不偷换成数字解析。
- 驻留地址对象拥有一份 `xnetaddr` POD 副本；不持有输入字符串、Engine、
  Resolver、回调或语言子对象。地址销毁策略是经审计的 leaf policy，并单独
  登记 typed ownership domain；不借用旧网络大域来扩大准入。
- 只读整数／布尔查询声明 `pure + no-raise + nonblocking`，对应有效语言
  handle 的精确合同；文本、字节、复制工厂允许分配失败，DNS 保持可阻塞。
- 注册表同时声明参数／结果物理 ABI、精确拥有者、析构策略和效果。Cgen
  消费已验证 IR，仅输出调用与清理，不生成地址解析或适配函数体。
- `NET_ADDRESS`、`NET_RESOLVE_ADDRESS` 是独立产物根。只使用数字地址不拉入
  DNS、接口枚举、Resolver、Engine、TLS、Executor 或协程调度器。
- NetAddressList 的旧声明已与单地址拆开，后续 Resolver／列表重构另做，
  不将它或现有接口／传输层假称已完成。

## 验收

证据根：`F:/xlang-validation/five-groups-20261020/p2-net-address`。
完整当前源码 driver：Windows `source-win-r1` 627 个编译单元、Linux
`source-linux-r1` 626 个。仅复用真实 TCC 核心，不替换编译器／runtime 对象。

最终报告：Windows `address-win-r5/results.json`、Linux
`address-linux-r3/results.json`，两端各 24 阶段、各 2 个预期语义拒绝：

- 数字 IPv4／IPv6、端点、scope、映射／unmap、分类、端口和字节副本；
  负例包括 NUL、主机名误用、接口名 zone 与数值越界。
- 实际 JIT、模块与独立 AOT 运行，生成 C 的 O0／O2 严格编译；数字主机
  输入另行验证同步 DNS provider，未查询外部主机。
- 每个合同产物检查 282 个无效 IR，均在 C／头文件输出前拒绝；覆盖效果、
  阻塞边界、错误、拥有字段、析构符号、policy 身份与参数投影。
- 原生地址只读查询保留已有错误；4 个分配失败前缀、句柄包装失败的消耗
  回滚、leaf trace 与精确分配账本通过，清理不覆盖首个失败。
- 独立产物 native module 清单证明按需裁剪；额外检查可执行文件动态依赖，
  不依赖 xl／xlc DLL／SO。输入、对象、头、产物与日志摘要在封存时复核。

早期失败轮次保留，但不计入通过；Linux 第一轮因测试输入发生变化而作废，
由冻结输入后的轮次替代。不宣称 shipping DLL／SO／VFS 提升、总 RC0、全部
网络功能或网络安全审计通过。没有连接任何其他本机服务或外部目标。

提交后补审纠正了裸句柄负例：早期 `point(0)` 会先触发不相关的整数到 point
转换错误，不能证明禁止 adopt。最终使用合法 `point raw = null` 后调用构造器，
并断言诊断为“没有匹配的 create 重载”；隐私负例亦核对具体拒绝原因。
修正测试后的全矩阵以以上 Windows 第五轮／Linux 第三轮为准，生产源码未改。

复跑：`python tools/check_stdlib_net_address.py --source-build <完整源码构建/build.json>
--out <全新目录>`；Linux 另传 `--native-root <匹配的原生 TCC 产品目录>`。
后续依次为接口、Socket／Engine 与显式后端选择、Resolver，再完成传输层。
