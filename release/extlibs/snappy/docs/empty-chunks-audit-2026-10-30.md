# Snappy 标准空块审查（2026-10-30）

## 缺陷及改动

[Google framing 规范](https://github.com/google/snappy/blob/main/framing_format.txt)
4.2/4.3 只限制原文上界，不要求正文非空；CRC 仍为四字节。
独立 python-snappy 0.7.3 / cramjam 2.14.0 也接受两种空块：
未压缩块长 4，压缩块长 5（CRC 后是零原文长度 varint）。

旧 `xrt_snappy.c` 统一要求块长至少 5，并单独拒绝 declared == 0。
现在按块类型使用最小长度，移除非标准的零原文禁令。压缩空块不再
申请临时解压区，但仍进入原 raw 解码器和 CRC 检查；缺 varint、
零长度后带非法指令、CRC 错、截断仍拒绝。输出 Buffer/Carry 原有
预留容量不变，不宣称空块处理零分配。编码器和所有公开 API/ABI、
资源配置、所有权、生产依赖不变，vendor/冻结 XRT/主线未修改。

这是补齐已有标准解码合同，不是新格式、兼容回退或跳过空块校验。
没有测吞吐，也不根据几行代码变化宣称端到端更快或更小。

## 证据

- 最终 C 回归 379 次：八种空块/前后正文/重复成员/padding、一次性
  和 Reader/Writer 包装、每个二分切点及前段 drain；检查完整正文、
  members、inputBytes/outputBytes、finished。CRC 错/缺 varint/
  假零长度指令/0–3 短块/逐前缀截断均拒绝。
- 精确输入预算、输出/item 预算和成员上限未被空块绕过。两个空帧的
  10 个真实 XRT OOM 点检查分类、空结果、活动数/字节/非法释放/
  双重释放/上下溢/UAF，并立即重试；不控制 vendor 的 malloc。
- 同一最终夹具，Windows TCC 与 Linux sanitizer 下旧源各 352 条
  断言失败，新源各 0；Linux 仅替换旧 core 对象，其余对象相同。
  这是一个标准格式缺陷的多条断言，不算 352 个独立 BUG。
- 新 XLang 空帧同时走 Buffer/cstring 一次性和逐字节流式入口；
  C/XL 新范例同一 59 字节流、三行 golden，GCC C、TCC C JIT/AOT、
  XL JIT/AOT 均过。没有用字符串/Buffer 二次包装代替文本接管。
- 实际 cgen 的 26 个 xsnappy 声明与 C 公共头同编；故意将 version
  返回改成 int 必须被 GCC 拒绝。最初中央 cgen 只覆盖 22 个，测试
  明确失败；补专门 abi_surface 使用两种 update 与全部计数 getter，
  没有放宽期望集合来伪造完整 ABI 覆盖。
- 两平台使用相同固定版本的独立实现：原四种载荷双向 framed 互通，
  新八组固定空块流先独立解码，再验证 C 输出与退出码。Windows
  cp312 与 Linux cp314 wheels 是测试资源，有 SHA256，不进入发布包。

## 门禁及未闭环边界

Windows 最终 69 阶段前 68 项符合各自预期（包括两项私有方法拒绝），
64 个输入/工具指纹不变，最后原顶层生命周期专项仍 M2。Linux 原生
快照的 C ASan/UBSan/泄漏、旧/新 C 边界、实际 ABI、双语及 13 条
正常独立程序路径先通过，随后同一 M2 退出。两平台的顶层正例仍
要求 0，不把打印 PASS 后退出 1 当作成功，不搬入 main 避开。

61 份来源/复制输入、三个 Linux 工具和独立 oracle 文件前后不变；
Windows 69 阶段属于固定当前发布物，不是最新 master。八个冻结
二进制/单头不变。后续只改说明和官网源，不改已执行的生产实现、
测试或门禁。输入工具哈希不变只证明本次现场，不自动关闭历史风险。

新 Windows Run 用有界子进程，C 包运行器不再在当前 PowerShell
进程内借用 LASTEXITCODE；失败保留真实状态/输出/命令和指纹。
PS5/PS7 上实际 Run 的 null、空参数表、显式空实参三义正确；
Win 原始 37 导致门禁 1、Linux 37/缺失工具 127 都失败留档。

本轮不证明 32 位、TSan、长时并发、持续 fuzz、上游 allocator 全点
或所有文件事务异常已覆盖；Reader/Writer 首因及文件回滚可继续审查。
当前损坏/截断通过 native checked 边界实测为 IOError，不能仅依据
未到达的包装层 fallback 声称必为 CompressionError。此处只统一
文档事实，没有擅自改公共异常分派；MemoryError 的既有测试保留。

官网三个语言代码块也按原文提取，两平台分别 JIT/build/AOT 验证：
快速开始与修正后的压缩率片段均正常；顶层流对象片段仍只在 AOT
退役失败，保留原文并明确 M2。压缩率片段补 import、使用当前 len，
移除旧的 202 字节硬编码（本次实际为 196，不把它承诺为固定线长）。
另外以真实坏 CRC 源核对 IOError 及其诊断；没有把文字替换当作修改
语言异常模型。Windows oracle 模块也另做前后哈希及互通复跑。

## 现场

- `build/test/snappy_empty_audit_1793368310/`：旧/新 C 控制、wheel、
  独立互通、进程反证、冻结指纹、源码和测试记录。
- Windows `build/test/snappy_guard/run_b05171ffd9864dc09a15599984563418.report.json`，
  M2 子记录 `build/test/native_globals_55fb5ac50dff494d8fa8cb14e3924315/results.json`。
- Linux `/var/tmp/xlang-snappy-empty-audit-Sn5zY9Ia`，门禁现场
  `tmp/xlang-snappy-52hw0G`，M2 子现场 `tmp/xlang-native-globals-ZGFrI8`。
- 默认包门禁仍红，Snappy 仍 M2；100 包 G30/R7/M59/X4 不变。
