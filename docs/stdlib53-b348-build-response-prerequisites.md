# DLL 构建响应文件与失败状态

系统、文本及容器结项审计发现当前工作区的普通构建前置冲突。本批按用户
授权整理 DLL 响应模板和调用助手，修复重复源码、启动失败误报成功与符号表
换行检查差异；不提交其他编译器或 IDE 增量，不改变标准库 API 或 XRT 快照。

## 源码只编入一次

`xl6_closure_capture_v3.c` 和 `xl6_nullable_slot_map_key_capture.c` 已由共享
对象编译命令提供，不能再作为直接源码加入两种 DLL 的响应模板。当前工作区
模板已删除这两个重复项；Windows runtime／compiler 和 Linux runtime／compiler
四种来源检查均为 423 项齐全且无重复，五项既有反向控制仍拒绝。

提交的两份模板按原 DLL 命令的参数顺序机械拆分，完整参数等价，而不是重新
维护另一份来源列表。当前模板另含五个 IDE 相关源码单元的引用及排序调整，
这些内容留在工作区，不纳入本批提交。批处理还保留资源校验后立即检查退出码
的修正，避免后续生成器覆盖失败状态。

## 原生退出码与响应协议

PowerShell 调用助手将环境变量展开到无 BOM 的 UTF-8 响应文件，用单一
`@response` 参数启动编译器，保留原生 stderr 和退出码。编译器没有成功启动
时，不能将空的 `LASTEXITCODE` 当作成功；现在先保持失败状态，只有实际
原生进程提供退出码后才采用该状态。

旧助手已复现“可执行文件不存在，但返回 0”。修正后的工作区模板与准备
提交的等价模板分别通过八项隔离协议测试，包括带空格路径、精确响应字节、
成功但带 stderr、退出码 29、缺失可执行文件和未解析环境变量。测试探针只是
读取响应文件并返回指定状态，不是 GCC，也不作为 DLL 链接或运行证明。

```text
python tools/check_stdlib_dll_response_helper.py --root <demo6> --out <fresh-directory>
```

## 符号表换行检查

PowerShell 的 `-Check` 现在将 CRLF 规范为 LF 后比较完整生成文本，接受这
两种等价换行；改名、重复符号和单独 CR 的三项反例仍拒绝。五项隔离检查
通过，当前 3,674 项符号表通过真实工作区检查。声明头和快照元数据仍按实际
字节与 SHA-256 校验，没有同步外部 XRT 或放宽这些资产的字节合同。

```text
python tools/check_stdlib_symbol_table_line_endings.py --root <demo6> --out <fresh-directory>
```

协议报告为 `D:/xlang-validation/stdlib53-b348-helper-fixed/results.json` 和
`D:/xlang-validation/stdlib53-b348-helper-selected/results.json`，符号表报告为
`D:/xlang-validation/stdlib53-b348-symbol-line-endings/results.json`，来源检查为
`D:/xlang-validation/stdlib53-b348-dev/shipping-sources.fixed.json`。

## 新产品与结项边界

当前工作区的独立整合候选包含未提交的编译器增量，只用于核对共存行为，
不扩大本批提交范围。普通构建、随后产品运行和 53 模块逐项结项分别验收；
旧产品的通过记录不能自动证明新候选。

两端新候选的普通构建均已通过，构建凭据为
`D:/xlang-validation/stdlib53-b348r3-build-win/build.shipping.json` 和
`/var/tmp/xlang-stdlib53-b348r4-build-linux/build.shipping.json`。Linux 导出前仅对
两份已知生成表准备规范 LF，并分别记录原始和导出哈希；此前输入换行变化
的失败凭据保留，不改判为通过。没有复用旧 DLL、SO、CLI、XRT 对象或 VFS。

批量产品回归另发现工作区 IDE 整合缺口：公共源码驱动已加入五个新编译单元，
但 `xlAddEmbeddedXlangRuntimeSources` 的内嵌源码清单尚未同步，嵌套编译专项
拒绝通过。Windows 的 328 项语言清单中，可执行项已通过，宿主／平台要求与
负例保留原分类；其余产品专项和独立 AOT 仍分别验收。这个编译器增量不在本批
提交范围，不能删去专项或据此宣称当前完整工作区已发布验收通过。

Windows DST 按用户决定独立保留
待验收，不作为本轮环境阻塞，也不计作通过。CRT 无未决策项，XRT 其余
19 组未决策和对象池最后的顺序不变。
