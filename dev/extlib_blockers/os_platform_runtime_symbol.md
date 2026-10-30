> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# os.platform 运行时符号缺失

2026-10-28 在 ImageHash 测试准备中发现；不修改扩展包或 xlang 主线
来掩盖该问题，ImageHash 产品路径没有调用此 API。该观察不改变包
发布矩阵的 G/R/M/X 统计。

从 `demo6` 运行：

```powershell
release\xl.exe run dev\extlib_blockers\os_platform_runtime_symbol.xl
```

当前 Windows `xl.exe` SHA256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
运行退出 1，TCC 报 `unresolved reference to 'xl6_xrt_os_platform'`。
公开设计定义返回 `windows`/`linux`/`macos`/`unknown`；当前标准库
声明表仍引用该符号。本记录仅确认该发布物的 Windows JIT，不据此
宣称 Linux、AOT 或另一份发布物有相同问题。

主线应检查函数实现、裁剪依赖和内存执行环境符号注册是否同步，
修复后同时验证 JIT/AOT 与嵌入上下文；不能仅绕过一个扩展库的调用。
不要为此在 ImageHash 添加系统识别 C 包装器。其预算夹具改由测试
门禁显式传入已测的平台数值，故障注入之外处理，产品 API 不变。
