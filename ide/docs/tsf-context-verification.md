# Windows TSF 上下文与双语法编辑器验收

2026-10-11 已通过实际 Windows TSF COM 上下文测试。C 和 xlang 各自的编辑器完成全文读取、UTF-16 选区核对、中文替换、单步撤销和重做，共 39 项断言。此项补充 [原生输入与 IME](input-and-ime.md) 的事件队列验收。

## 实际连接与结果

测试先创建真实 XGE 窗口和完整 IDE 工作台，通过正式文档与编辑器接口分别打开 C、xlang 文档。随后在同一 GUI 线程取得 Windows `ITfThreadMgr` 的焦点文档与顶部 `ITfContext`，申请实际读写编辑会话。没有创建替代文本存储，也没有修改 SDK 或调用 `xgeInputEventPost`。

测试文本同时包含 ASCII、中文、表情、换行和对应语言的代码。Windows 返回的全文必须与原文一致；选中表情时，CodeEdit 的四个 UTF-8 字节必须对应 TSF 的两个 UTF-16 代码单元。测试使用 `ITfRange::SetText` 将该选区替换为“文字”，经 SDK 正式输入桥进入文档，核对其余正文完整、语言标识保持 C/xlang、脏状态及版本增加一次。IDE 正式撤销／重做动作必须分别恢复原文和替换后的正文。

两次实际会话均返回 `request=0`、`session=0`、`callbackResult=0`，回调各执行一次。最终 `assertions=39 result=passed`，进程退出码为 0。

Windows API 合同见微软文档：[焦点文档](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgr-getfocus)、[编辑会话与读写锁](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontext-requesteditsession)、[范围替换及临时组合](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfrange-settext)。本次结果是上述接口的实际执行证据。

## 复验

独立工具使用成功的严格 Release 构建报告，在新目录编译测试程序并执行。工具逐项核对原始源码和构建快照副本、对象文件、输出以及 SDK；不重编 IDE，不覆盖旧构建、测试日志或便携包。

```powershell
& 'C:\Program Files\Python312\python.exe' `
  'D:\GIT\x-lang\ide\tools\check_tsf_context.py' `
  --build-report 'D:\GIT\x-lang\ide\build\cb-6bcdf568\current-build-report.json' `
  --output 'D:\GIT\x-lang\ide\build\tsf-context-recheck-new'
```

每次指定尚不存在的输出目录。当前成功记录位于 `build/tsf-context-canonical-v2/evidence.json` 和 `run.log`，使用 774 份严格源码输入、84 个对象文件、106 份严格输出和 SDK 目录内 17 个实际文件。退出码、执行 PID、输入／日志／可执行文件指纹及执行后输入不变检查均已记录。

测试运行在本工具创建的私有 Win32 桌面，始终不切换用户桌面、不激活输入法配置、不改写系统输入语言或注册表。测试结束后释放自己的 COM 引用、进程句柄和桌面。它通过本程序窗口的真实 Win32 焦点消息取得上下文；不代表实体键盘焦点切换已经验收。

初始 C 测试的 `interface` 字段与 Windows 宏冲突，并误将返回 void 的 IDE 动作当成状态码；失败源码和编译日志保留在 `build/tsf-context-v1/`。修正后的第一次实际通过记录保留在 `build/tsf-context-v2/`。验证工具最初使用了错误的 SDK 清单文件名，未执行原生测试；原工具、失败记录保留在 `build/tsf-context-canonical-v1/`，修正后结果单独写入 v2。

## 当前输入法条件与剩余边界

只读 TSF 枚举程序实际查询全部配置及语言配置，只在私有测试桌面返回一个中文普通键盘布局，未得到可调用的文本输入服务配置。两次实际枚举结果保留在 `build/tsf-readonly-v2/`、`build/tsf-readonly-v3/`。注册表同时显示当前用户微软拼音配置启用，且系统中文输入法目录存在。因此不能把私有桌面的枚举结果解释为本机没有安装输入法。

本次测试连接的是实际 Windows TSF 与 SDK 文本存储通路，并由独立测试客户端请求编辑；没有驱动微软拼音引擎或候选窗口。`physicalImeVerified`、`candidatePagingVerified` 和 `mixedMonitorDpiVerified` 继续保持 false。拼音／五笔等实体输入法的候选翻页、选词、重转换、系统快捷键，以及跨实体显示器的 DPI 候选定位仍按既有设备验收要求处理。

IDE 产品源码、默认编译器、固定 SDK 和当前便携候选保持原样。此增量没有重新执行完整 IDE 回归，也没有改变正式发行状态。
