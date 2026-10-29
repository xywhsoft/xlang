# 交付验收限制收口

本轮对应 `delivery-size-optimization-20261030.md` 的六项限制。以下记录已经完成的独立批次；邮件客户端与原生组合资源协议、其余特定编译上下文仍在开发和验收中，不能据此宣称全部收口。

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
