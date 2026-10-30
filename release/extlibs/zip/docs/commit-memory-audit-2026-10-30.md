# ZIP 内存提交与发布边界审查（2026-10-30）

## 生产修改

`xrt_zip_source.c` 的 `ZIP_SOURCE_WRITE` 在 Buffer 扩容失败时原本固定
上报 `ZIP_ER_WRITE/ENOMEM`。外层因此分类为 ZIP I/O，语言薄绑定将
真实的内存失败抛成 `ArchiveError`。

现在只在写失败分支读取当前 XRT 错误：内存故障映射 `ZIP_ER_MEMORY`，
其余写故障保持 `ZIP_ER_WRITE/EIO`。没有新增成功路径分配、复制、
缓存、API/ABI 或生产依赖；错误分支增加一次错误查询，不称整条桥接
零成本，也没有测吞吐。vendor、冻结 XRT、编译器和运行时均未修改。

联动审查了 BeginWrite、Seek/Tell、Commit/Rollback 与文件源：
Buffer Writer 的 Seek/Tell 不分配内存，也没有 Flush/Close 回调，
不机械扩散本次修改；文件源已有 MEMORY 分类，不改第二遍。

## 新测试与反证

- `test/test_commit_memory.c`：STORE/DEFLATE、直接 C/实际 xzip close、
  0/65472/65536/131072/262161 字节的固定不可压缩正文。逐预算扫描至
  普通成功，合计 100 次场景、80 个真实 XRT OOM；要求 ZIP 与 XRT
  均为 MEMORY、失败仍打开、解除故障可重试、完整字节/CRC 不变。
  每次检查活动数/字节、非法释放、双重释放、上下溢和 UAF。
- Windows TCC 与 Linux GCC sanitizer 下，同一最终夹具配旧内存源
  均有 30 条失败断言；新源均为 0。这是一个分类缺陷的多条失败断言，
  不是 30 个独立 BUG。Linux 旧/新只替换内存源对象，其余对象相同。
- `test/commit_memory.xl`：真正捕获 MemoryError，清除注入后重试并
  读取完整正文，JIT/AOT 均过。Linux 正确夹具配旧源仍误抛
  ArchiveError。最早旧 Windows XL 尝试误写不存在的 `addText`，
  该语义错误不是旧版本行为的证据，未计入红绿结果。
- 新双语范例同输入同四行输出；输入提前退役、读取结果跨关闭有效。
  五种执行路径产生的 ZIP 经 Python 独立核对。Python `-O` 下五份
  正例及同名/同注释但正文错误的负例也已验证，不依赖 assert。

OOM 扫描只针对这里的 XRT 分配和提交生命周期，不声称穷尽操作系统
刷新/关闭故障、所有第三方分配器或恶意并发目录。

另实测新 C 示例的显式 import 混编构建，两平台均完整输出四行并退出
0；官网 C 原文也在使用实际包 C/头搜索路径闭包后退出 0。最早官网
C 检查命令漏 include 目录，记录为测试配置错误而非库 BUG。只有
import 的 XL 文件作为 run 入口虽返回 0，却不输出 C main 的内容，
不算 C 示例运行成功；教学明确采用 build 后执行，不伪报空输出为通过。

## 固定发布物验收

Windows 默认最终门禁 182 阶段、202 个输入/工具指纹：前置运行能力、
32 个实际生成的桥接原型与 C 定义、错误返回类型必拒绝、原 C/XL
边界和双语例、独立格式互通、九条正常独立程序路径通过。四条红项
分别是顶层 AOT、描述 JIT、描述 AOT、私有构造器必拒绝，仍各自保持
原成功/拒绝要求；178 条符合各自预期不等于整包通过。

Linux 使用原生文件系统独占快照：C ASan/UBSan/泄漏、相同 100/80
提交测试、32 个生成原型及错改返回负例、Python/C 双向互通、新旧
范例与九条普通 AOT 通过。随后链接精确诊断、顶层 AOT、描述 JIT/AOT、
私有构造器验证器五项均失败。最后一项验证器返回 1 的原因是原始
`xl check` 实际返回 0，而正确要求为 1，不是编译器已拒绝。

快照 200 份来源/复制输入与三个工具前后哈希不变；Windows 门禁内
的 202 个输入/工具也未漂移。Windows 仅在最终 Linux 专用脚本完善
前的中间记录不作为最终门禁。后续更新只涉及 README、审查、范例
说明和官网源，不更改已执行的实现/脚本/夹具。八份冻结发布物/单头
保持原哈希。不是最新 master 的测试，也没有重建或同步主线。

门禁本身的反证：Windows 假 GCC 原始 37 导致外层 1，保留正确诊断；
Linux 原始 37 与缺失工具 127 也失败留档，不会吞成绿灯。初始 C 范例
漏声明模块已在 fixture 修正；没有增加生产依赖来掩盖测试头文件问题。

## 未闭环边界

1. M2：`test/global_lifecycle.xl` 和官网原文均 JIT 0，AOT 打印正确后
   state 错误、退出 1；两平台复现。不搬入 main、不提前清空全局。
2. M3：`test/private_memory_constructor_negative.xl` 仍被包外接受。
   只编译，不运行伪造指针。私有普通成员被拒绝不能替代构造器检查。
3. M12：`test/error_messages.xl` 两种普通错误均被 ArchiveError 捕获，
   但消息为 0/2；无包 C 对照正常、语言借用返回前清错的最小源在
   两平台 JIT/AOT 都失败。保留复制 OOM 的需要是合理的，清掉 C
   getter 需要读取的原错误则必须由主线解决，不给各包另加 TLS 副本。
4. RFS：当前仍路径式提交，不抵御目标目录被并发替换。Linux 链接
   专项这次只执行到第一条消息断言；独立 probe 证实该项已拒绝且
   链接外无文件，不能把诊断失败当成已发生越界写入，也不能声称
   剩余链接组合已重新通过。

修复建议与最小源在 `dev/extlib_blockers/no_raise_borrowed_error_text.md`、
`zip_private_memory_constructor.md`、`global_native_class_aot_fini.md`、
`zip_root_rename_sync.md`。本轮 ZIP 仍 M，100 包 G30/R7/M59/X4 不变。

## 现场索引

- `build/test/zip_commit_audit_1793364258/`：冻结指纹、旧/新 C 控制、
  正确 Linux 旧 XL 对照、借用 getter/官网原文、门禁和格式负例记录。
- Windows 最终 `build/test/zip_guard_77b92a54f8194de68a03e070756de133.report.json`，
  同名目录保留每条命令和产物；报告 `passed=false`。
- Linux 最终 `/var/tmp/xlang-zip-commit-audit-vTSMUrPs`，实际门禁目录
  `tmp/xlang-zip-LWdcv7`；日志、sanitizer 产物、TSV 红项、来源/副本/
  工具核对文件均保留。外层审计对已知红项作确认，不是包验收绿灯。
- 首次 Linux `/var/tmp/xlang-zip-commit-audit-UYUTDNh5` 保留第一条
  链接诊断失败和隔离 probe，不删除或用后一次结果覆盖它。
