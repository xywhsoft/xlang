# xs 2.0

`xs` 是使用 xlang 编写的多协议服务宿主。网络、TLS、HTTP、线程、日志和基础数据结构由 XRT 提供；xs 负责严格配置、服务装配、C/xlang 动态处理器、代际 reload 和生命周期管理。

当前核心能力：

- HTTP/HTTPS、静态文件、精确 Host、通配 Host 和 TLS SNI。
- TCP/UDP 固定工作池、有界队列、超时和运行指标。
- TCP 支持 `raw`、UTF-8 `text`、自定义 `delimiter` 和 4 字节网络序 `length-prefix` framing。
- C 与 xlang 共用 `xs_handler` ABI，不使用子进程和临时文件交换请求。
- 每个动态处理器使用固定大小实例池；reload 先完整构建新代，再原子切换。
- 严格 JSON 配置、滚动日志、状态快照、信号 reload 和优雅停止。

## 运行

从 `demo6/release` 目录执行：

```bat
xl run projects\xs\xs.xl -- --check
xl run projects\xs\xs.xl -- --status
xl run projects\xs\xs.xl -- --duration 10000
```

默认端口：HTTP `18080`、TCP `18081`、UDP `18082`。

## Handler

C 和 xlang 都导出固定入口：

```c
int xs_handler(xs_handler_frame* frame);
```

xlang：

```xlang
export "xs_handler" func handle(point raw) as c_int
    auto frame = sdk.handler.wrap(raw)
    frame.write("hello")
    return c_int(sdk.handler.OK)
end
```

同目录下同基名的 `.xl` 与 `.c` 是一个 xlang 模块的伴生源文件，会一起编译。两个独立 handler 必须使用不同基名，例如 `echo_c.c` 和 `echo_xlang.xl`。

## 测试

```bat
projects\xs\tests\run_fast.bat
projects\xs\tests\run_integration.bat
projects\xs\tests\run_stress.bat
projects\xs\tests\run_soak.bat
projects\xs\tests\run_memcheck.bat
projects\xs\tests\run_all.bat
```

- `fast`：配置、handler 生命周期与故障恢复、日志和博客数据层。
- `integration`：HTTP/HTTPS、TCP/UDP、原子 reload、异常协议输入和博客端到端流程。
- `stress`：C/xlang handler 并发 reload、HTTP 并发 reload 和 TCP 并发流量。
- `soak`：反复创建、启动、请求、reload 和停止多协议运行时。
- `memcheck`：对子进程私有内存取样，检查预热后是否出现明显持续增长。
- `all`：按以上顺序执行全部套件。`test_handler_runtime.bat` 与 `test_smoke.bat` 保留为单项入口，不是兼容语法。

核心测试也可逐项运行：

```bat
xl run projects\xs\src\config_test.xl
xl run projects\xs\src\handler_runtime_test.xl
xl run projects\xs\src\socket_service_test.xl
xl run projects\xs\src\http_service_test.xl
xl run projects\xs\src\service_runtime_test.xl
xl run projects\xs\src\atomic_reload_test.xl
xl run projects\xs\src\logging_test.xl
```

配置与发布验收依据见 `demo6/docs/xs2生产级重构设计.md`。
