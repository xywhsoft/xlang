# xs prototype

`xs` 是基于 xlang demo6 标准库实现的 xserver 雏形，用于验证当前网络、进程、脚本运行和 C/xlang 混编能力。

当前覆盖能力：

- 读取 JSON 配置。
- 启动 HTTP 服务，支持静态目录和动态 route。
- HTTP route 可调用 `.xl` 或 `.c` handler。
- 启动 TCP 服务并调用 handler 处理文本数据。
- 启动 UDP 服务并调用 handler 处理数据报。
- 启动 custom 服务，用于定时任务、启动钩子或自定义后台任务。

运行：

```bat
cd D:\GIT\x-lang\demo6\release
xl run projects\xs\xs.xl -- --duration 8000
```

smoke：

```bat
projects\xs\test_smoke.bat
```

handler 调试调用约定：

- HTTP: `xl run handler -- http METHOD PATH QUERY BODY`
- TCP: `xl run handler -- tcp SERVICE_NAME DATA`
- UDP: `xl run handler -- udp SERVICE_NAME DATA REMOTE_ADDR REMOTE_PORT`
- custom: `xl run handler -- custom SERVICE_NAME`

xs 内部 handler 采用常驻池：

- `.xl` handler 注册时创建 `XLangHandler`，后续请求复用同一个 `XL_Context`。
- `.c` handler 注册时创建 `TccHandler`，创建阶段完成 `addFile + relocate`，后续请求直接调用缓存的 `main(argc, argv)`。
- xs 内部会在上述参数末尾追加一个输出文件路径，handler 将响应写入该路径；直接 `xl run handler ...` 调试时没有输出路径，handler 会回退到 stdout。
