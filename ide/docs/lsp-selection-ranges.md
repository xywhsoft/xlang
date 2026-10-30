# 原生结构化选择范围

2026-10-30，M7a2b89a，开发服务 0.100。外部服务新增标准 `textDocument/selectionRange`，初始化声明 `selectionRangeProvider: true`。使用共享 xlang 原解析器及已有常驻语法会话，无需工作空间、CLI 或完整语义／IR 检查。结果形状遵循 Microsoft 的 [LSP 选择范围规范](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/selectionRange.md)。

## 分析与所有权

`analysis/selection.h` 提供独立的 opaque 结果、范围深度和只读查询。位置使用原始 UTF-8 字节偏移，包含源 BOM；范围另保存后台计算的 UTF-16 坐标。不向调用者暴露 AST、XUI 或 JSON 类型，也不借用原语法树和正文的存储；创建方负责销毁结果。

一批至多 256 个位置按偏移排序，共用一次原 AST 遍历。节点范围沿用已有的增量单元位置映射，token 范围使用同一个字节位移。仅收集包含查询位置的已知原范围，再按请求下标和范围跨度排序，去重并排除交叉范围，形成严格向外包含的 parent 链，最终包含整份文档。请求顺序和重复位置保留。没有引入词法或语法规则副本。

解析错误时返回空光标范围及文档父范围，避免把错误正文当作完整语义结果。候选容量限制为 8 MiB、每位置最多 128 层，超出限制或分配失败拒绝本次分析；这个局部限制不包含原语法快照、遍历栈、allocator 和协议编码，也不是进程总内存预算。

## 调度与协议

`xlsWorkerSelection` 在提交前复制位置并保留不可变文档输入，复用原后台 actor、语法缓存、Future 和唤醒机制。语法遍历、筛选、排序和 UTF-16 转换均在后台执行；协议 owner 仅编码已经计算的结果。公开核心接口没有 Windows／stdio／UI 依赖，不代表已完成其他平台构建或 DLL 集成。

请求仅要求已打开的 xlang 文档和标准 positions 数组；可选 version 必须匹配。空数组可用；非法数组、负坐标、非整数、UTF-16 代理内部位置、关闭文档和过期版本返回参数错误。取消、修改、关闭及 shutdown 复用原生请求的身份／版本保护，旧任务不能发布迟到成功。

本阶段提供外部协议和分析核心；IDE 内的扩展／收缩选区交互尚未接入，不将协议通过当作 GUI 验收或整条按键链路性能结论。C 的语义服务保持已有独立后端。

## 验收

主测试有 5,652 项检查，覆盖原字面量边界、严格父链、无序及重复位置、Unicode／BOM／CRLF、独立结果寿命、原会话缓存、输入位置复制、取消及逐项拒绝观察到的可失败分配／容器调用。另一个常规测试证明真实的一次函数片段解析／一次声明复用后，未修改函数的当前选择范围与独立新解析一致，并验证深度／候选限制及失败后的恢复。

四种实际源码破坏被拒绝：错误字面量边界、丢失 BOM 偏移、颠倒请求位置及忽略复用单元的新位置。103 层 parent 链通过真实 JSON 协议往返。

完整构建重新编译 208 个对象、执行 112 组原生测试；之后新增的投影／边界常规 fixture 使用该完整核心单独编译并通过，合计 113 组分别验证。生产核心、服务和其他对象未变，构建脚本仅追加测试清单成员；原脚本与新测试的命令、输入和产物另行保存。不能将末次清单修改称作又一次完整编译。最初归档清单漏项造成的链接失败保留，修复后的完整构建通过。

53 套协议回归全部通过，使用真实完整服务和当时根发布目录的 CLI；12 份现有协议工具的当前版本断言同步为 0.100，仍精确检查版本，未删除原断言。新增选择范围套件为十组场景，包括最大批量、错误恢复、真实大解析期间取消及编辑／关闭后的单次错误响应。第一次协议尝试因旧版本断言拒绝新服务，日志保留。

## 性能与交付边界

同一阶段恢复了固定 M88 程序的正式测试：A/A 全量完成但 2／64 项相对门槛失败；A/B 在 1,845／1,920 次请求后因提交余量不足中止。原门槛及所有慢样本保留，详见[调用查找的后续测量](lsp-callsite-lookup-performance.md)。选择范围本身尚无正式 UI／端到端性能成绩。

本轮程序隔离在 `ide/build/lsp-m89-20261030-e4f1cf1a/`，没有写入根 `release/`。前端继续使用 M87 冻结的 `a845a3471`／`003dd48ea` 组合，不包含并行的引擎和 XRT 更新（已提交为 `ed78373dd`，仍待本任务审核）。开发性能基线仍为 0.99，候选未准入；真正函数／交叉表的部分 IR 生产、当前匹配 SDK／IDE、其余适用 LSP、规模／内存／长稳与交互时延验收继续，长期目标 active。

```powershell
python -X utf8 -B .\ide\tools\build_lsp_core.py --frontend-root .\ide\build\lsp-m87-20261030-111f4b15\frontend --output .\ide\build\selection-core-new --server
python -X utf8 -B .\ide\tools\check_lsp_selection.py --server .\ide\build\selection-core-new\xlang-ls.exe --output .\ide\build\selection-protocol-new
```

阶段证据包括 `current-full-v2`、`additional-native-v1`、`protocol-current-v2`、`negatives-v1`、`negative-placement-v1`、`selection-extra-result.json`、`deep-protocol-v1`、两次配对运行和最终审核记录。此前失败的编译、链接和协议记录均保留。
