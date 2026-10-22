# totp

`totp` 提供 RFC 4226 HOTP 与 RFC 6238 TOTP 的 **HMAC-SHA1** 版本。密钥使用 RFC 4648 Base32 文本；接受大小写字母及合法的可选 `=` 尾部填充，但拒绝空密钥、非法字符、内部填充、错误填充和非零填充位。当前限制编码密钥为 1..256 字节，不自动忽略空格。哈希与时钟复用 XRT，包内只组合 HMAC-SHA1。

## XLang API

- `hotp(secret, counter, digits = 6)`：计数器非负，位数 6..9。
- `generate(secret, timeStep = 30, digits = 6)`：按当前 Unix 时间生成 TOTP。
- `generateAt(secret, timestamp, timeStep = 30, digits = 6)`：按给定非负 Unix 秒数生成，便于测试和时钟可控场景。
- `verify(secret, code, timeStep = 30, digits = 6, window = 1)` / `verifyAt(secret, code, timestamp, timeStep = 30, digits = 6, window = 1)`：返回布尔值；窗口在当前时间步前后各扩展 `window` 步，范围 0..10。`timeStep` 范围 1..2147483647 秒。

候选码必须恰好 `digits` 个 ASCII 数字；正常不匹配返回 `false`，非法密钥或参数抛 `ValueError`。C 验证接口相应返回 `1` / `0` / `-1`。文本结果为独立 XRT 分配：C 调用方 `xrtFree`；XLang 自动接管。C 接口和长度参数见 `include/totp_native.h`，均可显式拒绝嵌入 NUL 的密钥或验证码。
在 C 入口中，空密钥指针报告 `XERR_ARGUMENT`；非空指针配零长度或超过 256 字节报告 `XERR_RANGE`，非法 Base32 内容报告 `XERR_VALUE`。

公开 C 调用在入口清理本线程上次的 XRT 错误；成功生成或正常验证码不匹配时，`xrtGetError()` 为 `NULL`。失败后可直接再次调用，无需调用方先手动清错。

`generate` / `verify` 获取系统时间失败时分别返回 `NULL` / `-1`，保留 XRT 的 `XERR_IO`，不会把失败时的零时间戳当作有效 Unix 纪元。Linux 包级门禁通过一次性 `clock_gettime` 故障注入验证该路径，并检查随后的正常调用可以恢复；指定时间戳的 `generateAt` / `verifyAt` 不读取系统时钟。
实时 C 入口会先检查步长、位数及验证窗口，再读取系统时间；参数非法与时钟同时故障时优先报告 `XERR_RANGE`，且不消耗时钟调用。

合法输入生成验证码时若输出分配失败，XLang 抛 `MemoryError`，C 返回 `NULL` 并保留 `XERR_MEMORY`；失败后可再次调用。验收只覆盖生成入口的这一分配边界，不推断服务端限流或密钥管理能力。

## 安全边界

验证循环不因窗口内提前命中而缩短，逐位比较验证码；它并不代替服务端**限流、计数器/时间步重放防护、密钥保密和安全传输**。窗口越大，可接受的候选码越多；生产环境应按时钟同步能力选最小窗口。HOTP 计数器的持久化、消费与防重放由调用方负责。当前只承诺 SHA-1 互操作，不提供 SHA-256/512 或密钥供应/二维码系统。

## 范例和验证

2026-10-22：Linux 时钟故障注入先复现实时 C 入口用 `XERR_IO` 遮蔽非法
步长/窗口，现先校验参数再读时钟；正常时钟故障仍保留 `XERR_IO`。
Windows `test/check.ps1 -SkipAot` 与 Linux C ASan/UBSan、线程、XLang
双语门禁通过。完整 Windows AOT 仍缺共享 `xl6ValueDataSnapshotV1`，
尚不签收整包发布级。

- XLang：`release/examples/extlibs/totp.xl`
- 同输出 C：`release/extlibs/totp/examples/totp_c.c`

两份范例除固定 RFC 时间戳向量外，还调用实时 `generate/verify`；只输出
验证是否成功，不打印随时间变化的验证码。默认窗口允许相邻时间步，避免
范例恰在步长边界运行时出现假失败。

在 `demo6/release` 可直接运行 `xl run extlibs/totp/examples/totp_c.c`；
普通 C 编译仍按包清单单独链接 `totp_native.c`。
- 边界：包内 `test/test_totp.c`、`test/test_threads.c`、`test/boundaries.xl`，以及 `demo6/test/cases/extlib_totp.xl`

在 `demo6/release` 运行 `xl run extlibs/totp/test/boundaries.xl`；在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package totp -Source release/extlibs/totp/test/test_totp.c`。边界覆盖 RFC 向量、长密钥 HMAC、精确时间步、Base32 非法填充、窗口和多线程独立结果。

在 `demo6` 目录还可运行 `release/xl.exe run release/extlibs/totp/test/allocations.xl`；两份包内 XLang 用例均登记在 `coverage.tests`。Linux 运行 `bash release/extlibs/totp/test/check.sh`，以严格 GCC 和 ASan/UBSan 重建 C 边界与线程测试，检查 manifest 模块宏，并运行中央及包内 XLang 用例、逐字节核对 C/XLang 范例输出。2026-10-14 新增 C 输出分配故障、XLang 三个生成入口的内存异常和失败后恢复测试；算法没有更改。

Windows 可从 `demo6` 目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/totp/test/check.ps1`：严格 GCC 宏检查、C 边界和线程、中央及包内 XLang 用例，以及包运行器 C、单文件 C、XLang 范例分别与固定 RFC 输出比较。默认还构建运行中央核心与完整范例 AOT，并对照同一固定 RFC 输出；`-SkipAot` 只运行 C/JIT 快测。Linux 专属的系统时钟故障注入仍由 `test/check.sh` 检查。

2026-10-14 补 C 公开入口的失败后成功/不匹配连续调用回归，修复旧线程错误残留；Windows 严格 GCC、C/XLang 边界与 Linux ASan/UBSan 均通过，完整 AOT 阻断不变。

2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、C/XL 边界与线程测试、双语同输出范例及 `test/cases/extlib_totp.xl` 无输出核心 AOT。`xl build extlibs/totp/test/boundaries.xl -o ../build/test/totp_boundaries.exe` 仍因共享链接缺少 `xl6ValueDataSnapshotV1` 失败，完整 AOT 尚未验收；见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-19：新增 Windows 单入口及固定 RFC 范例输出门禁，当前整组通过；HMAC-SHA1 算法未改，Linux ASan/UBSan 本轮未重跑。完整带输出 AOT 的共享缺符号阻断不变，不签收整包发布级。

2026-10-20：补 256 字符 Base32 密钥、`int64` 最大时间戳、9 位验证码及窗口 10 的固定向量，期望值由独立 Python HMAC-SHA1 计算；C/XLang 同时拒绝 257 字符密钥。Windows 严格 C/线程/语言门禁与 Linux ASan/UBSan、语言对照门禁通过。算法未改，完整带输出 AOT 的共享缺符号阻断仍在。

同日 Windows 默认门禁补入中央核心和完整范例 AOT，产物只落在本次独占目录并于退出时清理。实测 C/JIT 快测与核心 AOT 通过；完整范例构建因共享 `xl6ValueDataSnapshotV1` 缺符号失败，AOT 输出对照尚未执行。完整发布形态仍未签收。
