# BSON 原子保存：发布错误叠加 OOM 遗留临时文件

## 2026-11-01 当前主目录确认

Windows x64 的现行 0.6.0 发布物、以及使用同一 XRT 头独立构建的 GCC
C 程序均可复现。本任务不修改编译器、XRT 快照或 XRT 源仓库，不在
BSON 中添加重复的原子写入、CRT 删除或故障重试实现。

普通新建/覆盖保存的 25 组检查、23 次真实分配失败全部通过。进一步将
**真实的 Windows 共享拒绝发布**与一次性分配失败组合后，发现一个
此前的“旧目标不变 + 内存平衡”断言没有覆盖的磁盘清理问题。

## 最小入口

从仓库根目录运行：

```powershell
./release/xl.exe run release/extlibs/bson/test/save_cleanup_c.c -- --locked
```

用纯 GCC/XRT 对照，完全不编译或调用 BSON 编码器，也不需要 xlang DLL：

```powershell
New-Item -ItemType Directory -Force build/test | Out-Null
gcc -std=c11 -O2 -Wall -Wextra -Werror -DBSON_TEST_XRT_ONLY -Ilib/xrt release/extlibs/bson/test/xrt_runtime.c release/extlibs/bson/test/save_cleanup_c.c -lws2_32 -ladvapi32 -lm -o build/test/bson-atomic-repro.exe
./build/test/bson-atomic-repro.exe --locked
```

编译期开关只选择同一测试中的直接 `xrtFileWriteAtomic` 调用，不增加
产品适配层。测试只使用自己排他创建的临时目录和 XRT 文件对象，不锁定
用户文件；Windows 不允许替换的句柄由同一测试进程创建并关闭。

## 精确结果

1. 先用不注入故障的保存操作确认目标的真实系统错误分类，且没有临时残留。
2. 对每一个分配预算重新建立隔离目录和内容为 `keep` 的 UTF-8 文件。
3. 以 `XFILE_SHARE_READ | XFILE_SHARE_WRITE` 打开目标，明确不共享 DELETE。
4. 在保存调用前启用当前线程的一次性 `xrtMemDebugFailAfter`。
5. 解除注入后检查返回值、原错误分类、旧目标内容、目录额外条目和七项内存指标。

| 路径 | 结果 |
| --- | --- |
| 当前 `xl run` 的真实 `xbson_save` | 14 组、13 次分配失败；预算 12 留一个 `.xrt-write-*.tmp`，退出 1。 |
| 独立 GCC 的真实 `xbson_save` | 同样 14 组、预算 12 留一个临时文件，退出 1。 |
| 独立 GCC 的直接 XRT 原子写入 | 12 组、11 次分配失败；预算 10 留一个临时文件，退出 1。 |
| 普通保存的独立对照 | 新建/覆盖共 25 组、23 次分配失败，无残留，全部通过。 |

残留点仍返回失败、保留无故障对照中的发布错误，旧目标保持 `keep`；
活动分配数量/字节和非法释放等指标都平衡。这不是数据覆盖、堆损坏或
BSON 线格式错误；问题是已经写完并关闭的临时文件未被回滚删除。
测试失败时保留自己的目录，路径在最后一行打印；成功才删除自己的目录。

`--locked` 的共享拒绝语义是 Windows 专项，非 Windows 返回 77，不能
当作 Linux 通过。Linux 普通分配故障与包级 sanitizer/JIT/AOT 另行验证，
不拿正常路径代替此组合边界。

## 根因与修复建议

当前单头的 `xrtFileWriteAtomic` 在刷新、关闭之后才提交路径替换。
`__xrtFileCleanup(NULL, temporary, true)` 回滚时重新调用按 UTF-8 路径删除；
Windows 路径转换仍需要分配。因此发布先因共享限制失败，随后回滚转换
再遇 OOM 时，原错误被正确恢复，但删除动作没有完成。

应在 XRT 文件原子事务实现中统一修复：临时文件仍打开时准备提交/清理
所需资源；未准备完成的失败使用打开句柄删除，已关闭后的失败使用预先
准备、无需分配的原生路径或回滚能力。保留原始发布错误和旧目标，不靠
吞错、重复尝试、ANSI 文件函数或包内复制实现绕过。

受控同步稳定 XRT 之后，先重跑上面三个入口，再跑 BSON 两平台完整门禁。
Windows 默认包门禁将该用例作为必须成功的正例，不把实际退出 1 改为
预期失败。源码、完整输出和摘要见 `docs/reports/extlib-bson-20261101.json`；
阶段详情保留在 `build/test/bson_current_20261101/`。
