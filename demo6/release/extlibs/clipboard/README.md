# clipboard 扩展库

Windows 剪贴板的 UTF-8 文本、文件列表、BMP 图片与格式元数据接口。此包只在 Windows 上构建；没有提供 Linux/macOS 的占位后端。

```xlang
import clipboard

assert clipboard.available()
assert clipboard.set_text("你好")
assert clipboard.get_text() == "你好"
```

`src/clipboard.xl` 提供文本、文件、图片、格式枚举和拥有型快照。C 用户包含 `include/clipboard.h`；所有 `xvalue*` 结果，包括文本、JSON 和 `x_clipboard_format_name`，均由调用方 `xrtValueRelease`。结果不借用包内线程缓冲，可在另一线程读取，但同一值的释放仍需由调用方协调。公开 C 整数参数/返回值为 `int64_t`，与 XLang `int` 的生成 ABI 一致。

```xlang
auto saved = clipboard.snapshot()
assert saved.valid()
assert clipboard.set_text("temporary")
assert saved.restore()
saved.release()  // 可选：对象析构时也会释放
```

C 对应入口是 `x_clipboard_snapshot()`、`x_clipboard_restore(handle)`、`x_clipboard_snapshot_release(handle)`；快照句柄由调用方持有和释放。释放后的句柄不可再次使用。不能在另一个线程释放快照的同时恢复它。

## 当前约定

- 输入字符串与路径固定为 UTF-8；无效 UTF-8 不回退到本地代码页。读取传统 `CF_TEXT`/`CF_OEMTEXT` 时，按 Windows 声明的代码页解码后转成 UTF-8。
- 注册格式名通过 Win32 Unicode 接口读取，再完整转换为 UTF-8；`format_name`、`formats` 和 `formats_json` 不使用固定 256 字节结果缓冲。过长名称超过受支持上限时显式失败，不静默截断。
- XLang 的 `set_text`、`register_format`、`set_file`、`set_image`、`save_image` 在进入 NUL 终止的 C 接口前拒绝内嵌 NUL，抛 `ValueError`，不会把前缀当作完整输入写入。`set_files` 使用长度感知的 `xvalue` 路径，含 NUL 项返回 false。C 入口仍遵循 C 字符串的 NUL 终止约定。
- `set_files` 的 C 入口仅接受非空字符串数组；非数组、空数组及非字符串、空字符串或含 NUL 的路径项返回参数错误，不修改剪贴板。`set_file("")` 同样返回参数错误。文件列表的 `CF_HDROP` 编码结果最多 256 MiB，超限在分配或写入前拒绝。
- `get_text` 在文本缺失或失败时返回空字符串，`last_error` 返回线程局部的错误类别；具体 Win32 错误码不会保留。`get_files` 在格式缺失时返回空数组；打开剪贴板、读取或转换失败时，C 入口返回 `NULL`。
- `snapshot` 在所有枚举格式都可复制时才成功；支持 `CF_BITMAP`、`CF_ENHMETAFILE` 及 Windows 从 EMF 合成的 `CF_METAFILEPICT`，分别复制图形句柄与嵌套的元文件句柄。`CF_PALETTE` 等尚不支持的格式明确失败，不会悄悄丢弃。快照最多 256 个格式、256 MiB 复制数据；这限制单次捕获的内存开销，不保证所有第三方剪贴板格式都可复制。失败不覆盖先前已持有的快照。
- `restore` 在清空剪贴板前预分配待转交的数据句柄；分配失败不会修改当前剪贴板。但 `SetClipboardData` 中途失败仍可能留下部分格式。它不是跨格式事务，也不是任意桌面剪贴板的无损备份机制。
- `set_image` 接受 BMP 文件，校验文件/DIB 头、尺寸、位深、像素偏移和实际像素字节数；当前支持未压缩及位掩码 DIB，输入上限 256 MiB。文件读取/写入错误按 XRT 错误类别映射，不解析错误消息。`save_image` 使用 XRT 原子文件写入；BMP 缓冲分配失败不会覆盖原目标文件。Windows 可以把剪贴板 DIB 转成 V5 格式，因此导出的 BMP 不保证与输入逐字节相同。
- 文本与 JSON 结果由调用方拥有，不再创建包级 Windows FLS 槽。`last_error` 使用 C 运行库的线程局部 `errno` 保存错误类别；与同线程其他 C 库共享，因此仅在紧接着 clipboard 调用时读取，不能当成可长期保存的诊断对象。快照是调用方持有的独立对象。Windows 剪贴板本身仍是系统共享资源，并发写入顺序由系统决定；同一个快照句柄不要并发恢复/释放。

## 验证

在 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/clipboard/test/check.ps1
```

门禁用独立 Windows window station 运行写入测试，不修改交互桌面的剪贴板。历史构建的 `-SkipAot` 曾通过严格 GCC、UTF-8/中文路径、文本和文件列表、NUL 截断拒绝及原值保持、120 字符中文格式名在 C 单项/列表/JSON 与 XLang 单项路径往返、注册格式原始字节快照往返、`CF_BITMAP`/`CF_ENHMETAFILE`/合成 `CF_METAFILEPICT` 快照与重复恢复、64 轮快照释放后 GDI 句柄数稳定、不支持格式失败、BMP 往返及畸形尺寸、XLang JIT 写入、26 个生成 C 原型与公开头一致性、C/XLang 基础与快照范例对照。测试专用 XRT 内存调试模块还逐点注入快照捕获故障，验证失败后原内容不变及恢复前分配失败不会清空剪贴板；发布清单不包含该模块。范例在 `examples/basic.*` 与 `examples/snapshot.*`。当前发布物的完整门禁状态见下方最新复核，不沿用旧 AOT 缺符号结论。

两份 C 范例可直接使用 `xl run` 或 `xl build`，不必手动添加头路径、包源码和 Windows 链接库。`basic_c.c` 只读取能力与格式信息；`snapshot_c.c` 会改写剪贴板，务必在隔离桌面测试。包门禁使用独立 window station 分别运行 JIT 与构建产物，并验证恢复后的内容。

图片门禁另验证不存在的输入路径分类为未找到，以及 BMP 缓冲分配失败时旧目标文件原字节保留、清除故障后可正常保存。

2026-10-15：`set_files` 的非数组输入曾被误报为“数据不可用”。新增纯参数负例覆盖非数组、空数组和非字符串项；现均归为参数错误。非字符串项此前还会留下 XRT `TypeError`，导致 XLang 在返回 `false` 前抛异常；现在先检查元素类型，C/XLang 均保持 `false + last_error` 语义。空路径在单文件和文件列表写入中也统一报参数错误。文件列表编码总长度增加上限及溢出检查；隔离桌面 C/XLang 门禁和生成 C 原型校验通过。真实第三方剪贴板格式与完整 AOT 仍待验收。

当前是包内开发进度，不是发布级签收。`test/check_lifecycle.ps1` 曾稳定复现 30 次导入上下文损失 30 个 FLS 槽；改为拥有型 `xvalue*` 文本/JSON 并移除模块级 FLS 后，同一宿主 30 次普通和 30 次导入上下文循环均保持可用槽数 4070 不变。尝试把错误槽迁入 XRT 错误对象会触发 xlang 自动异常检查，因此目前使用不分配额外槽位的线程局部 `errno` 类别；精细错误诊断仍可改进。另需在真实桌面验证更多第三方剪贴板格式，并在主线链接问题解决后完成 AOT 门禁。
2026-10-17：公开头原依赖调用方先启用 XRT value 模块才可见 `xvalue`；
现补前置声明，并在包级门禁中固定无模块和显式模块两种独立包含编译。
隔离剪贴板 C/XLang、生成 C ABI 与双语范例复跑通过；完整 AOT 阻断不变。

2026-10-20：包内私有的 UTF-8/UTF-16 转换及文件列表临时缓冲统一使用 XRT 分配器；交给 Windows 剪贴板的 `HGLOBAL` 仍按 Win32 约定使用 `GlobalAlloc`/`GlobalFree`。隔离 C 用例新增文本转换缓冲分配失败注入，验证错误分类、原内容保持与故障解除后的正常写入。Windows `test/check.ps1 -SkipAot` 的 C、XLang、范例及生成 C ABI 门禁通过；未执行受共享主线缺符号阻断的完整 AOT，不据此签收发布级。

同日两套 Windows 门禁改为各自使用单次独占构建目录，退出时仅清理本次产物。隔离 C 图片用例从门禁接收相对构建目录，仍用中文文件名验证 Win32 与包内 UTF-8 路径；单独运行该 C 用例时须传入已存在的测试目录。`check.ps1 -SkipAot` 全通过；默认门禁在 AOT 构建处仍缺共享 `xl6ValueDataSnapshotV1`，失败后无本次目录残留。`check_lifecycle.ps1` 的 30 次普通及 30 次导入上下文循环通过，FLS 可用槽数均为 4070；其独占目录也已清理。旧固定测试目录未动。

2026-10-21：中央 `release/examples/extlibs/clipboard.xl` 不再写入固定的
`examples_out/extlibs`，改用独占临时目录并在 `finally` 清理；仅在取得有效
快照后才修改剪贴板，退出时重试恢复并释放快照。不存在的 BMP 输入在
XLang 层抛 `FileError`，范例现在明确捕获这一负例，不再误认为返回 `false`。
Windows 包门禁在独立 window station 中运行该完整公开范例，并验证结束后
剪贴板仍是原始文本；失败时隔离运行器会输出子进程诊断。严格 GCC、C/XLang
与生成 ABI 的 `-SkipAot` 门禁全通过。真实交互桌面的第三方格式和完整 AOT
仍未签收。
同日不带 `-SkipAot` 的默认门禁复验仍在首次 XLang AOT 构建报
`xl6ValueDataSnapshotV1` 未解析；新纳入的公开范例在该步骤前已通过隔离验证。

2026-10-24：`Snapshot.destroy` 原在释放原生快照后写 `self.handle = null`，
导致有效快照在局部对象退出时抛 `RuntimeError`；隔离桌面最小用例先红后绿，
JIT 与独立 AOT 均退出 0 且将剪贴板留为预期文本。显式 `release()` 仍会
在对象有效期内清句柄。当前完整门禁的 C/只读/负例阶段通过，但
`test/mutate.xl` 在 `set_files(array)` 处失败：生成 C 传入数组投影，
原生入口要求 XRT `XVALUE_ARRAY`。见 `dev/extlib_blockers/foreign_var_container_projection.md`；
包内不解析编译器私有 handle，整包未签收发布级。
