# IDE 源码范围接口

此增量为 xlang IDE 的真实语义查询提供源码位置基础，沿用 [源码覆盖与加载报告](IDE源码覆盖接口.md)。公开接口只解析入口语法树，不加载导入模块，不执行语义检查，也不提供符号、类型或定义查询。

## 调用与兼容

```c
int xlcEmitAstWithRanges(XLC_Context* ctx, const char* outputPath);
```

```text
xl parse source.xl --json --source-ranges
xl parse virtual.xl --json --source-ranges --source-overlay virtual.xl snapshot.utf8
xl parse source.xl --json --source-ranges --emit output.ast.json
```

返回值为 1 表示完整输出成功，0 表示失败；错误通过 xlcGetLastError 获取。CLI 的范围选项仅允许 parse 的 JSON 模式。原 xlcEmitAst、普通 parse JSON/tree 与 check JSON 的合同保持。

接入方检查真实 DLL 导出或 CLI help 中的 --source-ranges 能力，不根据版本号推断。旧工具链不具备新接口时继续原有功能。XLC_Context 仍为不透明类型；内部 AST 布局发生变化，直接使用内部 AST 的程序需要重新编译。

内存入口正文保持权威；文件入口遵循上下文源码覆盖表，可以解析磁盘尚不存在的虚拟文件。输出不保存覆盖正文。与旧 parse 相同，语法错误返回失败，不输出成功的部分树；语义错误不属于此解析 API 的判定范围。

## 坐标与输出

JSON 外层 schema=1、scope=entry-syntax、offsetEncoding=utf-8-after-bom、columnEncoding=unicode-scalar-1-based、rangeConvention=half-open，ast 保留旧树的字段并为每个节点添加 source、range 和 tokenRange。

| 字段 | 合同 |
| --- | --- |
| source | 解析来源路径；无来源时为 null；虚拟路径也可能是相对路径 |
| range | 节点对应的完整语法范围；未知时为 null |
| tokenRange | 构造节点时使用的原始 token 范围；未知时为 null |
| startOffset/endOffset | 去除可选 UTF-8 BOM 后的字节偏移，半开区间 |
| startLine/startColumn/endLine/endColumn | 从 1 开始的 Unicode 标量位置；CRLF 保留两个字节，计一次换行 |

编辑器使用 UTF-16 坐标时必须通过同一份正文转换，不能直接将标量列当 UTF-16 列。该输出未携带来源摘要或依赖报告；接入语言服务前还须将结果与已复核的检查批次、正文摘要和文档版本绑定，磁盘输入必须复核稳定性。

range 包含括号、泛型定界符、声明修饰符与结束关键字等语法构件。消费语句分隔符的规则可能包含换行；模块范围只覆盖解析 token 所代表的区间，不保证覆盖文件首尾被词法器忽略的空白或注释。父规则复用同一节点时会扩展 range，而 tokenRange 保留构造 token 的位置。

tokenRange 不是通用的声明名称选区。例如普通函数的构造 token 是 func，外部函数可能使用库名称字符串；函数实际名称在真实 NAME 子节点中。不得按 text 长度或诊断起点猜测定义范围。直接用名称构造的变量/参数节点，其构造 token 才是该名称。

插值表达式来自真实解析结果。临时包装表达式的偏移会映射回原字面量，裁剪已知的包装括号，并重新计算原文件标量位置。嵌套和跨行插值使用相同规则。解码后文字节点可能使用整个字面量 token 作为来源，不能将解码正文按字节索引回原源码。

## 内部传播与边界

AST 保存独立的完整范围和构造 token 范围。Lemon 栈为每个 token/非终结符保存范围，在动作重用或释放语义值之前合并真实 RHS 范围，再仅对确认产生 AST 的非终结符应用；分隔符、标志和 token 类型不会被当 AST 指针访问。epsilon 没有凭空构造的位置。

xl6AstCopySource 同时复制来源范围，即使两个 source 指针同为 NULL。引用计数继续保证来源路径在原节点销毁后有效。此复制表示原节点的来源信息；后续语义规范化产生的合成节点不能仅凭复制的范围就被视为源码声明。

新增 normalize_lemon_ranges.ps1 由原规范后处理器在各分支调用。已对 SQLite 官方 [Lemon 3.51.0](https://github.com/sqlite/sqlite/blob/version-3.51.0/tool/lemon.c) 与 [3.45.0](https://github.com/sqlite/sqlite/blob/version-3.45.0/tool/lemon.c) 生成路径执行检查。当前修改前、后语法均有 68 个已有解析冲突，Lemon 返回 1；这是仍存在的生成限制，不能记作无冲突生成。3.51.0 的解析引擎/表及 441 条动作映射核对一致；差异为行号、注释拼写和等价动作合并。最终使用规范生成的 C 源码。

范围 JSON 在发布前检查树深度不超过 256、节点不超过 100,000，以及包含最坏 JSON 转义与缩进的保守输出预算 16 MiB。超限整次失败，丢弃缓冲，不发布截断/空树，并保留原输出文件。此预算较保守，实际达到允许字节上限前也可能拒绝。既有 parser 的固定栈限制保持，新增范围 API 不承诺解析任意嵌套深度。

## 验收入口

```text
python tools/check_source_ranges.py --compiler <独立工具链>/xl.exe --baseline-compiler <固定旧工具链>/xl.exe --gcc <MinGW GCC>
python tools/check_source_overlay.py --compiler <独立工具链>/xl.exe --gcc <MinGW GCC>
python tools/check_source_report.py --compiler <独立工具链>/xl.exe --gcc <MinGW GCC>
python tools/check_resolution_report.py --compiler <独立工具链>/xl.exe --gcc <MinGW GCC>
```

范围门禁使用真实 DLL API 与 CLI，逐节点按实际 UTF-8 正文核对字节边界、标量位置和构造 token 包含关系，并比较去掉新增字段后的旧 AST 与固定旧编译器。它覆盖 BOM/CRLF/无尾换行、中文/非 BMP、嵌套与跨行插值、泛型/匿名函数、虚拟覆盖、输出文件、错误输入、输出超限保留及失败后恢复；原生宿主还检查 AST 复制和来源引用生命周期。最终结果与已知失败在 [IDE 进度](../ide/docs/progress.md) 登记。
