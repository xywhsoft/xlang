# Gzip IO 与文件事务复核

2026-10-31，根目录发布矩阵的固定编译器和XRT。未改主线、XRT、
压缩算法、公开ABI或成功路径顺序。本包仍M，默认完整门禁保持失败。

## 已修复与未误判的边界

文件处理失败后原先关闭事务再按UTF-8路径删除，Windows二次OOM时
遗留临时文件。现借用打开xfile，先DeleteOpen，再沿原所有权关闭；
成功仍flush/close/rename，不新增分配、路径副本或提前提交。

同一正式文件ABI、真实XRT callable夹具共96组：压缩/解压、首/末
实际回调、旧/新目标及12预算。Windows旧16次实际OOM/8组残留，
新8次实际OOM/0失败；删除分配被消除后故障次数减少，不把未触发
当作已验故障点。逐项核对原业务cause的domain/code/message、
旧目标、零临时文件和七项内存快照。Linux原生sanitizer旧新同例0失败。

IO未确认新产品BUG。原共享夹具在再次调用finish后才比较第一次
失败cause，gzip会发布新的公开API诊断，造成38条假阳性断言。
现先检查本次失败，再单独检查终态拒绝和对象内错误快照不变。
四条Reader/Writer路径乘七种错误，加两次真实BufferWriter扩容OOM/
解除故障后的恢复，共30场景：未改gzip核心即在两平台全过。
原指针/分类/domain/code/message、偏移、借用流关闭次数和七项
内存均检查。独立Python gzip生成abc固定帧，不用本包编码器自证。

同一共享检查的Snappy/Zstd聚焦复验：Windows严格GCC/TCC及Linux
ASan/UBSan仍各30场景0失败，旧核心仍各32条断言失败，所用对象
SHA256不变；不把这项共享夹具复核说成两包整包重验。
共享文件夹具的模块开关加ifndef，允许严格GCC宿主从manifest选模块，
没有改变测试操作或产品模块清单。

## 门禁与交付

- Windows完整78阶段、76符合预期、65输入当时无变化；三项负例
  （private两条、错误ABI一条）均要求raw1及指定诊断。最终另两项
  closed rollback/M2仍失败，非整包绿灯。随后仅补C新例首次include前
  的模块选择，最终源GCC/TCC JIT/AOT另作聚焦验证；未重复声称全套重验。
- Linux最终源完整门禁含严格C、ASan/UBSan/泄漏、63来源及gcc/cc1/
  python三工具前后指纹不变；普通路径完成后在M2退出1。
- 两平台各13条普通独立程序运行：原中央、进度、错误、文件、分配、
  教学、双语基础例，加新增IO/回滚C与两个文件事务范例。不是把
  构建/运行阶段重复计作程序数，也不计失败的全局对象AOT。
- 四种native头包含顺序、25条实际生成C原型及故意错改返回类型的
  编译负例两平台通过。后者不能只凭符号名相同签收ABI。
- 新 `examples/file_transaction.xl`、`file_transaction_c.c` 演示UTF-8
  文件名、取消、业务异常、旧目标保护及重试，GCC C/TCC C JIT/AOT/
  XL JIT/AOT同三行golden，结束时清理独占目录。C沿用正式native文件
  ABI的XRT callable，不冒充普通C进度函数；核心流支持普通C回调。
- Python/zlib独立139项在两平台普通及 `-O` 模式均过；正文比较改为
  显式检查，不能被优化模式删除。驱动将格式/截断与OOM/IO/关闭失败
  分码，Python只接受预期0或1，并核对stdout/stderr，拒绝崩溃或任意
  非零退出。每个子进程30秒，门禁总阶段有界。
- 验证器普通/优化模式的12组结果控制通过，PS5/PS7实际Check的
  null/空/单空串、预期37、非预期37及缺程序通过；假GCC raw37使
  真实默认门禁外层1，保留原始37、诊断和结构化结果。

关闭态16组Windows两方向预算3仍各留临时文件，Linux同例0；
`dev/extlib_blockers/gzip_closed_transaction_oom_cleanup.md`记录责任和恢复。
两平台全局流对象JIT0、独立程序打印PASS后退出1，M2不关闭，
不移动原正例、不提前销毁或吞错。32位、TSan、长时压力、断电持久性、
全部IO故障点不在本轮证明范围；短门禁不等于全平台生产认证。

## 现场

- `build/test/gzip_io_audit_20261031/`：旧native、初测/最终结果、
  Windows完整 `windows-final/results.json`、旧红/新绿、控制与官网结果。
- Windows M2：`build/test/native_globals_5cd58c3b79424ff698e157eb11f77d42/`。
- Linux最终 `/var/tmp/xlang-gzip-io-audit-i64d0MRX`，完整门禁 `work/`，
  trace、指纹、退出码、旧native回滚和优化互通结果均保留。
- 两个早期尝试在新增C夹具的宏重定义/首次模块选择处失败，已保存，
  不算完整验证，也未据此修改任何库功能或放宽Werror。
- 文档/官网源在完整验证后补齐；8个固定发布物/单头库已最终核对不变。
- 官网仅隔离生成，详情副本与包内源SHA256相等。两个原文代码片段
  在Windows/Linux实跑：顶层流例JIT0/AOT1命中原M2，新的完整文件例
  JIT/AOT均0，不搬动原顶层对象；Linux官网快照
  `/var/tmp/xlang-gzip-site-audit-qeNbckR8` 的46输入前后一致。
  Windows生成和六个进程在审计目录 `site.json`，未部署外部官网。
