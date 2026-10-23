# xfeat

XFeat 特征匹配扩展库：通过 OpenCV 读取图片、ONNX Runtime 提取特征，使用双向描述子匹配和 RANSAC 仿射估计定位目标。

**尚未达到完整发布级验收。** 本机已有兼容的外部 XFeat 模型，真实图片局部定位已通过；权重不随本包分发。包内 `test/assets/target.png` 与 `unrelated.png` 来自本仓库 IDE 文档截图，分别提供可分发的正例目标和不相关负例。创建引擎必须显式提供 `xfeat.open(query_model_path, target_model_path)`；无模型的假默认入口已删除。`release/examples/extlibs/xfeat.xl` 只验证 ABI 与失败路径，`examples/live_model.xl` 才运行真实定位。

`include/xfeat.h` 提供 C 接口。JSON 结果是独立持有的 XRT 字符串：C 调用 `xfeat_stats_json_native` 或 `xfeat_location_json_native` 后以 `xrtFree` 释放；失败返回 `NULL`，不能当作空 JSON。XLang 包装层自动接管成功结果，失败时抛 `RuntimeError`；正常未找到则返回包含 `"found":false` 的 JSON。`last_error` 借用当前线程的错误槽，不应跨线程保存。统计桶为进程共享状态，读写由 XRT 互斥锁保护。

已修正 C `int` 与 XLang `int` 的 ABI 声明，动态尺寸、索引和最小内点数在收窄到 `c_int` 前检查范围。匹配核心拒绝不足三个内点和无效阈值，避免读取未初始化仿射。传给 C 的模型路径、图片路径和结果名称会拒绝内嵌 NUL，避免静默截断；在分配定位结果前完成检查。ONNX 桥接通过 XRT UTF-8 路径加载，并使用 XRT once 处理并发首次加载；旧 ANSI 加载和无效候选目录已移除。JSON 由 XRT 结构化写入，不再复用全局缓冲或截断长名称。

`test/test_match.c` 用合成特征点验证平移、拒绝匹配和参数边界；它包含实现文件以构造内部特征行，这不是公开 C 构造 API。`test/test_loader.c` 使用极小的测试桥接 DLL，验证中文路径、公开 C 特征行读取、匹配、补零区过滤、长 JSON、八线程冷加载、错误隔离和统计计数。ONNX 输出形状只接受 `[N,67]` 或 `[B,N,67]`，总数必须匹配，最多 10 万行；零特征合法，非有限坐标和描述子明确失败。`test/session_lifetime.xl` 使用真实 ONNX 桥接和已有 OCR 权重，只验证普通和带线程选项的会话创建/关闭。真实 XFeat 推理已另用兼容模型验证。会话归引擎持有；特征提取调用 C ABI 时只借用会话句柄，不再让临时拥有型对象提前释放它。

预处理固定为 BGR 转 RGB、按 `1/255` 归一化、右下补零到 32 的倍数，C 结果解析过滤补零区坐标。定位器有意拒绝宽或高超过目标图一半的结果，因此整图对自身不是正例；`examples/live_model.xl` 将目标图中心四分之一区域另存为查询图，再验证局部定位。`locate_and_save` 在正常未找到时保存原图，在读图、绘图或写盘失败时抛 `IOError`，不返回成功 JSON；输出路径由调用者提供，建议使用构建目录。

在 `demo6` 目录运行 Windows 门禁；默认还构建运行公开 XLang 范例 AOT，
`-SkipAot` 只用于 C/JIT 聚焦复验，不能作为发布签收：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/xfeat/test/check.ps1
```

Linux C 核心门禁（含 ASan/UBSan）：

```sh
sh release/extlibs/xfeat/test/check.sh
```

Linux 脚本同时运行无模型 `test/aot_core.xl`，核对 XLang 包导入与
`available()` 调用。2026-10-18 中央用例已按显式引擎 API 修正，
并纳入 Windows/Linux 包级门禁。中央用例只验证无模型失败路径；
实际桥接可用性和真实模型推理由 Windows 专项门禁负责。

`test/test_loader.c` 在测试版 XRT 上遍历 JSON 内存故障点，确认 C 层不把失败伪装为有效结果；Windows/Linux 同时运行 `test/json_failure.xl`。Windows 另用真实图片夹具运行 `test/save_result.xl`，覆盖保存成功、缺输入不改写已有输出和写盘失败。Linux 当前未提供可加载的 OpenCV 桥接，因此不把图片保存的 XLang 测试列入 Linux 门禁；它不能作为 Linux 图片保存能力的验收依据。

设置 `XFEAT_MODEL` 后，Windows 门禁自动使用包内两张图片，运行 C/XLang 的真实定位正例和不相关图片负例；可用 `XFEAT_TEST_IMAGE` 覆盖默认正例，此时跳过只针对固定夹具的负例。加 `-ParallelLive` 时，两条线程各自创建引擎和查询集，独立定位同一目标，均需成功。模型仍由调用者提供；完整 AOT 尚未验收，插件内部长期并发稳定性由插件维护者验收。`docs/site.html` 已作为官网详情页的可维护源文件接入，页面明确标注这些边界。

C 范例现在与 XLang `prepare_query` 一样提取 0.70、0.85、1.00、1.15、1.30、1.50 六个尺度，并按相同得分选最佳定位；真实模型门禁逐行比较两种语言的正例输出，而非仅核对 `found=true`。不相关图片继续分别验证未找到路径。

2026-10-15：manifest 声明原生代码使用的 XRT 路径、环境、once、互斥锁、
线程键、值容器、JSON、Buffer 和 Unicode 模块。无模型
`test/aot_core.xl` 调用公开 `available()`，同一 Windows AOT 从
4,162,048 降至 1,301,504 字节，构建和运行均通过，已纳入包级门禁。
严格 C、桥接调用、真实 ONNX 会话所有权和 XLang 失败路径复跑通过；
随后用本机兼容模型与真实局部图验证了 `found:true`，并复跑
Windows 严格门禁及 Linux ASan/UBSan C 门禁；模型不进入仓库。

2026-10-18：复用本机外部兼容模型执行 `test/check.ps1 -ParallelLive`，
严格 C、UTF-8 桥接、真实 ONNX 会话、XLang ABI/参数边界和核心 AOT
均通过；固定正例的 C/XLang 六尺度定位 JSON 逐行相同（177 次匹配、
63 个内点），不相关图片两端均拒绝，两线程独立推理通过。模型权重
仍不随包分发；完整范例 AOT 与插件长期并发稳定性未因此签收。

2026-10-20：Windows 门禁的 `中文路径` 原位于无 BOM UTF-8 脚本中，
PowerShell 5.1 会按系统代码页误读。脚本现带 UTF-8 BOM；默认门禁
通过严格 C、假桥接中文路径、真实 ONNX 会话、XLang ABI/参数边界和
无输出核心 AOT。未运行需要外部模型的 `-ParallelLive`，也不将本次
门禁视为插件内部长期并发或完整范例 AOT 的签收。

2026-10-20：Windows/Linux 包级门禁现为每次运行创建独立临时构建目录，
结束时仅清理本次目录，避免并行门禁互相覆盖产物；保留已有固定目录
中的文件。Windows 默认门禁和 Linux C ASan/UBSan、XLang 用例均通过。
这只改测试隔离，不改变包 ABI，也不解除完整带输出 AOT 的主线阻断。

2026-10-23：默认门禁新增公开范例 AOT 构建与固定输出检查；当前严格 C、
真实 ONNX 会话、XLang JIT 与无输出核心 AOT 均通过，范例 AOT 在共享
`xl6ValueDataSnapshotV1` 未解析处准确失败。`-SkipAot` 聚焦模式通过，
独占构建目录在成功和失败后均已清理；不据此签收整包发布级。
