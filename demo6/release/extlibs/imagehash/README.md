# imagehash

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

## 验证

从 `demo6` 目录运行：

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
AOT。单文件 C 范例只在 TCC 路径包含三个包内实现，正式包仍按清单分离
编译。脚本仅在 `build/test` 中建立本次独有的产物，结束时删除。

C/XL 测试固定纯色图结果，C 测试额外生成非均匀 PGM，校验跨 TCC/GCC 的 pHash 一致性、中文路径、损坏图片、无效/含 NUL 的指纹和多结果所有权。XLang 还覆盖三个哈希入口对内嵌 NUL 路径的拒绝。线程交错回归先复现了旧静态 TLS 的错误串覆盖，再验证 XRT 线程键修复；Windows JIT 与 Linux ASan/UBSan 均通过。Windows 严格 GCC、双语同输出及无输出 AOT 核心已通过。三个依赖包均声明所需 XRT 模块后，核心 AOT 从 3,879,424 字节降至 959,488 字节，构建产物已运行验证。带输出范例 AOT 仍受主线 `xl6ValueDataSnapshotV1` 缺符号阻断，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`，故尚未签收正式发布级验收。

分配故障回归覆盖图像解码到缩放链路：C 侧检查失败时仍为 `XERR_MEMORY`，成功后无旧错误；XLang 侧检查 `MemoryError` 与故障解除后的恢复。缺失/损坏图片保持 `RuntimeError`，不会留下底层 XRT 错误。Windows 包级 C 与 XLang 用例、Linux 全链路 ASan/UBSan 与双语输出门禁均通过。
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
