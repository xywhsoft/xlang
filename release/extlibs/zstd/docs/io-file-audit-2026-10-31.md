# Zstd IO 与文件事务复核

2026-10-31。根目录发布矩阵指定的冻结编译器/XRT；没有同步最新主线，
没有修改 vendor、压缩格式、公开 ABI 或生产分配策略。本包仍为 M，
默认完整门禁保留两个独立边界的失败，不据正文正确签收发布质量。

## 修复及成本

四处 ReaderRead/WriterWrite 失败原将固定 C 错误结构填为 IO/后端0/
通用消息，丢失 XRT 原始分类和诊断；真实 BufferWriter 扩容 OOM
也被误报。新增固定结构填充函数，按现有 XRT cause 映射
ARGUMENT/MEMORY/LIMIT/STATE/CANCELLED，其余归 IO；原 code、消息
及累计输入/输出偏移保留，不替换原 XRT 错误对象。
固定消息字段仍只有256字节，长消息可能截断，完整 cause 仍在 XRT。
此分支不新建错误对象；正常编解码路径不增加分配、复制或扫描。

文件处理在失败清理前保留打开事务的借用 xfile，先 DeleteOpen 再按
原所有权关闭 Writer/TempFile。避免 Windows 关闭后按 UTF-8 路径
删除需要额外 UTF-16 分配，故障再次发生时遗留临时文件。成功路径
仍是原来的 flush/close/rename，没有新增路径副本、额外分配或提前提交。
此修复只涵盖事务仍打开时已经失败的路径，不涵盖关闭或 rename 后失败。

## 同夹具红绿验证

| 检查 | Windows 旧→新 | Linux 旧→新 |
| --- | --- | --- |
| IO 30场景 | 32条断言失败→0 | ASan/UBSan 32→0 |
| 打开事务96组 | 8组失败→0 | 0→0，同例未现 Windows 路径分配 |
| 关闭事务16组 | 2组失败→2 | ASan/UBSan 0→0 |

IO 使用四条公开 Reader/Writer 路径乘七种错误情形，另有两个真实
BufferWriter 分配失败、故障解除后的往返。检查原错误指针、分类、
domain/code/message、偏移和流所有权；各场景核对七项 XRT 内存快照。
使用共享 `test/support/extlib_codec_io_errors.h`，通过编译期映射
选用本包 API，不引入运行时包装。abc raw block 标准帧另由 Python
Zstandard 0.25.0 解码为 abc，不以同一编码器产物自证。

文件夹具调用正式 native 文件 ABI 和真实 XRT callable；两个方向、
首/末真实回调、原目标存在/不存在、12个分配预算共96组。旧 Windows
发生16次真实 OOM、8组留临时文件；新8次真实 OOM、0失败，去掉多余
删除路径分配会减少触发点，不把未触发当故障已覆盖。保留首错、旧目标、
零临时文件及七项内存平衡。关闭态使用目录目标使 rename 自然失败，
在最后一次成功回调安排故障，不伪造 FINAL 标志或替换文件 IO。

## 完整门禁的实际结论

- Windows `check.ps1` 82阶段、80符合预期、67输入指纹不变；两条
  private updateNative 负例要求 raw1和指定诊断，不接受任意失败。
- Linux 默认门禁完成原生 ASan/UBSan/泄漏检查、全部普通 C/XL
  JIT/AOT；复制前/后及源目录结束前68来源、gcc/cc1/python三工具
  SHA256一致。默认最终退出1不是整包绿灯。
- 两平台各17条普通独立程序成功运行，包括新增三项 C 边界、两个
  文件事务范例，以及原中央/帧/串联/错误/进度/教学与二进制流例。
- 新 `examples/file_transaction.xl`、`file_transaction_c.c` 使用 UTF-8
  文件名，演示取消、业务异常、保留旧目标及重试。GCC C、TCC C
  JIT/AOT、XL JIT/AOT 两平台逐字匹配同一三行黄金输出并清理目录。
- 原后端 customMem 9路径/18真实失败点、分配/文件/文本、损坏帧、
  串联帧和限额继续通过；XRT 内存快照不代表覆盖 vendor 的 CRT 分配。
  native 源 `-Wmissing-prototypes`、两个公开 xstrview 入口以含 NUL/
  中文输入在 GCC/TCC JIT/AOT 验证。20条实际生成原型校验仍是10-30
  记录，本轮没有把此数字重复算作新增或全量 ABI 复验。
- Python Zstandard 0.25.0 与正式 C 核心在两平台进行四载荷双向互通，
  隔离 oracle 指纹不变。Python只为测试依赖。互通 Python 子进程30秒
  超时、成功时出现 stderr 也拒绝；Windows 可选脚本自身也改为有界
  进程、保存原始状态及输入指纹、失败不删现场，并已实跑通过。
- Windows PS5/PS7 实际 Run 的 null/空/单空字符串参数、预期37、
  非预期37和缺程序均验证；假 GCC raw37使真实默认门禁外层退出1，
  报告保留37及原诊断。控制测试不冒充产品复现。

Windows 关闭态两方向预算3仍各遗留一个临时文件，旧目标、首错与
内存平衡保持；Linux同16组只有2次真实 OOM、0失败。详
`dev/extlib_blockers/zstd_closed_transaction_oom_cleanup.md`。
M2在两平台仍是全局流对象 JIT0、AOT打印 PASS后退出1；不把原正例
挪进 main、不提前销毁、不吞异常。32位、TSan、长时压力、持续模糊
及全部 IO 故障点不在本轮证明范围。

## 证据

- `build/test/zstd_io_audit_1793378359/`：旧对象、原始结果、独立
  oracle、控制验证及官网隔离生成记录。
- Windows `build/test/zstd_guard/run_87ac5b3bab5045818f67acdb35627654.report.json`；
  M2 `build/test/native_globals_c1c3569aa85f4e088daa6d20b75a22e5/`。
- Linux `/var/tmp/xlang-zstd-io-audit-kX3awIB1`；完整门禁
  `tmp/xlang-zstd-WADf1V`，退出码、trace、逐步结果和指纹均保留。
- 可选互通门禁在完整包门禁结束后单独更新/实跑；其后文档及官网源
  更新不描述为再次执行整包门禁。可选脚本的假GCC raw37也确认外层1、
  原诊断和结构化报告保留。最终冻结8份发布物/单头库已核对不变；
  C/头/XL源码仍与Windows完整门禁的输入指纹相同。
- 官网隔离生成页与包内详情源SHA256相等；三个原文片段在两平台
  JIT/AOT均退出0，无stderr，不把全局Buffer快例或函数内流例当成
  M2顶层类对象退役验证。Linux官网快照
  `/var/tmp/xlang-zstd-site-audit-MTf4d3NV` 的59输入前后一致；
  Windows生成与九个进程记录在上述审计目录 `site.json`。
  没有向外部官网部署。
