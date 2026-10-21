# Windows 工作目录路径解析

实现状态：开发候选，2026-10-09。路径服务在显式基目录下解析普通相对路径、同盘驱动器相对路径与盘根相对路径，不改变 IDE 进程的当前目录。任务租约沿用实际项目编译目录；原始 `XLANG_PACKAGE_PATH` 仍原样传递。

例如项目在 `D:/project`、IDE 启动目录在 `D:/shell` 时，`D:packages` 指向 `D:/project/packages`，`D:` 与 `D:.` 指向项目目录。若编译器工作目录在 C 盘，`/packages` 与 `\packages` 使用 C 盘根。其他盘的 `X:packages` 使用继承环境中的 `=X:` 目录项；该项不存在时从 X 盘根开始。解析和规范化失败会返回失败，不临时调用 `SetCurrentDirectory`。

这些形式依据 [Windows 路径格式](https://learn.microsoft.com/en-us/dotnet/standard/io/file-path-formats)、[GetFullPathNameW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfullpathnamew) 和 [CreateProcessW 的目录环境说明](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw) 核对，并用实际本机子进程对照。完整绝对路径、UNC 与设备路径保持显式拼写；设备前缀工作目录的驱动器/盘根相对结果也与实际 Win32 子进程保持一致，不把 Windows 返回的无效或不完整路径当成有效文件。

## 原生验证

`path_context_test.c` 使用实际路径服务、Win32 API 和固定 XGE/XRT DLL。`check_windows_paths.py` 比较词法解析、物理解析、实际 Win32 子进程与实际 XRT 子进程的结果，覆盖 Unicode、普通/驱动器/盘根相对、跨盘、大小写盘符、盘符单独形式、显式设备前缀及不存在的基目录。其他盘的环境目录分别测试存在和未设置；只向隔离子进程传环境，不修改父进程环境或系统盘映射。本机有不同于测试根的固定盘时，执行跨盘和继承目录检查；单盘设备如实记录这些检查未执行。

包阶段原生测试另行运行普通相对和驱动器相对两种模式，验证活动任务及失败后的历史引用确实防止清理项目缓存。驱动器模式没有普通相对项代为保留同一缓存；修改前的实际业务对象复现保护断言失败，仅替换路径模块后通过。

## 验收边界

本机矩阵不能代替真实 UNC/映射网络驱动器、任意深工作目录、外部工具自身的长路径预算、设备切换与所有文件系统。网络缓存自动删除继续禁用。其他盘目录项在解析/启动时沿用进程环境；本轮没有增加所有环境变量的提交时快照。完整目录身份、租约与阶段清理策略见 [阶段目录保留](task-stage-storage.md)，原始包环境捕获见 [包搜索目录](package-search-paths.md)。
