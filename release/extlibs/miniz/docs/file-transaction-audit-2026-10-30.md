# miniz 文件事务与标准 zlib 审查

审查于 2026-10-30 开始，2026-10-31 跨日收口；证据按起始日期命名。

## 实际修复

只改本包 native 文件失败清理。原实现先关闭临时文件，再按 UTF-8
路径删除；Windows 的 UTF-16 路径分配遇 OOM 会留下临时文件。
现借用已打开的句柄，在失败时先 `xrtFileDeleteOpen`，再由原来的
Writer/TempFile 关闭；已删除不再走路径删除，未成功才保留原回退。

正常操作仍在处理、flush/close 全成功后重命名，不提前提交，也不
改变错误首因或文件覆盖语义。成功路径只多一个借用句柄和布尔量，
不加堆分配、正文复制、运行时封装或生产依赖。API/ABI 与标准 zlib
格式不变；无性能基准，不声称吞吐提升或整个操作零分配。

主线、TCC 与冻结 XRT 不改，不同步尚在开发中的 XRT。

## 双故障与共享测试

先以真实 miniz 文件入口、XRT callable 和分配注入复现，再做修改。
测试正文复用 `test/support/extlib_native_codec_file_rollback.h`，
本包适配仅为 7 行编译期绑定，实际调用仍直达本包函数；没有替换
产品 IO 或分配器。它与带 FINAL 的普通 C 回调夹具分开，native
三个参数没有 FINAL，只通过成功操作的真实调用次数选择末次调用。

打开态 96 组 = 压缩/解压 × 首次/末次回调 × 新/旧目标 × 12 个
预算。检查返回值、回调次数、首错 domain/code/message、零临时
残留、旧目标和活动数/字节/非法释放/重复释放/上下溢/UAF 七项快照。

| 平台与控制 | 真实结果 |
| --- | --- |
| Windows 同一最终夹具 + 旧 native 对象 | 96 组、16 次真实 OOM、8 失败；预算 1 留临时文件。 |
| Windows 同一夹具 + 新 native 对象 | 96 组、8 次真实 OOM、0 失败；GCC C、TCC C JIT/AOT 均通过。 |
| Linux 原生 GCC ASan/UBSan/泄漏，新旧对象 | 各 96 组、8 次真实 OOM、0 失败；不能记成 Linux 新修了 8 项。 |

旧/新 Windows 对照只换 native 对象，其余最终夹具和构建对象相同。
新实现少走一次路径删除分配，所以 OOM 触发数变化，不是削减预算。
夹具抽取后的 LZMA 也在 Windows TCC 和 Linux GCC sanitizer 复核，
结果保持原来的打开态通过、Windows 关闭态失败；不声称重验 LZMA 全包。

关闭态 `--closed` 则正常完成流，用本测试创建的目录让提交自然失败，
在末次回调注入一次 OOM。两方向各扫 8 个预算，共 16 组：

- Windows 新旧 native 均 8 次真实 OOM、2 失败；预算 3 留一个临时
  文件，原目录、首错和内存快照仍正确，退出 1。
- Linux 新旧均 2 次真实 OOM、0 失败，退出 0；没有这次 Windows
  转码删除的分配路径，不宣称 Linux 也触发了相同故障点。

这使 RFS 有 **8 个逐包实测包**，不按相似代码推测数量。需要稳定
XRT 的创建时预备事务凭据，使关闭后 Abort 也无须临时分配。包内不
复制 Windows 文件 IO、不重复重试删除、不吞错、不改变提交顺序。

既有 M2 独立保留：两平台全局压缩器 JIT 0，AOT 打印 PASS 后
报状态错误、退出 1。默认门禁两条边界都执行并要求成功，不把现有
失败配置成预期成功。本包仍 M，不能作完整发布签收。

## 独立互通与范例

新增 C 标准输入/输出工具及 Python 标准库 zlib 差分。7 份载荷包含
空、UTF-8/NUL、全字节、重复和固定随机数据；不要求压缩结果相同，
而由外部解码器检查完整明文、eof 与无尾部垃圾。

- 0..10 共 77 组本包编码，Python 解码全部正确。
- 56 组 Python 存储/固定/动态 Huffman、RLE、Huffman-only、不同
  级别与窗口输入由本包正确解码。
- 70 组坏头/校验/截断/尾随/串联/raw/gzip/预设字典输入拒绝；本包
  当前 API 不提供 preset dictionary，也不把 gzip/raw 当 zlib 读。
- 坏流必须 raw1、指定 miniz 诊断、DATA/TRUNCATED 代码且零 stdout；
  OOM、崩溃、超时、无关错误都不是正确拒绝。所有检查在 `-I -O`
  下仍执行，单个子进程 30 秒限时。最终更严格判据在完整门禁之后
  两平台各专项重跑；不将此专项冒称再次完整门禁。
- 8 个验证器反向控制还检验错误明文、半输出、OOM 假阳性及超时。
  模拟只用于验证器，不混为产品故障注入或真实 OS 故障证据。

新增 `examples/file_transaction.xl` / `file_transaction_c.c`：独占目录
和 UTF-8 文件名，取消/回调错误保留旧目标，随后重试并恢复原文。
两平台各 GCC C、TCC C JIT/AOT、XL JIT/AOT 五路径与三行黄金输出
一致。C 文件例使用现有 native ABI 的 XRT callable；普通 C 流
仍用 `xrtprogressproc`，不新增公共 C 文件 API。

## 门禁与边界

Windows 默认门禁 76 阶段，74 项符合要求，最后关闭态 RFS 与 M2
为真实失败；55 项输入/工具指纹运行前后不变。Linux 最终原生快照
完成严格 C、ASan/UBSan/泄漏、所有语言与双语、各 14 个普通独立
程序路径、互通，最后只有 M2；60 个复制来源及原件、GCC/cc1/
Python 三工具运行前后不变。首次 Linux 门禁实际 13 条普通 AOT，
总结误标 14；补入 C 基础例 AOT 后用新快照完整重跑，最终才为 14。

Windows 进程改用共享有界 helper，Run-C 以子脚本运行，不取旧的
LASTEXITCODE；失败保留原始状态、退出码、stdout/stderr 和产物。
两平台所有失败路径也复核输入指纹；权限负例必须 raw1 及指定诊断。
PS5/PS7 的真实 Run 已验证 null、空参数表、单空字符串、raw37 和
缺失程序，真实门禁拒绝假 GCC raw37，外层返回 1 且保存原始 37。

八份冻结发布物/单头库不变。README/范例说明/官网源更新，仅隔离
生成官网详情页，不部署到并行维护的外部站点。100 包仍 G30/R7/M59/X4。
官网三份原文代码片段在两平台实跑：快速开始 JIT/AOT 都为 0，
两份顶层对象例 JIT 为 0、AOT 为 1 且命中 M2，未移入 main 绕开。
隔离生成的详情页与包内源 SHA256 相等，不代表外部网站已更新。
32 位、TSan、长时压力/模糊、所有关闭失败与断电持久性不由本轮证明。

## 现场

- `build/test/miniz_transaction_audit_1793374784/`：新旧对象、原始结果、
  进程与验证器控制、网站与哈希证据。
- Windows：`build/test/miniz_guard/run_dcb48402f38a458bb545460faabefe2d.report.json`；
  M2 `build/test/native_globals_22fc48526ddb4686ab97f02f091000cd/`。
- Linux 最终：`/var/tmp/xlang-miniz-transaction-52NZTJGU`，门禁
  `tmp/xlang-miniz-zmOMCK`，原始 trace/输出/前后 SHA256 与新旧对象
  对照均保留。最终互通专项在其 `oracle-final/`，未修改原快照输入。
- 早期 PowerShell 探针误给自动 `$args` 加类型导致解析失败，纠正为
  独立命令变量后执行；不把这一探针错误计为库 BUG。
