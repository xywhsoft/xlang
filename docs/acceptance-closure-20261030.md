# 交付验收限制收口

本轮已关闭 `delivery-size-optimization-20261030.md` 中的六项验收限制，并完成用户确认的通用扩展资源拥有图与邮件客户端迁移。专项按固定输入分批执行，最终使用重新构建的 Windows/Linux 共享产品验收交付；机器可读汇总见 `reports/acceptance-closure-20261030.json`。

## 已验证的修复

- BSON、CBOR、MessagePack 的语言入口使用现有 `xl6ForeignDataSnapshotV1`，将语言容器投影为原生 wire 值。编码与 BSON 文件保存均覆盖；未引入 JSON 中转。该投影有容器分配与图遍历成本，不宣称零开销。
- 上述三个包和 Plist 的现有语言夹具均通过实际发布编译器的 JIT、独立 AOT 构建与运行，共 12 个阶段。Plist 本轮无需源码修补；完整包依赖中的 `xml.dom` 必须可用。这里的结论限于这些夹具，不能替代各扩展库全部测试。
- 引擎配置的旧微秒溢出断言改为直接毫秒边界：`INT64_MAX` 毫秒创建和读回成功，`INT64_MIN` 拒绝。原有引擎/配置 O0、O2、OOM 与错误拥有关系负控制完整通过，输入哈希保持不变。
- `check_xruntime_language_bridge.ps1` 更新为现行分队列模块与驻留运行时合同。旧版实际是 **24 条**失败，原报告写 23 条有误。前端依赖检查继续禁止语义分析调用，允许名字相近的 semaphore 驻留函数。默认检查增加 8 个真实语言夹具的 JIT/AOT 构建与运行，并检查 SPSC 实际生成的原生模块闭包没有带入 MPMC 或全部模块；`-StaticOnly` 明确只验证静态合同。
- XRT `net_file_future` 新增仅选择该模块、内存诊断和平台文件后端的单头测试入口，Windows GCC 实际构建和运行通过。上游提交 `9b5d52694c9c53e7c57de9c9d2760a171a97858f`。严格成熟度检查现为 377/377 单头覆盖，随后不使用开发豁免完成下游快照同步；生成库实现内容未变。

## 证据

原始报告与日志位于 `F:\xlang-validation\acceptance-closure-20261030`：

- `codec-plist/results.json`：12 个 JIT/AOT 阶段。
- `bridge-final/results.json`、`spsc-selected-native.c`：语言运行与实际模块选择。
- `engine-final/results.json`：完整配置/引擎专项，包含故意破坏实现的负控制。
- `discovery-execution/results.json`、`discovery-executor/results.json`：执行资源与 Executor 既有专项。
- `xrt-single-discovery-r3.log`、`strict-xrt-sync.log`：上游单头实测与严格同步。

本轮不执行网络安全、攻击、外部端点或协议模糊测试。

## 特定原生上下文与扩展包验收

- 使用实际模块规划器生成选项及驻留源码闭包，完成 12 组既有专项（socket、file execution/language、TCP execution/language/Future、TaskPool、server、buffer graph、server ownership、UDP execution/language），各报告 Complete/InputsUnchanged 均为 true。保留 O0/O2、OOM、拥有关系和故意破坏实现的负控制。证据 `configured-final/batch.json`。
- 选取式单头实现必须先于声明头 `xl_clock.h`，否则过早的 include guard 会屏蔽该编译单元需要的实现。相关支持入口调整包含顺序；`text_value.h` 补齐只用于指针签名的三个不透明类型声明，没有扩大算法闭包。
- 毫秒迁移清除 TaskPool 与 TCP server 夹具中的旧单位假设。`INT64_MAX` 是合法毫秒等待，在已经静止的池上验证；阻塞负例改用非法负值。取消请求未获胜也不等于 Future 已经终结，竞态夹具在 join/有界等待后继续检查真正终态与内存平衡。
- 独立 Pool/Bytes 目录的 O0/O2、真实 owning references 与五个目录 OOM 前缀检查通过；证据 `buffer-native-final/results.json`。该闭包不引入 Engine。
- Plist 完整 Windows gate 通过：两份原生 C 测试、JIT/AOT 边界与分配失败、私有类型负例、C/语言示例比对及 Python plistlib 四格式互通。证据 `plist-full.log`。
- CBOR/MessagePack 时间边界夹具统一毫秒与明确 Unix epoch。固定 timestamp32/64/96 和 int64 全域向量由独立整数计算得到，不保留旧微秒行为。

## 最终收口

| 原限制 | 修复与最终验证 |
| --- | --- |
| BSON、CBOR、MessagePack 语言容器 | 复用通用数据快照，实际容器编码成功；四包 Windows/Linux 各 12 个 JIT/AOT smoke 阶段；CBOR/MessagePack 完整 Windows gate 与独立 Python 双向互通通过 |
| XMail 旧 Lease/裸引擎接入 | 完整迁移至通用组合资源合同和真实 Engine/TLS 拥有句柄；Windows/Linux 各 23 阶段通过，输入哈希稳定；101 个原生声明逐一编译链接 |
| 24 个原生夹具缺上下文 | 使用现行模块规划器生成配置、驻留源码和平台后端；原生目录/OOM/生命周期专项及最终两端共享产品终端检查全部覆盖 24 个入口；逐项映射见证据 `configured-fixture-coverage.json` |
| 旧 bridge 断言 | 驻留委托、前端边界检查与 8 个真实 JIT/AOT 用例通过；实际 SPSC 原生闭包没有混入 MPMC/ALL；最终发布编译器复验通过 |
| XRT net_file_future 单头覆盖 | 上游提交 `9b5d5269`，模块化/选取式单头的真实文件 Future 测试通过，严格快照检查通过，377/377 单头覆盖 |
| Plist 独立验收 | 完整 Windows/Linux gate 通过；Linux 包含原生 ASan/UBSan/泄漏检查；两端保留私有类型负例、分配失败、双语 AOT 与 Python plistlib 四格式互通 |

### 通用协议与邮件迁移

- 通用合同、类型封闭的字段投影及 C 源码依赖声明已提交 `fc2cf2414`；设计见 `foreign-native-graph.md`。原生组合不再按没有隐藏引用的叶对象处理。实际引用环、BUSY/重入、Prepare、16 个 OOM 前缀、两个独立 DLL/SO 的代码驻留，以及原生工作线程回调返回后的代码释放均已验证。
- 邮件迁移提交 `6b1929f81`。SMTP/POP3/IMAP 保留原有命令、DATA/BDAT、字面量/事件和分块路径；接入 `TlsClientContext` 的完整配置与语言闭包。客户端追踪真实 Engine 与线路拥有槽；构造期工作通过拥有任务派发，排队超时、取消或丢弃时都不再使用调用方已经结束的借用。
- 修复文本附件通过 bytes 读取器探测类型留下错误的问题；改为先判别类型再读取。公开 C 边界与 `c_longlong`/`c_ulonglong`、point、var 精确对齐，保留内部只读签名，解决 LP64 和 const 指针声明冲突。
- `tools/check_xmail_migration.py` 首先编译 101 个入口的声明探针，再执行原生 O0/O2 交接与构造失败、MIME、三个普通 loopback 协议及闭包配置投影的 JIT/AOT。原生交接测试的 record 创建失败使用测试替身；真实 record 分配失败由通用 Graph 专项覆盖。声明探针只编译链接，不调用。

### 发布产物与证据边界

`release/xl.dll`、`xlc.dll`、`libxl.so`、`libxlc.so` 和 Linux CLI 已更新。Windows 构建前记录逐文件输入，构建后验证未变；隔离 Linux 产物在预编译前后、VFS 生成和共享构建完成后核对源码指纹，两端的核心指纹一致。Linux 逐文件哈希在构建后记录，方法在汇总 JSON 中明确保留。

- Windows/Linux 的实际双共享产品终端测试均为 O0/O2，分别完成 64 次会话，每次保留 13 项错误 guard 及堆/拥有句柄检查；缺失/错误 resize 的四个反例在每个平台均正确失败。Windows 平台自身保留句柄另列，不宣称系统总句柄零增长。
- 两端 source-free 交付检查通过：原生 C、嵌套编译器、EXE/动态库及毫秒时间往返，不依赖生产者源码目录。
- 最终邮件证据为 `mail-shipping-r3/results.json` 与 `linux-evidence/mail-shipping-r3/results.json`，均 complete、inputsUnchanged，23/23 阶段返回 0。历史失败和中间发现保留，未覆盖重写成成功。
- 其他关键证据为 `terminal-shipping-final/results.json`、`linux-evidence/terminal/results.json`、`delivery-final/results.json`、`linux-evidence/delivery/results.json`、`bridge-shipping-final/results.json`、`wire-interop/results.json`、`plist-full.log`、`linux-plist-full.log`。

验收范围是离线数据、拥有权/分配失败及自建普通回环会话；本轮没有执行攻击、安全专项、外部端点或实际 TLS 握手测试。扩展记录、参数载体/releaser、原子引用/Mutation 作用域、Engine/TLS pin、STARTTLS 请求/任务分配、主机名扫描和 MIME 容器投影都存在实际成本，不宣称零桥接开销。协议算法继续复用 XRT。
