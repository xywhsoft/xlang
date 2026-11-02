# 系统信息与采样

以下命令从仓库根目录执行，Linux 将 `release/xl.exe` 换成 `./release/xl`。
不需要管理员权限，不创建或修改注册表、系统配置及业务文件。

```text
release/xl.exe run release/extlibs/sysinfo/examples/basic.xl
release/xl.exe run release/extlibs/sysinfo/examples/sysinfo_c.c
release/xl.exe run release/extlibs/sysinfo/examples/metrics.xl
release/xl.exe run release/extlibs/sysinfo/examples/metrics_c.c
```

`basic.xl` 与 `sysinfo_c.c` 展示标识、CPU型号/数量、内存总量和页大小；
同一主机上对应八行输出一致。结果是各自拥有的UTF-8文本，C在用完后
逐个 `xrtFree`，XLang自动释放。中文与有效代理对完整转换，不依赖控制台
当前代码页来解释系统API的UTF-16。非法系统文本按约定返回 `unknown`，
真实内存不足是错误，不应当作一台名为 `unknown` 的主机。

`metrics.xl` 与 `metrics_c.c` 展示可用内存、使用率和启动时长，共三行。
它们是瞬时观测，相邻两次运行的数值可能不同；不要把机器名或动态数值
硬编码成可跨主机复用的期望输出。值为0也可能表示探测不可用，当前API
不承诺仅凭数值区分该情况。这些指标不是容器配额或任务资源限制。

两组示例合计覆盖全部11个公开函数，支持 `xl build 源文件 -o 输出文件`。
正式比较与错误边界由包内 `test/check.ps1` / `test/check.sh` 验证，
其中异常系统数据只在独立C夹具中模拟，不在真实系统上制造坏配置。
