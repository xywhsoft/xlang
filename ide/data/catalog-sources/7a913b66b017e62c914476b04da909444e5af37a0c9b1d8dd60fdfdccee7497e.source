# terraform_state

2026-10-24 当前 Windows 发布版：原有 C、XLang 解析查询、双语范例和
完整 AOT 门禁通过，但新增合法语言字典传入 `version(var)` 的用例
报 `TypeError: Terraform state must be a JSON object`；同结构的
`parse()` 结果可正常查询。包级门禁现固定这条失败路径，不能仅凭
解析产物的成功宣称公开 `var state` 参数完整可用。复现和主线修复
方向见 `test/literal_state.xl` 及
`dev/extlib_blockers/foreign_var_container_projection.md`；包内不
读取编译器私有 handle。默认门禁须待共享 ABI 修复后重新验收。

只读查询 Terraform **原始 `.tfstate` v4 JSON**。这不是 `terraform show -json`
的 `format_version` / `values.root_module` 表示；两种格式不能混用。
原始格式的 `resources`、`instances`、`outputs` 形状可参照
[Terraform state 教程](https://developer.hashicorp.com/terraform/tutorials/state/state-cli)；
另一种长期兼容的 JSON 输出格式见
[官方 JSON 输出规范](https://developer.hashicorp.com/terraform/internals/json-format)。

```xlang
import terraform_state

var state = terraform_state.parse(text)
for address in terraform_state.addresses(state)
    println(address)
end
var attrs = terraform_state.attributes(state, "aws_instance.web")
println(attrs["id"])
```

包内的 [XLang 范例](examples/basic.xl) 与 [C 对照范例](examples/basic_c.c)
在内存里查询同一份合成 state，不读取或输出真实凭据。
C 范例可直接 `xl run` 单文件运行；仅该路径在 TCC 下编入本包原生实现，
普通 C 构建仍按清单分离链接。双平台门禁分别核对两种 C 构建与 XLang 输出。

## 接口与范围

| XLang | 含义 |
| --- | --- |
| `parse(text)` | 解析并校验原始 v4 顶层形状，返回保留原始字段的 `var` |
| `valid(text)` | 检查语法、64 MiB 输入限额及顶层必需字段；不验证每个资源的全部字段 |
| `version(state)`、`serial(state)`、`lineage(state)` | 读取状态元数据 |
| `addresses(state)` | 按原始数组顺序生成资源地址 |
| `find(state, address)` | 查找资源，未找到返回 `null` |
| `instances(state, address)` | 返回实例数组，资源未找到返回 `null` |
| `attributes(state, address, index=0)` | 返回指定实例的属性对象 |
| `outputs(state)`、`output(state, name)` | 返回输出对象或单项；缺少名称返回 `null` |

`parse` 要求根为对象，`version` 为整数 4、`serial` 为非负整数、
`lineage` 为不含 NUL 的字符串、`resources` 为数组、`outputs` 为对象。
JSON 转义 `\u0000` 解码后仍按 NUL 拒绝，避免必需元数据通过解析却无法由
公开的 C 字符串访问器读取。未知顶层字段保留，
资源详情按查询懒校验。`valid` 为 false 不代表外部云资源仍存在、state 已同步，
也不代表某个 provider 的私有属性符合其 schema。

XLang 的资源地址和输出名称不能包含内嵌 NUL；`find`、`instances`、
`attributes`、`output` 会在进入 C 零结尾接口前抛 `ValueError`，
不会把 `"safe\0other"` 静默当成 `"safe"`。C 入口仍遵循普通
NUL 结尾字符串约定。

`valid` 遇到语法或顶层形状错误返回 `false`，不保留 XRT 错误；内存不足同样返回 `false`，但保留 `XERR_MEMORY`，C 调用方可据此区分资源故障。XLang 使用同一原生入口；`test/allocations.xl` 验证了内存故障映射为 `MemoryError`，并在清除注入后恢复正常调用。

`version`、`serial` 的 C 返回值和 `attributes` 的实例索引使用 `int64_t`，与 XLang `int` 的 C ABI 一致。
公开头自行声明 Value Container 依赖，可以单独包含，不要求 C 调用方预先定义 XRT 模块宏。

地址由可选 `module.`、可选 `data.`、`type.name` 拼接；`mode` 只允许
`managed` 和 `data`。`attributes` 的 index 是对应资源 `instances` 数组中的
位置，不会把 `index_key` 拼进资源地址。多实例时调用者应读取实例原始字段，
而不是把顺序索引当作 Terraform 的持久地址。查询返回的 `var` 保留 XRT 引用，
可以独立于源 state 变量存活；调用期间不得并发修改源值。
`addresses` 拒绝重复的合成资源地址；`find`、`instances`、`attributes` 在
目标地址匹配多项时也抛 `ValueError`，不会静默选中第一项。包括不同字段
拼出同一文本地址的情况。`parse` / `valid` 仍只校验顶层形状，不提前遍历
资源；因此顶层有效不保证随后每个地址查询都成功。

## C 接口和所有权

公共头为 [`include/terraform_state.h`](include/terraform_state.h)。
`xtf_parse_len` 和 `xtf_parse_value` 保留输入长度，内嵌 NUL 不会被截断成
另一份有效 JSON；`xtf_parse` / `xtf_valid` 是普通 NUL 结尾 C 字符串入口。
解析结果及所有查询返回的非 NULL `xvalue*` 均由调用方 `xrtValueRelease`。
`xtf_lineage` 返回独立 `char*`，由调用方 `xrtFree`；
`xtf_lineage_borrowed` 只在源 state 存活且未修改时有效。
`xtf_last_error` 是借用的线程局部错误文本。失败返回 NULL 或 false 时检查
`xrtGetError()`；资源未找到返回真正的 XRT null 值，不设置错误。

state 文件可能含明文敏感数据。此包不记录完整输入、属性或输出值，
范例也不使用真实 state。调用者应控制文件权限与日志内容。

## 验证

在仓库根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/terraform_state/test/check.ps1 -SkipAot
```

当前该门禁覆盖严格 GCC、C 边界、XLang JIT 与内存故障注入、C/XLang 范例逐行对照及
无输出边界 AOT。两平台门禁还将 C 范例与 `test/basic.expected.txt` 的七项固定结果比较，避免 C/XLang 同时输出错误值却因互相一致而通过。范例同时查询普通资源与 `module.site.data.aws_ami.ubuntu` 的属性。去掉 `-SkipAot` 可检查带输出范例的完整 AOT；当前
`xl build` 在共享运行时符号 `xl6ValueDataSnapshotV1` 上失败，见
`dev/extlib_blockers/text_snapshot_link.md`。另有独立主线问题：
`var` 字符串 `as string` 会得到错误结果，见
`dev/extlib_blockers/var_string_cast.md`；本包不靠该转换实现文本桥接。
因此当前只能记为**包级 C/JIT 可用、完整发布验收未签收**。

`test/boundaries.xl` 和 `test/allocations.xl` 已登记到包级 smoke，可从
仓库根目录直接运行；故障注入用例在每次探针后清除注入状态。

Linux/WSL 从仓库根目录运行 `bash release/extlibs/terraform_state/test/check.sh`：
它以严格 GCC 和 ASan/UBSan 运行 C 边界，使用正式 `release/xl` 运行
中央、包内边界和分配故障用例，并逐字节对照 C/XLang 合成 state 范例输出。
该脚本不读取真实凭据，也不覆盖完整 AOT。

2026-10-15：`valid` / `valid_value` 原在解析分配失败时清空 `XERR_MEMORY`，将资源耗尽伪装成无效 state。两入口的前 16 个分配点红绿回归后，仅清除格式/形状错误，保留内存错误；无效输入返回 `false` 且无错误也有断言。Windows 严格 GCC/C、TCC C、XLang JIT、无输出 AOT 和双语同输出通过；新增 `test/check.sh` 的 Linux ASan/UBSan 门禁通过。带输出完整 AOT 仍缺 `xl6ValueDataSnapshotV1`，未签收发布级。

2026-10-17：Linux 包级脚本加入正式 XLang 的中央/包内/故障用例及
C/XLang 范例逐字节同输出；与 C ASan/UBSan 同次通过。完整 AOT 和
`var as string` 的共享主线阻断仍未关闭。

2026-10-22：资源数组中出现重复或拼接冲突的地址时，原 `find` 静默返回
第一项、`addresses` 返回重复文本。C/XLang 红例先复现，现两个入口
明确拒绝歧义；`instances`、`attributes` 继承 `find` 的约束。Windows
严格 GCC、分配故障、双语范例及无输出 AOT，Linux C ASan/UBSan 与
XLang JIT 均通过。完整带输出 AOT 的共享阻断不变。

同日补查询名称的 NUL 前缀红绿回归：四个公开 XLang 查询入口原会命中
NUL 前缀对应的资源或输出；现在统一在包装层拒绝。Windows 包级
严格 GCC、C/XL 边界、双语输出和边界 AOT，Linux C ASan/UBSan 与
XLang 联合门禁均通过；带输出范例 AOT 的共享阻断不变。

2026-10-18：顶层 `lineage` 的 JSON 转义 NUL 原被 `parse/valid` 接受，
随后 `lineage(state)` 才报错。C/XLang 负例先红后绿，现于顶层校验时拒绝；
Windows 严格 GCC、C/XLang 边界、双语同输出与无输出 AOT、Linux
ASan/UBSan 联合门禁通过。带输出完整 AOT 的共享阻断未变。

2026-10-19：C 所有权回归直接释放源 state，再读取 `find`、`instances`、
`attributes`、`outputs` 和 `output` 的查询结果，固定独立持有合同。
Windows C 定向与原有 `-SkipAot` 包门禁、Linux C ASan/UBSan、XLang JIT
及双语范例门禁通过；库实现未改。完整带输出 AOT 仍缺共享
`xl6ValueDataSnapshotV1`，未签收发布级验收。

2026-10-20：Windows 包门禁改为每次使用独占构建目录并重编 XRT，
不再按旧对象时间戳跳过；成功和失败均只清理本次目录。
`-SkipAot` 的严格 C、C/XLang 边界、故障注入、双语范例及无输出
AOT 均通过。默认门禁在带输出范例 AOT 处仍缺共享
`xl6ValueDataSnapshotV1`，失败后的独占目录已清理；库 API 未改，
Linux C ASan/UBSan、XLang JIT 与双语门禁也已复验通过。
完整发布验收仍未签收。

同日补地址列表和按地址查找的逐点分配故障回归：C 与 XLang 均确认
`addresses` / `find` 遇内存不足时报告内存错误，清除注入后同一 state
仍可查询。Windows 严格 GCC、C/XLang 边界与双语对照、无输出 AOT，
以及 Linux C ASan/UBSan 与 XLang 门禁通过。包实现与公开 API 未变；
带输出完整 AOT 的共享缺符号阻断仍在。

2026-10-23：XLang 边界测试中七处单名 `catch ValueError` 改为明确的
类型捕获，固定 NUL 查询名、重复地址及非法 lineage 的异常分类，避免
其他异常误报通过。Windows `test/check.ps1 -SkipAot` 严格 C/XLang、
双语范例及边界 AOT，与 Linux `test/check.sh` C ASan/UBSan、XLang
门禁通过。解析实现/API 未改，带输出完整 AOT 的共享阻断仍在。
