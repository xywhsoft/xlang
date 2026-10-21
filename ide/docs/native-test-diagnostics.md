# 原生改名测试退出诊断

工程树改名的历史静默退出仍未查明。诊断工具记录新的运行，不修改历史失败结果，也不替代完整发布回归。

## 使用

在 `ide` 目录执行，Windows x64 Python 与仓库已有 GCC 可用即可：

```powershell
python tools/check_native_test_debug.py
```

该检查严格编译一个小型 C 夹具，分别验证正常退出、显式非零退出、可恢复异常、未处理访问异常、未处理断点、工作线程退出和超时。异常转储由独立解析器核对异常码、线程 ID、RIP/RSP，不能只以生成了文件作为通过条件。

单独调查工程树改名，使用一个新的输出目录和较短的项目夹具路径：

```powershell
$env:XIDE_TEST_TRACE='1'
python tools/native_test_debug.py --output build/rename-debug-new -- release/tree_rename_test.exe build/rd
Remove-Item Env:XIDE_TEST_TRACE
```

最后可追加已验证的 xlang 编译器路径，以实际构建并运行改名后的混合项目。夹具目录使用短路径；本次额外复现了长项目测试根下“无法创建编译器项目快照”的任务失败，该限制尚待修复。

完整回归可增加 `-NativeDebugTrace`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test.ps1 -Configuration Release -NativeDebugTrace
```

该选项先验证诊断器的七类结果，然后仅将工程树改名测试置于调试事件观察下，并开启阶段日志。退出非零时，回归仍失败，输出目录保留原生退出码、异常与正文日志。其他编译器参数继续按正常完整回归配置传入。

## 记录与范围

- `events.jsonl`：逐条刷新 Windows 创建/退出线程、加载/卸载模块和异常事件，包含线程 ID、原始地址、原生退出码与资源计数。
- `stdout.log`、`stderr.log`：原始进程输出；改名夹具在断言失败时记录阶段，任务等待失败时记录任务状态和具体错误。
- `result.json`：退出码的十进制和十六进制表示、超时状态、输入程序和日志摘要。
- `unhandled-*.dmp`：未处理异常时保存的小型转储，包含异常上下文和线程信息。

阶段覆盖旧构建准备/等待、入口改名提交、任务失效等待、头文件与资源引用更新、真实构建运行和应用销毁。夹具原有 30 秒任务等待、90 秒工作线程等待和行为断言保留。

观察器只调试它启动的测试进程；编译器等子进程仍按原方式运行。第一轮加载器断点被继续执行，后续异常交还程序处理，只有未处理异常才生成转储。转储的上下文位于观察器地址空间，按 [Microsoft 的 MINIDUMP_EXCEPTION_INFORMATION 说明](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/ns-minidumpapiset-minidump_exception_information)设置 `ClientPointers=FALSE`。接口绑定与 AMD64 上下文仅覆盖当前 Windows x64 环境。

调试器会影响线程调度；同一夹具的直接运行与调试运行应分别保留。新增诊断通过不说明历史静默退出已解决。本轮没有修改 IDE 业务模块、编译器、SDK 或已接受的便携包。
