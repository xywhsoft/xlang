# XRT XACME 绑定验收

验证源码提交：`21bb31c14ca4ae025ecafa4acf2ed34c22b35bf4`。
上游快照：`2d1c55d7`，来源及文件 SHA-256 记录在包内 `upstream/source.json`。

旧 acme 占位包、案例、示例和独立设计稿已删除。新 xacme 的 43 个公开函数与
38 个业务常量由 `tools/check_xacme_api.py` 对照上游声明自动检查。
Client、Store、Obtain、五家 DNS provider 和自定义回调均使用上游实现。

| 验证内容 | Windows x64 | Linux x64 |
| --- | --- | --- |
| 源码编译器／运行时 | 通过 | 通过 |
| 基础 OOP 与自定义回调，JIT / AOT | 通过 / 通过 | 通过 / 通过 |
| PEM 双结果、只读别名原指针、替换写入，JIT / AOT | 通过 / 通过 | 通过 / 通过 |
| 带全局网络资源与模块回调的退役，JIT / AOT | 通过 / 通过 | 通过 / 通过 |
| 完整语言层模拟 CA 流程，JIT / AOT | 通过 / 通过 | 通过 / 通过 |
| 原生 OOM、借用引擎、重入与清理重试 | 通过 | 通过 |
| 独立证书／私钥配对核验 | 通过 | 通过 |

模拟流程执行真实 TLS、JWS、TXT 传播、IssueEx、IssueStored、Store、Obtain、Revoke、
Rollover 和 Deactivate，不访问公共 CA 或真实云账户。额外验证 40 条输入只有两个不同域名时
保持 C 的去重语义，以及扩展视图数组分配失败后的回滚。五家 provider 的默认 Endpoint
验证直接检查原生上下文，防止空语言字符串覆盖 C 的默认地址。

清理测试保留一个实际 UDP 对象阻止私有引擎退休，确认 Cleanup 返回 false、
CleanupPending(0) 非阻塞返回剩余数量、拥有者不会丢失，解除阻塞后可以完成清理。
这些进度值在语言外部调用错误域中仍是正常返回值。回调内重入读取会被拒绝，
该拒绝在嵌套错误域中处理后不影响外层成功返回。

PEM 通过 cstring.take 接管，双结果以临时原生拥有者保证部分接管失败时的释放。
原生测试比较转移前后指针，语言测试通过 C 指针探针比较局部别名与专门化函数返回值。
普通字符串参数的 C 读取借用原数据，配置及域名数组采用显式数据快照。

构建记录和日志保存在本任务隔离工作树的 `build/xacme/`：

* `windows-verified/result.json`：最终 Windows 验收。
* `linux-verified/result.json`：最终 Linux 验收，含原生编译输入校验值。
* `source-driver-6/build.json` 及 `source-driver-8/build.json`：1273 个编译单元的源码构建及
  经输入／对象校验后的增量更新；更新只重编译 XACME 适配器。
* `linux-compiler-1/result.json`：1275 个编译单元；缓存仅在完整依赖校验一致时复用，
  随后更新适配器和嵌入源码。
* `shipping-verified.json`：四种构建配置均完整提供登记的 855 个运行时源。

新增 XACME SDK 声明及六个 vendor／兼容源在实际生成的编译器 VFS 中逐项验证存在，
Linux 的正常 CLI 使用该 VFS 完成 AOT。通用私有 SDK 检查另有一个原有问题：
`xl6_tls_metadata_internal.h` 包含 `.inc`，不符合该检查器的“只包含 .h”规则；
`0013ddb26` 基线已存在，此次未改动相关 TLS 文件，也不将该全局检查记为通过。

本轮不替换发布目录的旧二进制。使用新增 API 前需要按正常流程重建 xlang 编译器和运行库。
验收使用隔离工作树，主工作树其他线程的未提交内容不纳入本次功能结果。
