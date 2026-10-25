# TLS 恢复票据

`TlsResume` 是受语言生命周期管理的不可变恢复凭据，复用 XRT 的深拷贝、引用计数和最终清零实现。它不是连接，也不会自行发起网络操作。

本阶段只完成凭据对象：角色配置、服务端类型化查找回调、Session/传输层签发与接收仍待接入。不能把下面的对象验收等同于 TLS 恢复握手或发布封板已完成。

## 公开合同

高级恢复入口为：

```xlang
TlsResume.fromParts(TlsVersion version, TlsCipherSuite cipher,
    bytes ticket, bytes secret, time issuedAt, int lifetimeSeconds,
    uint64 ageAdd = 0, string serverName = "", bytes protocol = b"",
    bytes peerIdentity = b"", uint64 maxEarlyData = 0) as TlsResume
```

该入口用于恢复调用者已有的完整凭据，不产生随机票据或 PSK，也不完成认证。全部视图在返回前独立深拷贝；凭据输入不做前缀截断，没有裸指针构造、`nativeHandle()` 或可写字段。

数值缩窄前检查范围；XRT 继续负责 TLS 版本、密码套件、完整 PSK 长度、票据长度、生命周期和过期时刻溢出等验证。`ageAdd` 和 `maxEarlyData` 的语言载体是 `uint64`，有效值为 uint32 范围；后者仅保存原生元数据，本阶段不提供 0-RTT 读写能力。

只读属性：

| 属性 | 类型 | 单位/含义 |
| --- | --- | --- |
| version / cipher | TlsVersion / TlsCipherSuite | 精确枚举 |
| ticket / protocol / peerIdentity | bytes | 完整拥有副本，包含 NUL 和任意字节 |
| serverName | string | 拥有副本；原生 SNI 合同禁止内嵌 NUL |
| lifetimeSeconds | int | 秒 |
| ageAdd / maxEarlyData | uint64 | 原生 uint32 元数据 |
| issuedAt / expiresAt | time | xlang 固定快照的微秒时间 ABI |

`exportSecret()` 显式返回 PSK 的独立 `bytes` 副本。该副本与其他普通 `bytes` 一样管理生命周期，不承诺自动安全擦除；应用应避免日志、无保护持久化或无必要的副本。原生凭据自身在最后一个引用释放时仍由 XRT 清零。

`validAt(time)` 在半开区间 `[issuedAt, expiresAt)` 内返回 true；时钟早于签发时刻或已过期返回 false，不产生错误。`ticketAge(time)` 返回 TLS 1.3 线路混淆年龄，单位固定为毫秒、按 uint32 模加；无有效年龄时 checked 报状态错误，不把数值 0 当失败哨兵。

## 编译与拥有权

声明使用现行的驻留 direct-bridge 合同：16 个精确签名、枚举、时间、效果、错误和独立 native-handle policy。Cgen 只生成普通 ABI 调用与清理，没有票据算法或包装函数体。`@pure @nonblocking` 不等于 `@no_raise`；可分配和检查参数的接口保留 checked 错误边界。

凭据是拥有图中的独立叶能力，不持有 Session、模块回调、临时视图或语言子对象。句柄装箱失败也消费原生拥有值，不能遗留裸引用。仅使用该对象时按需选择 `XRT_MODULE_TLS_RESUME`，不引入客户端、服务端、握手或传输模块。

## 本批验证边界

两端均从本批固定源码构建实际编译器与运行时适配对象（Windows 1254、Linux 1253 个编译单元），不以已发布编译器替代。只复用真实 TCC 核心；这不是 shipping DLL/VFS 验收。

Windows/Linux 各 42 步，共 84 步，包括声明与 XLIR、严格 GCC O0/O2、JIT、模块、独立 AOT、精确合同拒绝、真实裁剪计划、全量拥有目录唯一性及回滚。构造/导出/装箱共 1536 个分配失败前缀，完整目录共 232 个，总计 1768 个；内存快照平衡。

验证没有网络流量、外部 DNS、扫描、畸形密码材料专项或安全测试。记录位于 `F:/xlang-validation/five-groups-20261025/p6-tls-resume-language/next.json`。首次空程序对照因验证脚本把同名生成 C 放在语言文件旁而触发正常伴随 C 接入，失败证据保留；输入和产物分目录后完整重跑通过，未改编译器来规避它。
