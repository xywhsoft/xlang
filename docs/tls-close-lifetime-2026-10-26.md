# TLS 显式关闭与最后引用：2026-10-26

本批将已批准的 TCP 关闭规则落实到受管理 TLS Stream：成功接受显式 Close
后，语言最后引用消失不撤销已接纳发送与协议关闭；自动释放未关闭的连接
仍中止，显式 Abort 始终优先。没有增加密码算法、原生 API 或 Cgen 包装。

## 真实缺陷与实现

旧 TLS resource 的 `Retire` 无条件调用 `xrtTlsStreamAbort`。在真实 Worker
暂停、排队发送、Close、释放最后 owner 后，原生发送 Future 被取消；首轮
失败日志保留。底层 XRT 本来就能等待已接纳 AsyncSends、发送 close_notify
并处理关闭期限，缺陷在语言退役层，不应再造一套协议或发送队列。

私有关闭原因分为自动、正常关闭、中止。成功 Close 仅从自动状态 CAS 到
正常关闭；Abort 单向覆盖，后续 Close 不能降级它。调用 scope 保护状态
发布与退役交界。正常关闭资源保持真实 source 登记、原生 pin 和 Engine
拥有边，直到 TLS 与实际 TCP 均终止；BUSY 和资源清理错误保持原有 checked
边界。清理就绪不等于发送／协议成功，操作结果仍须通过原有状态或 Future 观察。
准备中／未交付的 Accept／Dial 仍按原取消和结果拥有合同处理。

不保证对端业务消费，不自造有限完成时间，也不跳过 XRT 的关闭超时／错误。
Engine／模块整体停止与独立取消仍是更高层清理边界。公开物理 ABI 不变，
独立 XRT、固定快照和两边时间 ABI 不变，原生算法和 Cgen 均未修改。

## 批量验证

原生测试使用固定有效证书、实际两个 Worker、实际 TLS／TCP 和有界事件门。
排队 65,536 字节（全部字节值含 NUL），Close 两次，释放真实最后 owner，
等待真实退役 worker 已尝试，再恢复 I/O。检查完整交付、TLS/TCP 物理关闭、
真实堆账本；检查用 Hold 不增加语言 owner。另测 Close→Abort→Close。

GCC O0／O2、TCC 和 select／IOCP／epoll 均通过；Linux 独立闭包另做
ASan／UBSan。恢复旧 Abort 行为、用 Close 覆盖 Abort 的两个真实错误实现
必须失败。Windows 20／Linux 23，共 43 个原生阶段，不冒充语言或发布证明。

新增公开生命周期用例与两个编译负例，复用已有固定有效证书材料；负例
拒绝 raw point 构造和 private handle。原网络产品验证器增加显式 TLS
清单及 `.xl` 支撑文件，保留相对导入布局，默认 18 项网络清单不变。
只复制语言输入与规范声明，隔离部署仍只有三个实际产品，没有替换对象。

冻结 9,961 个输入，正常重建 Windows／Linux 的 CLI、运行库、编译器库、
TCC、XRT 对象和两份 VFS 后验证：

| 实际产品范围 | 每端阶段数 |
| --- | ---: |
| 10 个 TLS 正例、2 个编译负例 | 63 |
| 原 18 项网络清单 | 99 |
| 五组联合，两个实际后端 | 7 |
| IDE CLI 查询合同 | 13 |
| 组件声明、严格 C、独立 AOT | 21 |

每端 203、两端 406 阶段。正例同次编译输出 typed／lowered IR、确定性 C、
头文件，执行 JIT、module、checked 退役，再做 GCC O0／O2、移位后的 AOT
和普通 CLI；AOT 不依赖 xl DLL／SO。独立 reader 核对全部输入、日志、
产品哈希和计数，另检查所有 TLS 生成 C 没有旧 wrapper 或驻留实现函数体。

当前完整 Windows 源码 cohort 另通过原 30 正例／4 负例的 125 阶段，未
重写原例、不使用宿主地址代替 AOT 链接；它不算作 Linux 同组或发布阶段。
PowerShell 5 构建前置与正常守卫已[独立修复验证](source-driver-staging-2026-10-26.md)。

所有测试限于离线固定材料、数值地址、自有字面回环和常规 ABI／生命周期；
没有公网 DNS、探测、畸形密码材料或网络安全性质专项。只编译发现的两个
WebSocket 旧用例不纳入已批准 TLS 清单，留在未决策 WebSocket 边界。

证据根 `F:/xlang-validation/five-groups-20261026/p7-tls-close/`：失败记录、
构建收据、43 原生阶段、406 产品阶段、源码批次和独立哈希回执均保留。
Linux 原件另镜像为逐文件哈希验证的归档，共 12,537 个文件。

## 仍开放

本批关闭 TLS 的 last-owner 缺陷，不等于五组完整验收。参数化总门的
worker／共享合同构建整合、完整跨组逐项矩阵、嵌套编译／IDE 与 RC0 条件
继续；不会把所有标准库算法安全审计纳入本轮或宣称已完成。
未决策 CRT 0／XRT 11；内存池最后。
