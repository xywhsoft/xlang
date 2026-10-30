# CLI：os.args 的运行时符号缺失

2026-10-28 在扩展库工作树的既有发布物上复验。编译器源码基线为
`d64672903`，扩展库整理起点为 `bb7c6b45e`；不等同于尚未合入并
重建的最新 master。二进制哈希见
[验收矩阵](../../docs/普通扩展库发布验收矩阵_2026-10-28.md)。

## 最小复现

[cli_os_args_symbol.xl](cli_os_args_symbol.xl) 不导入任何扩展库，
只在 main 中调用标准库 `os.args()`。从仓库根目录执行：

```powershell
.\release\xl.exe check dev/extlib_blockers/cli_os_args_symbol.xl
.\release\xl.exe run dev/extlib_blockers/cli_os_args_symbol.xl
.\release\xl.exe build dev/extlib_blockers/cli_os_args_symbol.xl -o build/test/cli_os_args_symbol.exe
wsl ./release/xl run dev/extlib_blockers/cli_os_args_symbol.xl
```

Windows `check` 返回 0 并输出 `ok`；Windows run/build 与 Linux run
均未进入脚本，报告 `unresolved reference to 'xl6_xrt_os_args'`。
本轮未单独运行 Linux build 或 DLL 参数隔离测试。
`test/cases/extlib_cli.xl` 也在同一缺符号处失败，不能据此判断
CLI 原生参数解析器出错。

## 排查与验收边界

`src/stdlib/xl6_stdlib_data.c:4053` 将内部 `osArgs` 注册到 C 符号
`xl6_xrt_os_args`。主线应核对该入口的运行时实现、需要项收集、
内嵌 TCC 符号注入和独立 EXE 发射是否一致；只在包内重新实现
`parseOs` 或删除其公开能力不是修复。

参数应遵循已有上下文/入口参数合同：内嵌运行不能悄悄改读宿主
进程的全局 argv，结果数组的所有权也必须明确。修复后先验证
无包最小源的 JIT/EXE，再跑 CLI 双平台默认门禁、C/XLang 输出，
并用两份不同参数的 DLL 上下文检查隔离与生命周期。本任务不改
编译器或运行时实现，也不增加兼容回退。

## 2026-10-30 复核

默认包门禁两平台各39阶段；中央、basic/options公开例和此无包源的
JIT/build八项均仍缺 `xl6_xrt_os_args`，check/cgen正常。现在实际验证了
Linux build 同样失败，不能再沿用“本轮未测 Linux build”的旧范围。
无包源从当前仓库根目录运行，无需旧 demo6 前缀。

另一个独立边界不能被本符号修复掩盖：`cli_args_projection.xl` 的同名
C伴随源只提供真实xcli_parse，不导入含parseOs的模块。两平台均构建成功，
JIT/EXE均在数组/对象类型检查报错；见共享foreign容器投影记录。该源只用于
隔离诊断，不作为用户绕过import、重建私有容器或替代parseOs的方案。
先修本符号/上下文参数，再修M1投影，最后完整复跑包级门禁和双语输出。
