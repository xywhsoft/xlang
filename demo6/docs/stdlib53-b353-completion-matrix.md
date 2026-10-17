# 标准库完成要求与证据矩阵

后续状态：Buffer 缺口已由[实际产品原生专项](stdlib53-b354-buffer-product-suites.md)
补齐，下面保留 B353 审计时的范围和结论。

批次 B353，2026-10-17。三组任务的 53 个模块现有统一的要求与证据对应关系：
49 项行为合同、84 份断言夹具、166 处具体检查片段。双端证据关联核对通过，
但这不是全部结项：Buffer 的三份原生故障专项仍需当前产品验收。

## 范围和定位

模块目的取自《三组标准库接入长期任务》的原定 53 行目标表，不按已通过的
测试名称反推范围。模块仍是系统与进程 12、文本 Unicode 正则 20、容器 21；
不把 WaitQueue 或某个内部子模块另算成新的命名空间。

| 定位 | 模块数 | 验收方式 |
| --- | --- | --- |
| 公开原生能力 | 34 | 对应语言功能、精确声明、行为断言与原生选择 |
| 类型安全格式替代 | 1 | typed 格式和插值，不公开 C 可变参数 printf ABI |
| 按需聚合 | 1 | Regex 子模块公开功能及各自选择 |
| typed 等价实现 | 11 | 复用 xruntime typed 存储、元素与拥有合同 |
| 内部依赖 | 3 | 公开消费者覆盖与正确依赖，不重复制造 API |
| 不作为语言存储 | 3 | 原生 raw 栈排除与编译反例，保留显式 C 互操作边界 |

`tools/stdlib53_completion_requirements.py` 保存每个模块的行为编号及具体断言。
系统 S01—S14 覆盖时间、环境、signal、进程、Future、文件能力和终端；
文本 T01—T20 覆盖精确字符串、数字、编码和 Pattern/Regex；容器 C01—C14
覆盖 typed 集合、拥有返回、Buffer 和并发队列；I01 记录内部及排除定位。
完整原生定位仍由 `stdlib53_pruning_contract.py` 独立校验，两个模块集合必须
一致，且都必须与原定目标表的 53 项一致。

## 证据分层

`tools/check_stdlib_completion_audit.py` 只读核对实际产物、日志和输入摘要：

- 84 份断言夹具必须具有相同源字节的 JIT／模块通过记录和可搬移独立 AOT
  通过记录，Windows 与 Linux 分别核对。仅声明的 Dict 配方夹具不作为运行
  行为证明；恢复合同关联实际 JSON／XLON 往返断言。
- 整体 328 项人口、专用宿主／平台条件、严格 C、48 个裁剪配置及 106 个
  O0／O2 原生对象保留原来的证明范围，未缩减为只验收这 84 份夹具。
- 各行为分别关联原生文本／系统、进程故障、拥有图、并发拓扑、终端等现有
  产品子门；不把 Stack／Tree／Queue 图门误写成 Buffer 原生故障证明。
- 当前文档使用 B351 的 33 项控制，错误声明使用 B352 的 426 次拒绝；两者
  必须固定到同一构建及产品摘要。不会把 B350 旧父报告改成已经执行新检查。

84 份夹具包含 166 处经源码检查的具体片段；这些片段用于追溯合同，不是运行
分支插桩。699 项签名关联仍作为独立的声明与可达调用证据，不冒称所有分支执行。
七项审计判定反例拒绝遗漏模块、遗漏断言、仅声明夹具、未执行夹具、不同源码、
旧版文档控制与缺失裁剪定位。

## 历史输入和当前版本

旧报告引用的两份说明／脚本已由后续批次更新，另有一份产品父门在冻结构建后
更新。审计从构建冻结源恢复前两份，从 B350 的 Git blob 恢复后一份，三个
版本的字节摘要均必须与原报告完全相同，并保留来源记录。

这不是忽略输入变化：当前文档使用新报告，旧检查使用原始字节；原报告、产物
和日志不改写。恢复仅允许已登记的 tools/docs 输入，不允许用旧生产代码或
替代提供方掩盖当前实现变化。

## 尚缺的验收

Buffer、buffer_hex、buffer_base64 的语言功能、声明拒绝、AOT 和裁剪已有
当前产品证据；其核心、值与迭代器、codec 的三份 C 故障专项目前仍是早期源码
专项。需要直接链接当前 DLL／SO，保留原有错误、范围、自引用事务、完整故障
前缀、跨线程和关闭重入断言，并检查真实资源账本。

对应文件为 `stdlib_buffer_core_host.c`、`stdlib_buffer_value_host.c` 和
`stdlib_buffer_codec_host.c`。矩阵在全局及这三个模块条目中明确保留
`buffer-native-suites` 未完成项；没有因此认定生产实现有缺陷，也不以普通
正例替代这项验收。Windows DST gap/fold 实测按用户决定另列待验收，不标为通过。

## 运行记录

```text
python tools/check_stdlib_completion_audit.py --root <frozen-demo6> --product-certificate <B350-native-certificate> --guide-certificate <B351-native-certificate> --abi-certificate <B352-native-certificate> --history-root <exact-historical-inputs> --out <new-directory>
```

最终报告为 `D:/xlang-validation/stdlib53-b353-audit-win-r3/results.json` 与
`/var/tmp/xlang-stdlib53-b353-audit-linux-r3/results.json`；Complete 表示本次
只读审计完成，Full53Acceptance 仍为 false。本批不重跑编译器矩阵、不更换
产品、不修改运行库实现。CRT 未决策为零，XRT 其余十九组及对象池最后不变。
