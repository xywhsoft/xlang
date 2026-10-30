# imagehash

2026-10-30 冷启动与独立定义复核：未确认新的产品 BUG，算法/API/ABI、
stb 与冻结 XRT 未改。补六类首次 C 调用的独立进程预算与活动内存
平衡、三种哈希的冷 JIT/AOT，以及 20 张图/60 哈希/20 距离的独立
整数定义参照与三种破坏对照。Windows 默认 246 项符合预期；Linux
原生 ASan/UBSan、完整默认门禁通过，两平台各 11 份独立 AOT 程序。
新 `examples/similarity.xl` / `similarity_c.c` 展示图片候选筛选，默认、
同图与首图缺失在五条语言/引擎路径下验证。详细范围与现场见
[冷启动审查](COLD_HASH_AUDIT_2026-10-30.md)及[范例说明](examples/README.md)。
这不是任意图片/缩放器认证，也没有新增性能结论。

2026-10-28 完整公开返回链：三条 `@owned string?` 声明保持可空，
成功调用临时值通过 `??` 转移，失败才读取原有线程诊断并抛
`RuntimeError`。完整 cgen 夹具从三处结果正文复制减至零；真实
内存失败仍抛 `MemoryError`，没有把普通图片错误改成通用非空错误。
借用的版本/诊断字符串仍按快照返回，不接管线程缓冲所有权。

固定 tiny.png、预热后的完整解码/缩放/格式化，C/XL 各六组预算
通过，语言回归先红后绿：Windows C14、XL16→15；Linux C13、
XL15→14。差额仍是路径 NUL 针串分配，长度扫描/错误作用域也仍在，
不宣称整条操作零开销。另验证三条入口各自的空路径、缺失图片、
NUL 拒绝、精确诊断、持有结果及恢复。预算不外推冷启动或其他图片。

Windows/Linux 默认完整门禁通过严格 C、线程、原有 C/XL 全分配点
扫描、中央/新预算及双语 JIT/AOT。Linux ASan/UBSan 继续覆盖原有
完整图像链路；固定预算另用普通构建。stb_image_resize2 在 ASan 下
自动启用 `STBIR__SEPARATE_ALLOCATIONS`，其诊断分配不能套用普通
构建预算，未关闭该功能来凑预算。首次缺测试 event 模块和预算模式
混用的现场均保留，修正后用新快照完整通过。

测试运行库按产品依赖加文件/线程/event/内存调试编译并共享，记录
依赖、图片和发布物指纹，失败日志/产物保留。主线、XRT 和 stb 源码
均未改。测试准备另发现的 Windows `os.platform()` 缺符号已单列
`dev/extlib_blockers/os_platform_runtime_symbol.md`；产品 API 不依赖它，
预算由门禁显式传入。单独运行预算夹具：Windows 为
`xl run release/extlibs/imagehash/test/owned_results.xl -- 15`，Linux 为
`xl run release/extlibs/imagehash/test/owned_results.xl -- 14`。

2026-10-27 所有权收口：三种图片哈希的 C 返回类型明确为拥有型
`char*`，XLang 使用 `@owned string?` 直接接管，去掉
`point` / `takeCstr` 转发。生成 C 门禁逐项检查直接保存原生
指针，且不复制正文；接管仍需一次 `strlen` 获取长度。
文件/图片失败与内存失败继续分别走现有异常合同。

图片相似性指纹：`ahash(path)`、`dhash(path)` 和 `phash(path)` 各返回 16 位十六进制字符串；`hamming(left, right)` 计算两个指纹的位差。三种算法**不可跨算法比较**，结果不是密码学摘要，也不保证两张不同图片必有不同指纹。

```xlang
import imagehash

string a = imagehash.phash("photo.png")
string b = imagehash.phash("thumbnail.png")
int distance = imagehash.hamming(a, b)
```

路径由 `stb_image` 扩展库通过 XRT 打开，支持 Windows UTF-8 文件名；XLang 入口拒绝含内嵌 NUL 的路径，避免 C 字符串只读取前缀。缩放复用 `stb_image_resize2`，不再在本包内重复编译两份 stb。输入文件与解码尺寸采用依赖包的限制。哈希结果独立持有，xlang 自动接管；文件缺失、损坏或普通缩放失败时 xlang 抛 `RuntimeError`，内存不足抛 `MemoryError`。C 接口失败时返回 `NULL`，可读 `xihash_last_error()`；内存不足还保留 XRT 的 `XERR_MEMORY`，普通图片错误不泄漏依赖包的 XRT 错误。`valid` 严格要求 16 个十六进制字节，`hamming` 对非法输入返回 `-1`。

`phash` 取 32×32 灰度像素的 8×8 低频 DCT，舍弃 DC，按其余 63 个系数的中位数置位。DCT 比较使用定点整数，纯色图不会将浮点舍入噪声当作图像特征；这项口径不保证与其他软件的 pHash 位布局一致。`ahash` 用均值阈值，`dhash` 用相邻像素差，均以灰度图计算。

## C 接口

`include/imagehash_native.h` 公开对应函数。三种哈希返回 XRT 分配的字符串，C 调用方使用 `xrtFree`，不可用 CRT `free`。`valid` 与 `hamming` 接收指针和真实字节长度，避免内嵌 NUL 截断；输出总是小写 16 位 HEX。`xihash_last_error` 是线程局部诊断，只供失败后立即查看；同一实例没有可变状态。诊断缓冲使用 XRT 线程键，线程结束时自动释放；不依赖 TCC JIT 下不可靠的静态 TLS。
`valid` 的 `false` 是普通格式检查结果，不设置错误；真假两种结果均清除上一
操作留下的 `last_error`，不可把旧诊断当成本次检查结果。
`ahash`、`dhash`、`phash`、`hamming` 和 `valid` 在入口清除旧 XRT 错误；
成功哈希、正常数值查询和空路径等普通图片错误不会继承别处的内存故障。
本次真实分配失败仍保留 `XERR_MEMORY`。读取 `last_error` 本身不会清除诊断。

## 验证

2026-10-26：当前 Windows/Linux 发布物的默认包门禁均退出 0。
两平台均检查固定图像向量、线程错误隔离、XLang 中央/分配故障
JIT、核心/分配故障/公开范例独立 EXE，以及 C 范例单文件
`xl run`/`xl build` 与 XLang 范例的固定输出对照；Linux 另通过
C ASan/UBSan。旧日期的共享快照缺符号仅描述当时的发布物。
此验收不延伸为与第三方 pHash 位布局互通、不同算法交叉比较，
或任意图像输入的性能保证。

2026-10-25：WSL 复跑 `test/check.sh`，线程错误隔离、完整图片链路的
C ASan/UBSan、XLang JIT、分配故障以及 C/XLang 范例对照全部通过。
当前 Linux `release/xl` 是旧发布物；本次没有验证新版 Linux XL 的
独立 EXE AOT，不能据此签收跨平台发布级验收。Windows 当前发布版
完整门禁结果见下条记录。

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，
严格 C、固定图像向量、线程错误隔离、XLang 故障注入、C 范例直接
运行/构建、双语固定输出及核心/完整范例 AOT 均通过。旧的共享
快照缺符号是先前发布物状态；当日未复跑 Linux sanitizer，固定
样本也不能证明所有图像或其他 pHash 实现具有相同指纹布局。

2026-10-22：Windows 默认门禁原只构建无输出核心 AOT，即使公开的
XLang 范例 AOT 缺符号也会返回成功。现在默认模式还构建、运行完整
范例并核对固定输出；当前在 `xl6ValueDataSnapshotV1` 未解析处正确
失败，核心 AOT 和 C 直接 AOT 仍通过。`-SkipAot` 的严格 C、线程、
XLang JIT 与双语对照通过；两种模式都不留下本次独占范例产物。

2026-10-22：C 回归先复现 `hamming` 成功仍携带旧 `XERR_MEMORY`；
五个操作入口已隔离旧 XRT 错误，空路径仍通过 `last_error` 报告，
正常查询不再假报内存故障。Windows 严格 GCC、C/线程、XLang 与
双语对照及 C 直接 AOT，Linux C ASan/UBSan、线程和 XLang 联合门禁通过。
完整 XLang AOT 尚待共享文本快照符号修复。

从仓库根目录运行：

```powershell
.\release\extlibs\imagehash\test\check.ps1
.\release\xl.exe run test\cases\extlib_imagehash.xl
.\release\xl.exe run release\extlibs\imagehash\test\allocations.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package imagehash -Source release\extlibs\imagehash\test\test_imagehash.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package imagehash -Source release\extlibs\imagehash\test\test_error_threads.c
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package imagehash -Source release\extlibs\imagehash\test\bench_phash.c
.\release\xl.exe build release\extlibs\imagehash\test\aot_core.xl -o build\test\imagehash_aot_core.exe
.\build\test\imagehash_aot_core.exe
.\release\xl.exe run release\examples\extlibs\imagehash.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package imagehash -Source release\extlibs\imagehash\examples\imagehash.c
.\release\xl.exe run release\extlibs\imagehash\examples\imagehash.c
wsl bash release/extlibs/imagehash/test/check.sh
```

Windows `test/check.ps1` 是单入口门禁：按包清单模块执行严格 GCC 检查，
运行两组 C 边界、XLang 主用例与分配故障、C/XLang 范例同输出、C 范例
单文件直接运行/构建，以及独立核心 AOT；`-SkipAot` 跳过 XLang 核心
和完整范例 AOT。默认模式还构建、执行带输出的 XLang 范例并与固定结果比较；
单文件 C 范例只在 TCC 路径包含三个包内实现，正式包仍按清单分离
编译。脚本仅在 `build/test` 中建立本次独有的产物，成功后删除，失败
则保留日志、输入指纹和产物；完整结果返回预算及中央 AOT 也进入默认门禁。
开发门禁需要 GCC 和 Python 3，正常使用包/运行双语范例不依赖 Python。
全部子进程有时间上限；Windows 外部 `.report.json` 保存独立 stdout、
stderr、状态、预期码与原始码，成功清理独占目录，失败保留现场。
Linux `CC` / `PYTHON` 可指定工具，源与实际工具均参与前后指纹检查。

C/XL 测试固定纯色图结果，C 测试额外生成非均匀 PGM，校验跨 TCC/GCC 的 pHash 一致性、中文路径、损坏图片、无效/含 NUL 的指纹和多结果所有权。XLang 还覆盖三个哈希入口对内嵌 NUL 路径的拒绝。线程交错回归先复现了旧静态 TLS 的错误串覆盖，再验证 XRT 线程键修复；Windows JIT 与 Linux ASan/UBSan 均通过。Windows 严格 GCC、双语同输出及无输出 AOT 核心已通过。三个依赖包均声明所需 XRT 模块后，核心 AOT 从 3,879,424 字节降至 959,488 字节，构建产物已运行验证。带输出范例 AOT 在当时的发布物上受主线 `xl6ValueDataSnapshotV1` 缺符号阻断，见 `dev/extlib_blockers/text_snapshot_link.md`；当前 Windows 结果见上方 2026-10-24 记录。

分配故障回归分别扫描 `ahash`、`dhash`、`phash` 的图像解码、缩放和结果分配路径：C 侧各检查前 64 点，XLang 侧各检查前 128 点。底层可恢复的分配故障允许公开调用成功，但成功结果必须正确且无旧错误；公开调用失败时必须保留 `XERR_MEMORY` / 抛 `MemoryError`，清除故障后仍可恢复。缺失/损坏图片保持 `RuntimeError`，不会留下底层 XRT 错误。Windows 包级 C 与 XLang 用例、Linux 全链路 ASan/UBSan 与双语输出门禁均通过。
包内 `test/allocations.xl` 已登记为 `coverage.tests`，通用测试发现链会运行这条 XLang 故障恢复边界；它不替代 C sanitizer、线程隔离或 AOT 门禁。

2026-10-16：C/XLang 回归先复现 `hamming` 失败后 `valid` 保留旧诊断，
现由 `valid` 清除旧错误；两种布尔结果及 C 线程错误隔离用例复跑通过。
该项诊断修复当时未重跑 Linux sanitizer 或完整 AOT；后续 Linux
门禁结果见下文，完整 AOT 的发布阻断不变。

同日 C 主测试生成的梯度 PGM、中文路径 PNG 和损坏图片改放 XRT 创建的
独立系统临时目录，成功后清理，不再占用源码工作目录的固定文件名。
`phash` 的 DCT 定点系数由 XRT `once` 计算一次并复用，比较次序与
固定向量未变；新增八线程同时首次调用回归。可选 `bench_phash.c`
固定对同一素材调用 50000 次，只报告 CPU 时间，不设置跨机器阈值。
本机 TCC/Windows 两次旧实现约 4.27–4.36 s，缓存版两次约
3.88–3.92 s；吞吐改善不代表任意图片或机器的固定比例。
Linux ASan/UBSan 门禁已扩到完整解码、缩放与哈希链路，连同线程
错误隔离和首次初始化测试通过；C/XLang 范例输出一致。完整 AOT
仍受共享主线缺符号阻断。

2026-10-17：`test/check.sh` 将中央 XLang 用例、严格 GCC 编译的 C
范例与 XLang 范例逐字节输出对照纳入同一 Linux 门禁；完整 C
ASan/UBSan、线程测试和新增双语链路均通过。完整 AOT 阻断不变。

2026-10-20：旧 C/XLang 范例把 aHash 与 dHash 互比，和“不同算法不可比较”
的公开合同冲突。现改为两张固定图片的 pHash 距离，并用独立黄金输出
固定两个哈希值与距离 31；C/XLang 还覆盖 0、1、64 位差、大小写 HEX
和内嵌 NUL。新图片触发上游缩放包的未对齐 64 位写入，已在
`stb_image_resize2` 的 vendored 头文件中修复并纳入其 C sanitizer 回归。
Windows 两包门禁及 Linux 两包 ASan/UBSan、XLang/双语门禁通过。
完整带输出 AOT 仍受共享缺符号阻断，不据此签收发布级。
