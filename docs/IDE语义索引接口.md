# IDE 语义索引接口

本接口为同一已检查编译单元导出语义事实。原生 IDE 已接入受同批次输入守护保护的 stdio 定义、大纲、悬停及补全；首版仍为传统 IDE。编译器接口自身的通过不能代替 GUI 验收，当前结果见 [IDE 实施进度](../../ide/docs/progress.md)。

```text
xl check main.xl --json --semantic-index
xl check main.xl --json --semantic-index --source-overlay dependency.xl dependency.snapshot.utf8
```

公开 C API 为 `xlcCheckWithIndex(ctx, outputPath)`。它执行一次现有源码加载、import/stdlib 展开、语义检查及 IR 验证；按需记录查询元数据，构造索引后再次复核实际加载来源、解析候选与包环境。结果同时包含现有 `sourceReport`、`resolutionReport` 和新增 `semanticIndex`。`xlcCheck`、`xlcCheckWithSources`、普通 `check --json` 和 AST 输出格式保持原合同。新 CLI 标志只允许 `check --json`，扫描尊重选项参数值及 `--` 边界。

## 数据合同

成功索引的 `schema` 为 1，`scope` 为 `checked-compilation`，`complete` 为 true，`nodes` 为已展开语义 AST 的前序平面表。节点 `id` 从 1 开始，`parent` 指向同表父节点，根为 0。所有 ID 都仅在这一检查批次内有效；不能跨正文版本或编译请求复用。`complete` 表示整个可导出事实表成功生成，不表示编译器为所有节点提供了全部类型/目标事实。

`sourceId` 与 `selectionSourceId` 为同一 `sourceReport.files` 的从 1 开始索引。0 表示未关联到实际加载源码，范围为 null。来源 kind 为位标志，包含源码位（1）的混合角色文件也可定位；只有描述信息位（2）的文件不作为源码位置。路径使用 XRT 的绝对路径解析及平台路径比较规则；内部生成的 stdlib/隐式实体没有用户来源时保持不可定位。

`range` 是真实完整语法范围，`tokenRange` 是节点构造 token。显式声明的 `selectionRange` 单独选择真实名称 token；函数从其 NAME 子节点取得，其他已确认的命名声明使用自身名称 token。构造关键字、库字符串、匿名函数、隐式实体不会被自动当成声明名称。导入展开即使重命名语义文本，也保留物理文件中的原名称位置。

范围采用 `offsetEncoding=utf-8-after-bom`、`columnEncoding=unicode-scalar-1-based`、`rangeConvention=half-open`。偏移按各来源正文去掉可选 BOM 后计算，CRLF 保持原字节。输出前核对范围有界和 UTF-8 边界；不关联到来源或不可用的范围为 null。GUI 从已验证同批次正文转换成自己的 UTF-16 坐标。

`declaredSymbolId` 只由真实实体身份分配与实际作用域声明写入。`symbolId` 沿用编译器 AST 的语义身份，也可能出现在 NAME 引用上；`targetSymbolId` 和 `compoundOperatorSymbolId` 为实际检查写入的目标。不能仅凭 `symbolId` 或名称搜索推断声明。函数实体与函数名称作用域符号可能具有不同 ID，均通过实际源位置关联；CALL 的目标保留真正选中的重载实体。隐式 self、动态访问或当前编译器未保存目标的节点允许没有可导出的声明，查询端必须返回不可用。

`typeId`/`typeAbiId` 是节点本身的语义类型身份。可用的 `type` 对象另含 `origin`（declaration/inferred/syntax）、`id`、十六进制 `abiId` 与 `canonical`。声明类型从实际作用域符号使用的类型取得，包括 auto 推导及临时循环/捕获绑定类型；副本归 AST 节点独占，语义上下文销毁后仍有效。其他表达式只导出真正保存的 inferredType，TYPE 节点使用自身类型。canonical 复用真实类型格式化器，保留别名透明化、int64/int 和 float64/float 等规范化；内部规范拼写并非最终用户悬停展示文案。没有事实时为 null，零类型 ID 保持原编译器含义。

2026-10-08 的 schema-1 增量为每个节点添加 `declaredTypeId` 与 `referencedTypeId`，无事实时为 0；两者最多一个非零，并等于节点 `typeId`。字段可由旧工具链整表省略，查询端必须核对全表存在性一致。CLASS、UNION、ENUM、TYPE_ALIAS、EXTERN_TYPE_ALIAS、PROTOCOL 及 TYPE_PARAMS 的直接 NAME 子节点可声明真实 TypeId；TYPE 节点、类的已检查基类 NAME、已检查的 CLASS/PROTOCOL 泛型约束可引用真实 TypeId。类型声明沿用实际名称 token 选择范围，与 `declaredSymbolId` 无必然绑定。

protocol 使用类型注册表的实际声明身份；泛型参数由真实作用域检查分配独立 TypeId，`typeAbiId` 为零，不能把它解释为运行时 ABI。基类及类型约束仅在索引捕获打开且检查成功时补记实际类型身份；内建能力约束没有对应类型注册实体时保持 0。新增索引字段不分配 SymbolId，不改变普通检查/AST、公开 API 或生成头文件。单态化复制可能保留同一类型声明身份，查询端仅能合并类型角色、ABI 与真实完整/选择源码范围完全一致的目标，不能按名字消除歧义。

## 限额与失败

索引最多 65,536 AST 节点、树深 256、保守预估及实际 JSON 8 MiB；每个节点预留固定字段/范围预算，字符串按最坏 JSON 转义预算计数。声明临时类型复制总计最多 65,536 节点、4 MiB 文本、深 128；单个 canonical 类型最多 8 KiB、格式化递归深 128、8,192 次访问。超量或内存失败拒绝整份索引，诊断指出失败；不会发布部分节点。该限额仅用于 opt-in 查询，不增加普通检查的类型复制开销。

语法、导入、语义、IR、来源复核或索引构造失败时，检查返回失败，`semanticIndex` 为 schema-1、complete=false、nodes=[]；诊断和来源报告按现有检查合同输出。输出路径沿用检查 API 的行为，错误结果可写入该路径。内存入口正文优先于同路径覆盖，文件入口与依赖按覆盖合同处理；不写回源码。

## 已知来源报告边界

显式文件名目标（例如 `import "helper.xl"`）可能先被包解析器作为描述文件读取，再回退到源码加载；未保存覆盖与这一磁盘探测记录冲突时，现有 `check --json --source-report` 和新索引接口均安全拒绝。同批次索引保持 complete=false/nodes=[]，不把磁盘正文误当成编辑器覆盖。该问题在固定旧工具链也可复现，本增量没有改变包清单或解析优先级；普通 `import helper` 的覆盖可用并已验收。后续来源观察需要区分失败包探测与实际源码/描述输入，同时保持发布前复核与发布后守护，这仍属于长期任务。

真实 DLL 宿主销毁所有上下文，但沿用已有 XRT 合同将 DLL 保持加载直到进程退出，防止卸载后的 FLS 回调引用失效；此接口没有修复 DLL 卸载生命周期。

验收工具为 [check_semantic_index.py](../tools/check_semantic_index.py)，涵盖真实作用域遮蔽、重载、导入源名称、覆盖、别名/类身份、auto、BOM/CRLF、Unicode、原生 API 和限额。结果以 IDE 工作区的当次实际证据为准，见 [实施进度](../../ide/docs/progress.md)。
