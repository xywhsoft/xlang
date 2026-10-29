# gzip AOT: XRT 网络端口能力符号未解析

2026-10-21，在当前 Windows 发布版 `release/xl.exe` 上，从
`demo6` 目录运行：

```powershell
.\release\xl.exe build release/extlibs/gzip/examples/basic.c -o build/test/gzip_basic.exe
```

链接报错：`unresolved reference to 'xrtNetWorkerPortCapabilitiesV1'`。
`tools/check_extlib_gzip.ps1` 的文件分配夹具 AOT 构建也在同一符号停止；
此前的严格 C、Python/zlib 互通和 XLang 文件事务 JIT 检查均通过。

当前 `D:\GIT\xrt\src\network\tcp.c`、单头文件以及
`lib/xrt/xrt.h` 中可找到该符号的调用点，但在这些源码中没有找到
对应声明或实现。相关 XRT 源文件正由独立工作线修改，不能在 gzip 包内
伪造同名函数或关闭网络模块来绕过。建议 XRT 维护线核对发送文件能力
检查所需的公开端口能力 API，补齐实现或改用已有 API；随后重新生成并
同步单头文件，重建 `xl.exe`，复跑上述最小构建和 gzip 完整门禁。

这是一条当前源码快照的阻断记录，不证明 gzip 本体存在缺陷，也不代表
此前记录的 XLIR/AOT 共享阻断已消失。
