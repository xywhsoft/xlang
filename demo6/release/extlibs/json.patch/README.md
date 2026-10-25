# json.patch

2026-10-25 当前 Windows 发布版复验：严格 C 边界、正常 `_value`
指针入口以及两份 C 范例通过；XLang 字典传入原生 `xvalue*` 后，
`has` 对存在键返回 `false`，`get` 报路径不存在。最小复现及
容器投影边界分析见 `demo6/dev/extlib_blockers/json_patch_typed_dict_bridge.md`。
默认包门禁在 XLang 边界退出 1，尚未执行完整 AOT；本包不能签收
发布级。以下旧记录描述当时的发布物与实现，不代表当前门禁通过。

此包提供 JSON Pointer、JSON Patch、JSON Merge Patch 的 XRT `xvalue` 操作。
指针、操作补丁和合并补丁分别以 [RFC 6901](https://www.rfc-editor.org/rfc/rfc6901)、[RFC 6902](https://www.rfc-editor.org/rfc/rfc6902)、[RFC 7396](https://www.rfc-editor.org/rfc/rfc7396) 为依据。

包清单仅声明值图和 Unicode 两个直接 XRT 模块。C 测试宿主另启用 JSON
读取与内存故障注入模块；它们不属于包的运行时依赖。裁剪测试宿主已通过
全部 C 边界与双语范例，完整 AOT 仍需等待共享缺符号修复。

## 当前 API

```xlang
import json.patch

var old = {"values": [1, 2]}
var changed = json.patch.apply(old,
    [{"op": "add", "path": "/values/1", "value": 9}])
assert json.patch.get(changed, "/values/1") == 9
assert json.patch.get(old, "/values/1") == 2
```

- `apply(document, operations)`：支持 add/remove/replace/move/copy/test，先复制文档；失败不改输入。补丁数组的每一项必须是对象，`null`、数字、字符串和数组项均报 `ValueError`，不会泄露底层对象读取的类型错误。空指针 `""` 定位根；根 remove 映射为 JSON `null`，作为本包的可表示结果约定。
- `move` 的源与目标为同一路径时，确认源存在后直接保留当前文档，不重复复制和删除；缺失的源仍报错。
- `merge(document, patch)`：对象递归合并，`null` 字段删除键；非对象补丁替换文档。
- `get/has/set/remove(document, pointer)`：单路径访问及复制后修改。
- `encodePointer(key)`：按 `~0`/`~1` 编码单个键段，返回带 `/` 的路径。

公开 C 头为 `include/json_patch.h`。输入值只借用；结果 `xvalue*` 由调用者
`xrtValueRelease`。失败时结果可能是 XRT `null` 值，必须检查
`xjsonpatch_has_error()`；合法 JSON `null` 不代表失败。
`xjsonpatch_encode_pointer_len` 返回独立分配的 C 字符串，调用者 `xrtFree`；此便利入口不接受含 NUL 的键。完整字符串可使用 `xjsonpatch_encode_pointer_value`，返回的 `xvalue*` 由调用者释放。
`xjsonpatch_has_len(NULL, ...)` 是无效文档参数：返回 `false` 并设置 `XERR_VALUE`；合法文档中的缺失路径仅返回 `false`。
输入文档或值复制时若 XRT 报内存不足，包会保留 `XERR_MEMORY`；只有不存在底层错误的无效补丁/路径才补充 `XERR_VALUE`。`apply/set/remove/merge` 已逐点注入分配失败，检查错误类别、输入不变和释放平衡。
四个接收 `xvalue*` 指针的 C 入口在参数检查前清理旧错误：`NULL` 指针
报告本次 `XERR_VALUE`，错误类型由 XRT 报 `XERR_TYPE`，不会继承上次
操作的 `XERR_MEMORY`。XLang 的公开指针参数仍声明为 `string`。

## 边界

- 路径最多 32 段，每段最多 255 字节；超过限制明确拒绝。指针要求有效 UTF-8，支持解码后的 NUL 键。C 的 `_len` 入口接受显式长度；零终止便利入口只能表达普通 C 字符串。XLang 通过 `xvalue` 桥传入完整字符串。
- 由上述段数和 `~0`/`~1` 转义规则推得，原始路径最多 16,352 字节；C 显式长度入口在 UTF-8 扫描前拒绝更长的声明长度。恰好达到该上限的合法路径仍可解析。
- 四个 C 零终止路径便利入口也只扫描至 16,352 字节上限外一字节；超限路径在解析前拒绝，不再先执行无界 `strlen`。C 调用方仍须保证缓冲区在遇到 NUL 或到达此边界之前可读；任意无效指针无法由包层保护。
- `encodePointer` 同样限制原始键段为最多 255 字节，保证生成的单段指针可由本包解析；`/`、`~` 转义后路径总字节数可超过 256，限制按解码后的键段计算。
- C 显式长度入口在读取键缓冲前检查这个 255 字节上限；超限直接报错，调用方仍须保证限额内的字节可读。
- Merge Patch 对象嵌套最多 64 层；过深输入显式报错。对象键不受 JSON Pointer 的 255 字节段长限制。
- Merge Patch 的对象字段递归合并，`null` 删除对应键；数组整体替换，不逐项合并。补丁是对象而目标不是对象时创建对象结果；补丁不是对象时替换整个根值。C/XLang 边界均验证这四种组合且原目标不变。
- 输入应是 JSON 可表示的值；其他带身份/循环引用的 XRT `xvalue` 图不承诺 RFC JSON 语义。
- 当前 `xl build` 带输出/容器访问的用例还被主线 AOT 缺少
  `xl6ValueDataSnapshotV1` 阻断；包内不会修改编译器主线。
  最小 `test/aot_core.xl` 无输出且仅做对象 `has/get`，`xl run` 成功，
  `xl build` 仍缺同一符号，见 `dev/extlib_blockers/text_snapshot_link.md`。

## 定点验证

包内 `test/test_boundaries.c`、`test/boundaries.xl` 覆盖数组插入、根路径、
内嵌 NUL、长路径与畸形 UTF-8、同数组双向移动、复合值逻辑相等、
补丁原子性、合并深度、旧错误污染及复制失败的错误分类。
独立 `test/check_oracle.ps1` 用固定种子构造 160 组 JSON 文档差分，
再加入 move/copy/test/数组追加 4 组明确操作；用
`python-json-patch 1.35` 计算预期结果，同严格 GCC 构建的 C 包逐值比较。
它不依赖 XLang 导入，也不替代下述语言与 AOT 门禁。首次运行先安装
仅用于测试的依赖（安装目录不进入发布包）：

```powershell
python -m pip install --target build/test/json_patch_oracle_deps jsonpatch==1.35
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/json.patch/test/check_oracle.ps1
```

分配故障扫描还覆盖同数组 `move`、对象 `copy` 和 `test` 的结构相等比较，
检查原文档不变、内存错误类别与释放平衡；比较期间的分配失败不得误报
为补丁条件不匹配。
`test/allocations.xl` 在公开语言入口逐点注入故障，验证 `apply` 与 `merge`
的 `MemoryError` 分类、完整结果及原文档不变；已接入包清单和双平台门禁。
`examples/basic_c.c` 与 `examples/basic.xl` 运行输出相同；Windows 严格 GCC、
Linux ASan/UBSan 和 XLang JIT 已通过。
`examples/workflow_c.c` 与 `examples/workflow.xl` 是第二组成对范例：先用
`test/copy/move/replace/add/remove` 原子操作更新文档，再用 `merge/set/has/get`
处理配置，并检查原文档不变；`encodePointer` 保留在入门范例中。
两组范例均由 Windows 包门禁逐字对照；Linux sanitizer 门禁也运行 C 工作流。
Linux 包门禁现还需要 `release/xl`，运行中央与包内 XLang 边界，
并将两组 C/XLang 范例分别逐字比较；未运行完整 AOT。
两个 C 范例也可作为单文件直接执行：它们包含包内公开头和原生实现，
无需手动添加头文件目录或链接参数；正式包仍由清单编译原生源文件。

在 `demo6` 目录运行：

包内 `test/boundaries.xl` 已登记到 `coverage.tests`；
`tools/smoke_extlib_catalog.ps1 -Package json.patch` 会连同原主用例复验。

```powershell
./release/xl.exe run release/extlibs/json.patch/examples/basic_c.c
./release/xl.exe run release/extlibs/json.patch/examples/workflow_c.c
pwsh -NoProfile -File release/extlibs/json.patch/test/check.ps1 -SkipAot
wsl bash release/extlibs/json.patch/test/check.sh
```

2026-10-15：中央 `test/cases/extlib_json_patch.xl` 中的非法指针 `has`
断言更新为显式捕获 `ValueError`，与包内合同和边界用例统一；完整中央
JIT 用例、包级 C/XLang 门禁及 Linux ASan/UBSan 均通过。原生源文件的
XRT 模块宏改为条件定义，清除与包清单同时启用模块时的重定义警告。
中央用例的 AOT 仍缺 `xl6ValueDataSnapshotV1`，不作发布签收。

2026-10-16：按 [RFC 6902 附录 A](https://www.rfc-editor.org/rfc/rfc6902#appendix-A)
补 C/XLang 对照：`/~01` 必须解成键 `~1` 而非 `/`，向 `/-` 加入数组时
该数组应作为单个元素保留。Windows C/XLang 包级门禁和 Linux ASan/UBSan
通过；未改实现，完整 AOT 的共享缺符号阻断不变。

2026-10-16：新增覆盖全部公开操作的 C/XLang 工作流范例，并接入包级
同输出门禁；Linux C 门禁复用单次编译的 XRT/原生对象运行边界和工作流。
Windows C/XLang JIT、Linux ASan/UBSan 均通过，库实现和 AOT 阻断未变。

同日 Linux 门禁补中央与包内 XLang 边界，并将入门/工作流两组 C/XLang
范例逐字对照；完整 C ASan/UBSan + XLang 包级脚本通过。
完整 AOT 的共享缺符号阻断不变。

2026-10-17：补齐上述 XLang 包装层故障回归；C 层原有逐点故障测试
保持不变。Windows 目录 smoke、包级门禁和 Linux ASan/UBSan 包级
门禁通过；完整 AOT 共享符号阻断仍未解除。

同日补 `move` 的转义祖先路径边界：`/~01` 解码为键 `~1`，不能移动到
`/~01/child`。C 与 XLang 均验证拒绝且原文档不变；Windows 包级门禁、
Linux C ASan/UBSan 与双语对照同次通过。实现无需修改，完整 AOT 阻断不变。

2026-10-17：`encodePointer` 原可返回本包解析器无法使用的超长单段路径。
256 字节键的 C 负例先红后绿；255 字节键，以及其中含 `/`、转义后路径
超过 256 字节的键，在 C/XLang 中均可编码并访问。Windows 包级 C/XLang
及 Linux ASan/UBSan、双语对照通过；完整 AOT 的共享缺符号阻断不变。

2026-10-18：`encodePointer` 的 C 长度入口原在检查 255 字节上限前以调用方长度扫描 NUL；1 字节无终止符缓冲配 `SIZE_MAX` 的 Linux ASan 负例复现栈越界读。现在先按现有合同拒绝超长键，再查 NUL/UTF-8；Windows `-SkipAot` 全包门禁与 Linux ASan/UBSan、XLang、双语对照通过。完整 AOT 阻断不变。

同日审计四条路径 `_len` 入口：共享解析器原在段数限额前扫描声明长度，`/` 单字节缓冲配 `SIZE_MAX` 再次由 ASan 复现越界读。现在在首次读取前按 32 段、每段最多 255 字节的既定合同拒绝超过 16,352 字节的原始路径；最大合法转义路径正例通过。Windows/Linux 包级门禁通过，完整 AOT 阻断不变。

2026-10-18：Windows `test/check.ps1` 原复用固定 `build/test/json_patch_guard` 产物，两次并行运行可能互相覆盖。现在每次创建独立子目录，结束时核对绝对路径并只清理本次产物；不复用无效的旧 XRT 对象缓存。`-SkipAot` 下严格 GCC、C/XLang 边界、分配故障与两组双语范例同输出均通过。库 API 和完整 AOT 阻断不变。

2026-10-19：两个 C 范例改为可由 `xl run` 单文件直跑，Windows/Linux
门禁都将直跑结果与严格 C 编译结果逐字比较；PowerShell 7 的 Windows
`-SkipAot` 门禁及 Linux C ASan/UBSan、XLang 双语门禁通过。系统
PowerShell 5 本机复跑遇到 GCC 编译 XRT 对象时内存不足，不能把该次运行
计为通过。完整 AOT 共享缺符号阻断不变。

2026-10-20：四个 C 零终止路径便利入口统一有界扫描。新增恰好最大长度
可解析、超限且无 NUL 的缓冲区返回 `XERR_VALUE` 的 C 回归；Windows
PowerShell 7 `-SkipAot` 与 Linux C ASan/UBSan、XLang、双语对照均通过。
XLang API 未改，完整 AOT 的共享阻断仍未解除。

同日收紧 `move` 的同一路径边界：源存在时直接保持文档，不做额外深拷贝、
删除和插入；源缺失仍拒绝。C/XLang 新增根、对象、数组及缺失源用例；
另固定 `copy /0` 到 `/0` 仍须在数组中插入副本，不能共用移动的无操作路径。
Windows `-SkipAot` 和 Linux ASan/UBSan、双语门禁通过。完整 AOT 阻断不变。

同日补强默认 Windows AOT 门禁：边界 AOT 后还需构建/运行入门范例与
完整工作流范例，分别与 C 输出比较，工作流另核对固定结果。当前
`-SkipAot` 的严格 C、边界与双语范例通过；默认门禁在前置边界 AOT
构建遇到共享 `xl6ValueDataSnapshotV1` 缺符号，新增两段尚未执行到。
独占构建目录在失败后清理；不据此签收完整 AOT。

2026-10-21：按 RFC 6902 附录 A.11、A.12、A.15 补 C/XLang 双侧回归：
未知操作字段忽略，不存在的目标父节点拒绝，字符串与数字的 `test` 不相等；
另固定 `replace` 后 `test` 失败时调用方文档仍保持原样。Windows
`-SkipAot` 严格 C/XLang 与双语范例、Linux C ASan/UBSan 和 XLang
对照门禁通过，包实现与公开 API 无需修改。默认 Windows 门禁在边界
AOT 构建处仍缺共享 `xl6ValueDataSnapshotV1`，暂不签收整包发布级。

2026-10-21：Merge Patch 的对象递归改为先深拷贝目标一次，再在独立副本中合并；失败时释放整份副本，调用方文档不变。32 层对象的 C 回归在修改前触发 300 次分配故障阈值，修改后可在阈值内完成并核对新旧叶值。Windows `test/check.ps1 -SkipAot` 的严格 C、XLang 与双语范例及 Linux `test/check.sh` 的 C ASan/UBSan、语言对照通过。公开 API 未变；完整 AOT 的共享缺符号阻断仍在。

2026-10-24 复验：Windows `test/check.ps1 -SkipAot` 的严格 C、分配故障、
XLang 边界及两组 C/XLang 对照通过；Linux `test/check.sh` 的 C
ASan/UBSan、XLang 边界和对照通过。Windows 默认完整门禁在 AOT 构建时
仍因主线缺少 `xl6ValueDataSnapshotV1` 退出，不据聚焦测试签收发布级。
