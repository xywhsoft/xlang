# IDE 源码覆盖接口

本文定义 DEMO6 编译器的内存源码覆盖合同。覆盖能力用于检查或编译编辑器中的未保存 xlang 文本，语言语义仍由现有 parser、import、semantic 和 XLIR 流程决定。实现见 [公开 API](../release/include/xlc.h)、[覆盖表](../src/compile/xl6_source_overlay.c)、[编译单元](../src/compile/xl6_compile_unit.c) 和 [导入展开](../src/compile/xl6_imports.c)。

## 上下文和文本

新增三个 `XLC_Context` API：

```c
int xlcSetSourceOverlay(XLC_Context* ctx, const char* path,
                        const char* source, size_t size);
int xlcRemoveSourceOverlay(XLC_Context* ctx, const char* path);
int xlcClearSourceOverlays(XLC_Context* ctx);
```

`Set` 复制路径和明确长度的 UTF-8 文本，允许空文本，拒绝内嵌 NUL、坏 UTF-8、代理项和越界码点。最多 64 个独立路径，每份文本最多 8 MiB，合计最多 32 MiB。失败更新保留旧快照及原已编译单元；成功更新、移除或清除会使该上下文的编译缓存失效。不存在的覆盖可移除并返回成功。销毁上下文释放全部快照。

相对路径在注册时按上下文当前 `workDir` 规范化为词法绝对路径；之后改变工作目录不会移动已注册快照，但会使编译缓存失效。Windows 使用 Unicode 大小写不敏感比较，斜杠和 `.`/`..` 由绝对路径规范化处理；POSIX 路径大小写敏感。该合同不合并不同 junction、符号链接、硬链接或扩展前缀的物理别名。调用方应沿用文档的规范化路径，不依赖物理别名替换。

不同上下文的覆盖相互独立。公开编译 API 沿用现有 API/工作目录锁，不创建新的全局源码覆盖表。调用方不得在上下文销毁后继续使用该指针。DLL 按宿主进程生命周期保留；当前 XRT 的提前 `FreeLibrary` 限制另见验收记录。

## 加载优先级

`xlcAddSourceFile` 仍选择单个入口，`xlcAddSourceString` 仍替换入口文本。覆盖表不会把多个文档解释成多个入口。文件入口、普通递归源码导入、源码包的 `xlSources` 和包依赖展开均查询同一覆盖表；路径存在于覆盖表时，无需对应文件落盘。无覆盖时使用原磁盘读取。

字符串入口以 `xlcAddSourceString` 的文本为准，其递归导入继续使用覆盖表。更换入口不自动清空覆盖。覆盖不改变包清单、包查找、版本/ABI 合同、C 翻译单元、原生库、资源或环境配置；这些输入保持原加载机制。

覆盖进入真实的检查、语义/XLIR 编译和生成/构建/运行路径。文件入口的词法输出和 AST 输出也读取覆盖。磁盘文件不会被覆盖操作写入或创建；生成物与调用方指定的诊断输出仍按既有 API 写入。

```c
XLC_ContextOptions options = xlcDefaultContextOptions();
options.workDir = projectDirectory;
XLC_Context* ctx = xlcCreateContext(&options);
/* 实际宿主须逐项检查返回值；仅展示调用次序。 */
xlcAddSourceFile(ctx, "main.xl");
xlcSetSourceOverlay(ctx, "main.xl", entryText, entryBytes);
xlcSetSourceOverlay(ctx, "modules/math.xl", mathText, mathBytes);
int ok = xlcCheck(ctx, diagnosticJsonPath, 1);
xlcDestroyContext(ctx);
```

## CLI 快照

Windows CLI 使用有界、严格 UTF-8 的独立文本快照传递覆盖，避免把大正文放进命令行：

```text
xl check main.xl --json --source-overlay main.xl main.utf8 --source-overlay modules/math.xl math.utf8
```

`--source-overlay <source-path> <snapshot-path>` 可重复；同一规范化路径后注册的快照替换前者，唯一文件数与总文本仍受 API 上限约束。缺值、坏编码、NUL、超量或快照读取失败终止命令。CLI 读入快照时的源文件字节上限为 8 MiB，UTF-8 BOM 由文本读取器处理。快照参数也可用于 xlang 的 `lex/parse/cgen/build/run` 等上下文命令；C 输入命令拒绝该选项。

快照文件只承载已有内存文本，不替换原目录和导入层级。调用方负责快照的创建、访问限制、版本关联、取消后的删除和并发工作目录隔离。命令结束后上下文销毁，不留下进程间覆盖状态。

## 诊断来源

AST 来源是每次 parse 的共享、引用计数路径对象；导入节点移动和语义类型克隆保留来源。parser、导入和语义诊断复制实际文件标签，文本/JSON 输出优先使用该标签；同一行列和同一消息在不同文件中分别保留。相对磁盘标签按该次编译的工作目录解释；内存覆盖标签使用规范化绝对路径。库合成或无节点错误沿用入口/全局回退标签。

行列仍是现有一基行列，xlang 列按 Unicode 标量计数。此次只补齐文件身份，不宣称完整起止范围、字节偏移、符号/类型查询、定义位置或完整项目语义接口。后续需要向语言服务提供可验证的来源范围和依赖版本图；不能仅用路径标签推导这些能力。

## 检查时实际加载的输入报告

新增可选公开 API，原 `xlcCheck` 和不带报告选项的 JSON 格式保持兼容：

```c
int xlcCheckWithSources(XLC_Context* ctx, const char* outputPath);
```

```text
xl check main.xl --json --source-report --source-overlay main.xl main.utf8
```

`--source-report` 仅用于 `check` 的 JSON 输出（也可使用 `--format json`）。不带 JSON 或用于其他命令时退出码为 1；快照路径即使与该选项同名也按参数值处理，`--` 后的内容不作为报告选项。公开 API 返回值仍表示“分析成功且输出成功”；语义/解析失败返回 0，但已打开的输出含合法 JSON，CLI 对应退出码为 1。

顶层在原 `ok/phase/diagnostics` 后增加 `sourceReport`。`schema` 为 1，`scope` 固定为 `loaded-sources-and-manifests`。`complete` 表示该范围内的读取和结束复核完整，独立于语义结果；可完整记录一个语义失败的检查。`error/errorFile` 为空表示报告完整，失败时给出第一条读取/复核错误及路径。调用方必须检查 schema、scope、complete 和进程退出状态，不能把存在该字段当作依赖一致性通过。

每个 `files` 条目按实际首次读取顺序记录：

| 字段 | 合同 |
| --- | --- |
| `path` | 规范化词法绝对 UTF-8 路径，沿用覆盖表的路径比较规则 |
| `origin` | `disk`、`overlay` 或字符串入口的 `memory` |
| `kind` | 位标志：1 为 xlang 源码，2 为包描述文件，同路径可为 3 |
| `rawSize/rawSha256` | 实际磁盘原字节的长度/64 个十六进制字符 SHA-256；内存输入使用原传入文本 |
| `textSize/textSha256` | 实际参与解析的 UTF-8 正文长度/摘要；磁盘 UTF-8 BOM 移除，CRLF 保留 |
| `metadata.available` | XRT 文件信息可用位，磁盘要求 size/modify-time/identity，内存为 0 |
| `metadata.device/identity` | 16 位十六进制字符串，防止 JSON 数字丢失 64 位精度 |
| `metadata.modifiedUs/changedUs` | Unix Epoch 微秒的十进制字符串；changed 仅在其可用位存在时有意义 |
| `metadata.attributes` | 文件属性；内存输入各元数据为 0 |

同一次检查最多 512 个实际加载的词法路径，每份原文件/文本最多 8 MiB，保留正文合计最多 64 MiB。覆盖表自身仍受 64 份/32 MiB 上限约束。未被读取的覆盖不出现在报告中。报告由本次检查局部持有并释放，不缓存到其他检查或上下文；不返回全文，也不修改磁盘源码。

磁盘文件从同一句柄取得身份、原字节和摘要，读取前后及对应路径身份必须一致。解码严格匹配 UTF-8（允许 UTF-8 BOM），拒绝 NUL、UTF-16 BOM、坏 UTF-8、目录、缺失、超量和不稳定文件。同路径重复读取使用首次保留的不可变正文，覆盖包解析器的重复清单加载；最后重新读取并复核全部磁盘摘要、身份、大小、修改时间和可用 change-time。改写、删除、同内容文件替换或读取/复核失败使 `complete=false`、`ok=false`、`phase=source-report`，不会把多版本混合结果发布为成功。

报告覆盖入口、实际读取的递归源码、源码包的 `xlSources` 与包解析时读取的描述文件（包括实际尝试过且语法拒绝的 UTF-8 描述文件）。`sourceReport` 自身**不包含**查找过程的候选与环境，新增的独立 `resolutionReport` 记录下述 xlang 解析观察。两者都不覆盖 C/头文件/原生 ABI/库和资源内容，`complete=true` 不等于完整工程依赖图。检查结束后仍可能出现变化，语言服务必须在接收/发布时继续复核，并实现候选路径观察、文档版本关联、取消和分文件诊断。

## 可执行验收

[原生宿主](../test/cases/source_overlay_host.c) 通过真实 DLL 导出执行 API 合同，覆盖入口、递归依赖、虚拟新文件、解析/语义错误来源、移除/清空、大小写与 Unicode 路径、工作目录、并发上下文、失败更新和文件/总大小上限。[质量门](../tools/check_source_overlay.py) 严格编译该宿主，另执行真实 CLI、递归源码包、跨文件诊断去重及生成/构建/运行，并复核原文件和工具链 SHA-256。

```text
python tools/check_source_overlay.py --root <独立源码副本> --compiler <独立工具链>/xl.exe --gcc <MinGW GCC>
```

Python 仅用于开发验收，产品覆盖层、CLI 和 IDE 服务继续采用 C。Windows 质量门不代替 Linux 实测；跨平台构建源清单已同步，但 Linux 行为尚需单独验收。原 IDE 的独立工具链与固定 XGE SDK 保持原字节，新能力使用独立构建验证。

[报告快照宿主](../test/cases/source_report_host.c) 在真实 XRT DLL 下验证首次快照、BOM/原字节摘要、替换/删除/修改与严格编码/容量边界；[公开 API 宿主](../test/cases/source_report_api_host.c) 验证字符串入口、上下文隔离、撤销与并发检查。报告 [质量门](../tools/check_source_report.py) 逐项校验真实 CLI 的递归源码/包描述摘要、文件元数据、语义/解析失败报告和选项错误退出码。产品实现继续为 C。

```text
python tools/check_source_report.py --root <独立源码副本> --compiler <独立工具链>/xl.exe --gcc <MinGW GCC>
```


## xlang 导入候选和包搜索环境观察

带来源报告的检查还返回独立 `resolutionReport`，`schema:1`、`scope:xlang-import-resolution`。`probes` 项为绝对 path 与布尔 exists，记录普通源码查找和包描述查找**实际调用的普通文件存在谓词**，包括缺失候选；目录不等于文件。覆盖命中时不调用磁盘存在谓词，因此对应路径不列入 probes。路径沿用覆盖表的 Unicode 大小写比较，重复查询使用首次答案。

`packagePathObserved` 表示包解析是否实际读取 `XLANG_PACKAGE_PATH`；该键为首次拥有副本，缺失为 null，存在但为空为 `""`。未查询时 observed=false、值为 null，不把环境默认值伪装为读取结果。后续包查找使用首次副本，结束时复核当前环境。候选变化或观察过的环境改变使同一 sourceReport 的 complete=false、ok=false、phase=source-report。原普通 check JSON 仍只有 ok/phase/diagnostics。

每次最多 2,048 个唯一候选、路径字节（含 NUL）合计 1 MiB，环境值至多 32,767 UTF-8 字节。超量或无法保留观察终止完整报告。候选记录保留优先级搜索过程中的正/负答案，不包含未查询路径、伴随 C 文件/头文件/库/资源内容或整个目录的稳定性。IDE 使用此报告复核新建的优先候选，仍须在接收时再次验证输入和版本，并另外处理发布后的变化。

[原生观察宿主](../test/cases/resolution_report_host.c) 与 [质量门](../tools/check_resolution_report.py) 验证首次候选、创建/删除/目录到文件、Unicode 别名、2,048 上限以及缺失/空/非空环境的首次副本与失效；实际 CLI 验证覆盖短路、无导入未查询、环境与旧 JSON 合同。

```text
python tools/check_resolution_report.py --compiler <独立工具链>/xl.exe --gcc <MinGW GCC>
```
