# OpenCV 扩展库

`opencv_artifact.c` 加载 `bin/opencv_xlang.dll`，向 XLang 暴露图像读写、裁剪、变换、截图及张量转换。桥接 DLL 的源码不在当前仓库，其内部构建与维护由插件独立负责；普通扩展库任务只验收 XLang/C 调用链，不据此宣称桥接实现本身已通过发布验收。当前 `image_info` 双语范例读图及缩放同输出；完整 AOT 仍受主线共享运行时缺符号阻断。

包清单只声明包装层实际使用的 XRT once、字符转换、路径、环境变量与字符串模块。同一无输出 AOT 调用探针从 3,793,920 缩至 865,792 字节；构建后实际装载桥接 DLL，完成 PPM 读入与缩放尺寸检查。这不代表带输出的图像范例 AOT 已通过。

装载器现在通过 XRT `xrtOnce` 串行完成模块装载和函数表发布。Windows DLL 与环境变量路径按 UTF-8 处理，程序目录定位和拼接使用 XRT 路径 API，不再以 ANSI 路径或固定 512 字节缓冲判断文件是否存在。加载成功后的后端路径由包装层持有至进程结束；返回的 `backend()` 文本只读，不由调用方释放。
`OPENCV_XLANG_DLL` 显式设置时只装载该路径，不静默回退到 `PATH` 或程序目录；未设置时才执行常规搜索。桥接 DLL 必须提供全部公开包装入口；缺少符号时 `available()` 返回 false，`last_error()` 报告桥接符号不完整，不把部分可用的旧 DLL 当成完整后端。

## C 与 XLang

公开 C 原型位于 `include/opencv.h`，实现文件也包含该头以检查签名一致性。`opencv_imread_native`、`opencv_resize_native` 等返回的非空图像句柄由调用方拥有，以 `opencv_release_native` 释放一次；`opencv_backend_native`、`opencv_last_error_native` 和张量 JSON 返回的是借用文本，不得释放或长期保留。XLang 的 `Image` 包装类可显式 `release()`，未释放时由对象析构清理。

`examples/image_info_c.c` 与 `examples/image_info.xl` 读取同一张包内 P6 PPM 图片并缩放到 4×4，两者输出完全一致。更全面的 XLang API 范例在 `release/examples/extlibs/opencv.xl`；它需要可截图的桌面，但图片只写入独占系统临时目录，并在退出时清理，不作为无显示环境的日常门禁。
从 `release` 目录可直接运行 `xl run extlibs/opencv/examples/image_info_c.c`，
也可直接构建该 C 范例；单文件 TCC 路径复用包内装载包装层，桥接 DLL
仍动态装载。包门禁把 GCC 分离构建、直接 JIT、直接构建执行与 XLang
范例的同一图片输出逐字对照。
XLang 读图与写图入口拒绝含 NUL 的路径，避免 C 字符串只处理前缀而误读或误写其他文件。

## 验证

从 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/opencv/test/check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/opencv/test/check.ps1 -SkipAot
```

门禁执行严格 GCC 编译、C/XLang 范例同输出、缺图与无效尺寸负例、
所有权释放、中文 UTF-8 路径读图、显式 DLL 路径正例及残缺桥接 DLL
拒绝。默认模式还验证 C 直接构建、无输出核心 AOT，以及完整 XLang
图像信息范例 AOT 与 C 同输出；`-SkipAot` 只做 C/JIT 聚焦检查。
测试图像和残缺 DLL 源码在包内；门禁每次在 `build/test` 下使用独占
目录，结束时清理，不触碰旧的固定目录。Linux 可运行
`sh release/extlibs/opencv/test/check.sh` 做严格语法检查；当前仓库
只有 Windows 版桥接 DLL，Linux 检查不宣称能运行图像后端。

`release/extlibs/rapidocr/test/check.ps1 -SkipAot -Stress` 另用 C 16 线程和 XLang 双线程冷探针同时覆盖 OpenCV 与 ONNX Runtime 的首次装载。当前机器均通过。桥接源码、完整图像操作矩阵、跨平台二进制和长时并发由插件独立维护，不纳入普通扩展库调用验收。

`test/catalog_call.xl` 已登记为 `coverage.tests`，从 `demo6` 根目录调用包内 PPM 读图、缺图、缩放和句柄释放；它不重复验证插件内部图像算法。

2026-10-18：C `opencv_last_error_native()` 原在首次装载前直接读取共享函数表，
对残缺桥接 DLL 的首次错误查询返回空文本。现错误查询也等待同一 `xrtOnce`
装载完成，再返回后端或包装层诊断；独立残缺 DLL 夹具先红后绿。
Windows `-SkipAot` 严格 C/XLang/UTF-8 路径/双语门禁和 Linux 包装层语法检查
通过。此修复只收紧公开调用边界，不验证桥接插件内部的错误状态并发语义；
带输出 AOT 的共享阻断不变。

2026-10-20：Windows 门禁脚本含 `中文.ppm` 路径字面量，脚本原为无
BOM UTF-8；PowerShell 5.1 按系统代码页误读，使中文路径用例失去原定覆盖。
脚本现带 UTF-8 BOM；`-SkipAot` 下严格包装层编译、C/XLang 图片调用、
中文路径、显式桥接路径及残缺 DLL 拒绝均通过。本任务只确认公开
调用链，不据此认证插件内部或完整 AOT。

2026-10-20：包内 NUL 路径负例原先会先删除固定 `build/test/opencv/nul_output.png`，
有误删已有文件的风险；现改为独占临时目录并在 `finally` 清理。综合截图
范例也不再写固定 `examples_out`。该范例从 `demo6` 与 `release` 工作目录
各运行成功一次；Windows `-SkipAot` 的严格 GCC、C/XLang 双语调用、
UTF-8 路径和残缺桥接拒绝门禁通过。仍只验公开调用层，不宣称桥接源码
可复现构建或完整带输出 AOT 已通过。

2026-10-22：默认门禁原在无输出核心 AOT 后报告全绿，未构建公开图像
信息范例。现默认继续构建/运行该范例并与同次 C 输出对照；`-SkipAot`
跳过全部 AOT 路径。聚焦门禁通过，默认门禁的 C 直接构建、核心 AOT、
UTF-8 路径和残缺 DLL 检查通过后，完整范例构建在共享主线符号
`xl6ValueDataSnapshotV1` 处准确失败。脚本 UTF-8 BOM 保持不变，本次
独占目录在失败后清理；不签收完整 AOT 或桥接插件内部实现。
