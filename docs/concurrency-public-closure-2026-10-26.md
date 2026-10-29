# 并发常规产品回归与逐模块范围账本

## 本批结论

不改运行时、Cgen、XRT 或时间 ABI。对已重建的实际 Windows／Linux 产品，
新增统一的并发正例／编译拒绝清单，补做普通公共入口回归。全部通过不代表
完整并发语义、竞态、其他四组或全仓 RC0 已经验收。

`tools/concurrency_public_fixtures.json` 含 24 正例、8 编译拒绝例。覆盖原子、
Once／Event／ThreadLocal、同步对象、Promise／Future／聚合、Channel 的
有限等待与拥有式 Future、TaskGroup 原子启动及池／网络引擎组合、TaskPool
和 Executor。网络引擎组合只执行本机引擎任务，不创建 socket 或访问网络。

两端各 153 个产品阶段通过，共 306 个：

- 使用真实公共宿主及实际编译器库，逐例检查加载、编译、typed／lowered、
  C／头输出、重复输出一致性、JIT、module、AOT 和 checked 退役。
- 正例均经严格 GCC O0／O2 语法检查、独立目录 AOT 执行、实际动态依赖
  检查及隔离 CLI 执行；独立 AOT 不依赖 xl DLL／SO。
- 8 个负例在编译期 checked 拒绝且不输出 C／头文件；不是执行错误回调。
- 继续使用 `p7-crypto-migration/shipping-source-r3` 的普通全构建产品，没有
  编译对象替代、测试运行时或源码部署回退；未覆盖仓库 release 二进制。

## 198 个模块的账本

`tools/five_groups_module_roles.json` 保留用户要求的完整清单：并发 33、密码学
与 ASN.1 57、X.509 21、TLS 47、网络 40。不能用 C API 的直接引用数判定模块
是否缺失：例如 TaskGroup 与池／网络引擎通过已有原子 start 和拥有式 Future
组合；SpinLock 和七个有栈协程模块按批准方案保持内部，不另造语言执行模型。

本批完成全部 33 个并发模块及 `task_group_net` 的角色复核，记录公开面、
复用／组合原因、源码和设计依据以及相关普通用例。其余 164 项的本次角色
复核明确标 pending；这不是“164 个模块未实现”，也不是新的开发完成百分比。
原始 AES 块操作和独立 Poly1305 是否公开到 cryptoAdvanced 仍待 API 范围确认，
不能因二者已被 AEAD 使用便擅自认定公开接入完成。

`check_five_groups_module_roles.py --self-test` 只检查范围和证据引用的一致性，
不是运行验收：独立固定 198 个名称，拒绝遗漏、重复、改组、未知配置、虚构
完成状态、未知用例及越界／缺失文件；两端各 11 项错误库存拒绝通过。脚本
输出明确保留 `RuntimeAcceptanceEvaluated=false` 与整体未完成状态。

## 证据与下一步

证据根 `F:/xlang-validation/five-groups-20261026/p7-module-audit/`。独立复核
`verified-concurrency-windows.json`、`verified-concurrency-linux.json` 核对
原构建输入、实际产品、全部日志／输出、阶段集合，以及当前／冻结／已部署
三份用例内容；Linux 853 个证据文件归档后逐项校验，原件保留。

继续逐项复核其余四组、迁移剩余旧调用并执行最终联合门。没有新增公网、
扫描、攻击模拟、畸形密码学样本或安全专项。未决策 CRT 0／XRT 11 不变：
核心／错误／拥有权、内存、动态值／图、其余系统信息、HTTP、WebSocket、
运行时类型／转换、对象／字段／图、调用／值桥、typed 容器、内部支撑。
内存池继续最后处理。
