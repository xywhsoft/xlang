# Socket 数据报接入

本批公开 `NetDatagramInfo`、不可变 `NetDatagramControl` 和 Socket 数据报方法，
复用 XRT，不在 Cgen 中实现协议或包装函数体。它不是完整网络模块封板。

## 值与调用

- `NetDatagramInfo()` 是可复用接收输出。`hasPacket` 表示是否有成功接收快照，
  `clear()` 清除快照；`remote`、`destination` 返回独立拥有的地址副本。
  缺少对应元数据时，字段读取 checked 报错，不伪造零值。
- `NetDatagramControl()` 表示默认发送控制；`withSource`、`withInterface`、
  `withHopLimit`、`withTrafficClass`、`withSegmentSize` 返回新值，不修改原对象。
  `without` 同时清除标志和相应字段。非法范围在分配前拒绝。
- `metadataAvailable()`、`metadataEnabled()`、`controlAvailable()`、
  `datagramCapabilities()` 使用不同的名义枚举，不允许互相冒充。
  `enableMetadata` 只接受已知、可启用的元数据标志，不接受 `truncated`。
- `receiveFromInto(buffer, info, maxBytes)` 返回 `(NetStatus, uint64)`。
  `sendMessage(data, target, control, size=-1)` 发送到显式目标；
  `sendMessage(data, control, size=-1)` 使用已连接目标。两个版本都支持
  `string`、`bytes`、`Buffer`；显式目标版本拒绝空目标，绝不偷偷切换语义。
- `size=-1` 使用实际字节长度，非负值截断到实际长度，其余负值报错。
  不调用 `strlen`，内嵌 NUL 不是结束标志。
- 多播成员加入／离开、出接口、loop 和 hop limit 通过 Socket 独占控制借用调用
  原生实现。本批未验证成员加入／离开或多播流量。

## 生命周期与错误边界

接收在读取系统数据前完成 Buffer 容量预留。预留失败不消费数据报；
`again`／错误保留 Buffer 逻辑内容与 Info 快照，容量允许增长。
只有 `ok`／`truncated` 提交逻辑长度与新快照。零长数据报是有效数据报，
不解释为流式 EOF；`maxBytes=0` 仍可消费零长数据报。

Socket I/O 和查询共享受管理借用；修改配置使用独占控制借用。
Windows Provider 查询入口由固定 XRT 的原子缓存发布，不在语言层另设缓存。
Info 与 Buffer 是可变对象，调用者必须保证内容的排他访问，不能与另一次
接收或字段读取并发修改。Control 只读共享，不保留 Socket、Buffer 或语言回调。

IPv6 目的地址元数据保留接收接口作为 `Scope`，即使地址是 `::1`；其端口为零。
`sameIP` 按既有合同同时比较 IPv6 Scope，不能假设它与未带 Scope 的绑定地址相等。

## 裁剪与验收边界

值、控制选项、I/O 是独立组件，拥有域随精确闭包登记。只使用值不根入 Socket、
Buffer 或执行器；控制选项不根入 Buffer；I/O 不强制根入控制选项实现。
测试另设 I/O＋控制选项的组合根，不能为测试方便修改生产依赖。

原生验证脚本 `tools/check_stdlib_net_datagram_runtime.py`：

- Windows GCC／TCC O0／O2，四类闭包共 48 阶段；
- Linux 同矩阵及四类实际闭包 ASan／UBSan，共 56 阶段；
- 覆盖叶句柄 claim／restore／clear／finish、内存平衡、代表性分配失败、
  自建 IPv4／IPv6 回环端点、元数据、六种发送形式、截断与空数据报；
- 证据位于 `F:/xlang-validation/five-groups-20261020/p2-datagram/runtime-win-r4`
  和 `runtime-linux-r4`，输入、日志、产物均有固定哈希。

完整工作区源码驱动 Windows 657／Linux 656 单元；语言验证使用
`check_stdlib_net_socket_language.py --datagram`，两端各 91 阶段通过。
8 个用例覆盖值对象与 I/O、既有 TCP／UDP、标量产品和 Atomic 回归；
逐例检查 IR、严格 C O0／O2、直接执行、模块、独立 AOT 和动态依赖。
AOT 不依赖 `xl.dll`／`xlc.dll`／`libxl.so`／`libxlc.so`，值对象 AOT 不根入 Socket。
每级数据报合同检查 40 个目标、7 个 status/count 产品、25 个拥有型参数位置和
8 个枚举位置，拒绝 226 项损坏合同且不输出 C／头文件；既有 Socket 合同同步
扩大到 45 个目标、14 个产品和 323 项拒绝。证据在同目录的
`source-win-r2`／`source-linux-r2` 与 `language-win-r1`／`language-linux-r1`。

这些是工作区源码与驻留闭包验证，不是 shipping DLL／SO、完整 OOM 穷尽或完整网络认证。
无外部服务、DNS、多播流量、网络安全性质测试。

数字枚举的位运算组合仍待用户决定；现有单标志和同类型查询结果可直接传递。
语言集成测试暂用已有的显式整数转换检查位，不改变语言枚举规则。
向量／批量、错误队列／PMTU、完整多播验收、Resolver／Future、完整传输、
密钥／X.509／TLS、旧 28 组守卫和 shipping 联合验收继续开放。
