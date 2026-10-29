# XRT 与 xlang 系统整合任务

## 1. 目标

XRT 是可独立用于 C 的运行库，也是 xlang 的标准运行库底座。整合工作的目标是让 XRT 提供稳定、可裁剪、可测试的真实能力，xlang 只保留语言类型、所有权、异常和易用性适配，不在编译器或标准库中复制第二套实现。

## 2. 当前范围

本任务按依赖顺序整合：

1. 基础类型、错误、内存、字符串、时间、路径和文件。
2. 同步原语、线程、协程、Future、任务、取消和等待源。
3. 网络引擎、DNS、TCP、UDP、TLS、背压、缓冲和零复制传输。
4. xlang 异常映射、资源生命周期、native-backed class、TCC 符号和裁剪宏。
5. 双仓 ABI、单头文件、测试、性能和发布门禁。

## 3. HTTP 与 WebSocket 边界

本任务只整合能够构成协议底座或高性能链路的能力：

- HTTP 快速路径所需的 request-target、Host、Header、Cookie 与 HTTP/1 报文基础解析和写出。
- 增量解析、流式正文、明确长度、chunked、压缩解码和 Upgrade。
- 原始报文、借用引用、拥有引用、vectored send、背压和直接响应路径。
- WebSocket 握手、帧、消息流、控制帧、压缩和客户端/服务端传输状态机。

以下能力延期到 HTTP/WebSocket 专项评估，不在本任务中新增、迁移或扩张：

- 属性式请求/响应对象构建器和同义便利封装。
- 自动 JSON 对象映射、业务路由框架、中间件框架和模板化响应。
- 自动重试、重定向、Cookie Jar、缓存等不属于协议快速路径的客户端策略。
- 完整 URL、Query 参数对象、`application/x-www-form-urlencoded`、multipart 表单对象等高级协议抽象。
- 为追求表面易用而引入多次临时存储、重复拼接或强制对象图的路径。

旧版延期能力不再由核心 XRT/xruntime 快照提供。`xregex` 已通过正式源码包重新接入；`xhttp`、`xws` 的 C 扩展产品也已完成收口，但高级语言包装按当前范围继续延期。完整 URL、Query 与表单语言桥接资产保存在 `dev/deferred/xhttp`，不得通过向核心快照补回符号恢复。XRT 核心保留 `xrtHttpTarget*` 与 `xrtHttpHost*` 低层能力，并已由 HTTP/1.1 核心测试锁定。任何快速路径都不得依赖延期能力。

## 4. 快照规则

发布构建只使用仓内的两个产品快照：

- `lib/xrt/xrt.h`：基础运行库和协议快速路径。
- `lib/xruntime/xruntime.h`：内嵌所需 XRT 闭包，并增加类型、对象、动态调用和 typed 容器。

宿主、内存执行和默认链接对象使用完整 xruntime 产品。生成的独立程序始终编译 `xruntime.h`，再由裁剪器同时选择 xruntime 与其内嵌 XRT 核心的最小模块闭包；不得重复编译 `xrt.h` 实现。普通构建只校验仓内快照，不访问外部 XRT 仓库。

当前逐模块与逐 API 对应状态由 `XRT与xlang能力矩阵.md` 和 `tools/gen_xrt_xlang_matrix.py` 维护。低层函数必须全部进入 TCC 符号面；语言内核与标准库只对适合语言表达的能力提供类型、方法或命名空间抽象，未包装函数仍可通过显式 C ABI 使用。

维护同步必须执行：

```powershell
python tools/sync_xrt_snapshot.py --sync --xrt-root D:\GIT\xrt
```

同步器先审计 XRT 与 xruntime 的成熟度和单头一致性，再同步两套实现头、两套声明头、独立的 schema 3 元数据和由 xruntime 完整公共面生成的 TCC 符号表。禁止手工修改快照或仅复制头文件而不更新派生资产。

纯仓内校验：

```powershell
python tools/sync_xrt_snapshot.py --check
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_xrt_snapshot.ps1
```

## 5. 完成标准

- XRT 和 xlang 对同一类型、错误、所有权、取消和等待语义只有一个规范来源。
- xlang 适配器不复制 XRT 已有算法或协议状态机。
- 每个绑定具备 C 对照测试、xlang 正负向测试和重复释放/取消/超时边界测试。
- 裁剪配置由真实使用符号和模块依赖生成，未使用体系不进入最终 XRT 实现单元。
- Windows 与 Linux 的构建、TCC 注入、DLL 导出、单头文件和 ABI 门禁全部通过。
- HTTP/WebSocket 快速路径具有独立性能门禁，且不依赖延期的非快速路径。

## 6. 对象所有权契约

当前 XRT 不存在 local/shared 对象模式，xlang 不得在私有适配层伪造该能力：

- 普通赋值和闭包捕获通过 retain 保持同一对象身份。
- 临时值和返回值通过 move 转移所有权，不得因跨线程或写入全局而隐式逐项复制。
- 只有语言语义要求独立结果时才显式 clone/copy，例如重复 await 同一 Future 的类型化容器结果。
- mutex、channel、Future、thread join 等同步原语负责跨线程发布和内存可见性。
- 同一可变对象被多个线程访问时，调用方必须使用同步原语；原子引用计数只保证生命周期，不保证复合读写互斥。

`tools/check_xrt_ownership_contract.ps1` 禁止旧对象模式、恒等 Share/发布 helper 和带无效 mode 参数的 copy helper 回流。

`release/include/xrt_decl.h` 与 `lib/xrt/xrt.h` 锁定核心产品；`release/include/xruntime_decl.h` 与 `lib/xruntime/xruntime.h` 锁定扩展运行时产品。两份 schema 3 元数据分别记录哈希、字节数和 API 数，xruntime 还记录自身新增 API 数。`xl_runtime.h` 只包含 `xruntime_decl.h`，不镜像 XRT 类型、结构体或函数原型；`tools/check_xrt_header_abi.ps1` 以三种包含顺序验证声明 SDK 与语言运行时头可以共存。
