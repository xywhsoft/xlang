# Proxy Dial 原生接入前置

状态：原生拥有图／Future 已进入固定快照；受管理 xlang 代理拨号公开面尚未完成。
不能把本批验收当作完整 P3、语言 JIT/AOT 或发布验收。

## 来源与边界

从 XRT `1e1b6fad` 到 `604b7bcc` 导入三个已独立验收的批次：
Stream 拥有型事件、Proxy Dial 真实拥有图、TCP／Proxy 共用 Future 生产者。
只更新明确列出的公共头、内部头、源码和选择器，不读取原生工作区中未提交的
内存、文件或 allocation-budget 改动。

新增 16 个原生公共入口；XRT 为 3305 个，xruntime 为 3770 个，其中扩展自有
465 个。公共声明、TCC 符号表、按模块原生链接目录和能力矩阵同步。
这些数字表示原生面，不表示新增了 16 个默认语言函数。

## 固定时间合同

| 项目 | xlang 固定快照 |
| --- | --- |
| 配置相对时长 | `uint64` 微秒 |
| Proxy 全过程默认超时 | 30,000,000 微秒 |
| 全过程超时为零 | 不加总期限，仍保留内层传输超时 |
| 绝对期限 | `xdeadline`，使用 `xrtDeadlineAfter/Expired` |
| 调度 | 固定快照的 `xrtNetEngineSchedule` |
| 有符号毫秒哨兵 | 不引入 `XRT_WAIT_FOREVER` |

不能仅替换 Timeout 字段类型：默认值、截止时间创建、期限比较和 Timer 调度
也必须一致。Stream／TCP Dial／Listener 的既有微秒合同及配置校验保持不变。

## 拥有权

Future 的 Bridge 借用 Promise，实际生产者只拥有一份 Promise、取消 Watch
和原生操作。TCP／Proxy 共享生产者实现，但按精确操作家族认证，不相互冒充。
Proxy Dial 追踪真实 Engine、Proxy、内层 Dial、Stream、回调数据、握手内的
第二份 Proxy 引用与缓冲／错误边；唯一事件绑定不伪造独立 RC 节点。

成功 Future 持有 Stream。`xrtFutureValue` 只借用，`xrtNetStreamRef` 才取得
独立引用；关闭与 Destroy 分开。后续 xlang 受管理接入必须主动关闭未交付的
成功结果，即使另有原生观察者，也不能把取消请求当作真实完成。

## 本批验证

`tools/check_stdlib_tcp_proxy_native.py` 在 xrt／xruntime 两个实际产品中，
使用 SOCKS5／HTTP CONNECT 固定有效回复和自建 literal loopback，覆盖真实
取消／超时、阻塞自定义 lookup、闭合拥有图、精确家族拒绝、构造分配失败、
独立 Stream 引用及应用数据回显。旧测试 driver/main 不被复制或执行。

同时回归 `tools/check_stdlib_tcp_dial_runtime.py` 的原生及受管理 TCP 图、
真实退役和目录政策负对照。Windows 使用 select／IOCP，Linux 使用
select／epoll；GCC／TCC、O0／O2，Linux 另有 ASan／UBSan。
原生链接目录按实际预处理定义推导，不凭函数名前缀猜测模块。

完整证据位于 `F:/xlang-validation/five-groups-20261021/p3-proxy-language`。
不调用系统 DNS、不访问公网，不进行畸形协议、攻击模拟或网络安全专项测试。
不替换发布二进制、VFS、IDE 或无关扩展库。

## 下一步

将不可变代理／拨号配置接入现有 TcpStream 结果载体，复用其真实父拥有者、
两步异步类字段捕获、结果提升、取消／期限及 checked 退役协议。
Resolver 只解析代理端点；目标主机必须交给代理，不在本地隐式解析。
Cgen 继续只生成声明、调用和清理；协议、状态机和拥有关系留在驻留实现。
