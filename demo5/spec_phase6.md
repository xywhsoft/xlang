# xlang 第五版 Demo 后续实施规范（第六阶段）

本文档用于承接 [spec_phase5.md](/D:/git/x-lang/demo5/spec_phase5.md) 完成后的继续开发。第五阶段已完成，本文档聚焦“宿主平台内嵌化”，也就是把 `tcc` 与 `xrt` 正式收进 `xl.exe` 内部。

## 1. 文档规则

- 本文件自创建起进入冻结管理。
- 后续默认只允许：
  - 勾选/取消勾选进度项
  - 补一行极短状态说明
  - 修复编码、错字、错误路径
- 未经用户批准，不向本文档新增范围、阶段或需求。

## 2. 总体目标

- 终止 `demo5` 当前“动态加载 libtcc.dll + 裁剪式 xrt glue” 的临时结构。
- 让 `xl.exe` 自身具备完整的：
  - 编译 C 代码
  - 运行 C 代码
  - 编译 xlang 代码
  - 运行 xlang 代码
- 把 `xlang -> C -> tcc` 明确收束为前端到后端的正式产品链路，而不是 demo 期临时拼接。
- 尽量复用 `xrt` 已有能力，不在 `xlang` 内部重复实现：
  - 时间
  - 路径
  - 文件
  - 目录
  - 进程
  - 字符串
  - 线程附着
  - future / task / coroutine 所需底层支撑

## 3. 当前问题确认

当前 `demo5` 存在以下结构性问题：

- `tcc` 仍通过 `libtcc.dll` 动态加载，未内嵌进 `xl.exe`
- `xrt` 当前通过 `src/runtime/xlrt_host.c` 的裁剪宏直接展开，属于 demo 期拼接，不是正式平台层
- `xlrt_generated.h` 中混入了大量本应属于统一 runtime bridge 的 helper
- `xlang` 的运行 C 能力与编译 C 能力未统一到一个宿主抽象
- 当前 async / generator / task 的运行时仍带有明显的 demo 期最小实现痕迹

第六阶段不直接解决语言语义完整性问题，而是先把“宿主平台”收正。

## 4. 第六阶段目标边界

第六阶段只做平台收口，不做以下扩展：

- 不在本阶段引入 `xlp` 包加载器
- 不在本阶段引入模块 ABI / 多版本依赖解析器
- 不在本阶段完成 `try/catch/finally`
- 不在本阶段完成命名参数/默认参数
- 不在本阶段完成 class/custom/type bridge 完整体系

本阶段的目标是为这些后续能力提供统一宿主底座。

## 5. 目标结构

第六阶段完成后，`demo5` 的内部结构应收敛为：

```text
xl.exe
    ├─ xlang frontend
    │   ├─ lexer
    │   ├─ parser
    │   ├─ sem
    │   ├─ lowering
    │   └─ cgen
    ├─ C frontend
    │   └─ direct C input
    ├─ host platform
    │   ├─ xrt runtime bridge
    │   ├─ tcc backend
    │   ├─ symbol registry
    │   ├─ compile API
    │   └─ run API
    └─ CLI
```

核心原则：

- `xlang` 与 `C` 是两个前端
- `tcc` 是统一后端执行器
- `xrt` 是统一运行时底座
- `xl.exe` 是统一宿主

## 6. 目录调整建议

建议新增：

```text
demo5/src/
    host/
        xl_host.h
        xl_host.c
        xl_host_runtime.h
        xl_host_runtime.c
        xl_host_tcc.h
        xl_host_tcc.c
        xl_host_symbols.h
        xl_host_symbols.c
        xl_host_cli.h
        xl_host_cli.c
```

说明：

- `xl_host`：统一宿主总入口
- `xl_host_runtime`：xrt 初始化、线程附着、bridge 生命周期
- `xl_host_tcc`：内嵌 tcc 编译/运行接口
- `xl_host_symbols`：统一符号注册表
- `xl_host_cli`：`run-c/build-c` 等宿主命令入口

当前这些文件中的逻辑应逐步迁出：

- `src/runtime/xl_exec_ctx.c`
- `src/runtime/xlrt_generated.h`
- `src/runtime/xlrt_host.c`

## 7. 宿主平台分层

### 7.1 `xl_host_runtime`

职责：

- 初始化全局 xrt 环境
- 管理线程附着/脱离
- 暴露统一的 runtime attach / detach 接口
- 持有 host 级运行状态

要求：

- 不再让代码生成器直接依赖线程附着细节
- 不再让 `xl_exec_ctx.c` 自己决定 xrt 生命周期

### 7.2 `xl_host_symbols`

职责：

- 注册所有宿主可见符号
- 为 `tcc` 提供统一符号注入入口
- 统一管理：
  - xrt API
  - xlang runtime bridge API
  - std.* native bridge API
  - host helper API

要求：

- 后续增加新标准库时，只改符号注册表，不改执行器核心
- 生成代码只面向稳定 bridge 符号，不直接散落底层 xrt 名称

### 7.3 `xl_host_tcc`

职责：

- 提供内嵌 tcc 的 compile / relocate / run 能力
- 支持：
  - 从内存编译 C 文本
  - 获取入口符号
  - 执行入口
  - 生成目标文件/可执行文件

要求：

- 不再使用 `LoadLibrary(libtcc.dll)`
- 默认优先静态链接或源码内嵌
- 所有 tcc 配置统一在一个模块内处理

### 7.4 `xl_host`

职责：

- 提供统一宿主 API：
  - `xlHostRunCText`
  - `xlHostBuildCText`
  - `xlHostRunXlangFile`
  - `xlHostBuildXlangFile`
- 串接 frontend 与 backend
- 控制 dump / trace / emit 的总流程

## 8. 统一执行模型

### 8.1 运行 C

流程：

1. CLI 读取 `.c`
2. `xl_host` 建立 runtime 环境
3. `xl_host_tcc` 编译 C 文本
4. `xl_host_symbols` 注入宿主符号
5. 执行入口
6. 输出 trace / result

### 8.2 运行 xlang

流程：

1. CLI 读取 `.xl`
2. xlang frontend 生成 C 文本
3. 进入与 C 相同的宿主执行链
4. `tcc` 编译并执行

结论：

- `run xlang` 与 `run-c` 的差异只在前端
- backend 与 runtime 完全共用

### 8.3 编译 C

流程：

1. CLI 读取 `.c`
2. 调用 `xl_host_tcc`
3. 输出：
  - exe
  - obj
  - dll
  - 或其他受支持目标

### 8.4 编译 xlang

流程：

1. CLI 读取 `.xl`
2. xlang frontend 生成 C
3. 输出中间 C 或继续进入 tcc
4. 产出目标文件/可执行产物

## 9. CLI 调整建议

第六阶段建议把 CLI 扩为：

```text
xl lex <file.xl>
xl parse <file.xl>
xl sem <file.xl>
xl lower <file.xl>
xl cgen <file.xl>
xl run <file.xl>
xl build <file.xl>

xl run-c <file.c>
xl build-c <file.c>
```

可选后续扩展：

```text
xl eval-c <snippet>
xl eval <snippet.xl>
```

要求：

- `run` / `build` 默认走 xlang 前端
- `run-c` / `build-c` 直接走 C 宿主
- CLI 只做路由，不承载底层宿主逻辑

## 10. xrt 接入原则

### 10.1 不重复造轮子

以下能力默认必须优先复用 `xrt`：

- 时间
- 路径
- 文件
- 目录
- 进程
- 字符串
- 随机
- 线程附着
- future / task / coroutine 所需底层原语

### 10.2 不再使用 demo 式大裁剪

当前 `xlrt_host.c` 里的大量 `XRT_NO_*` 宏是为了快速落 demo。

第六阶段要求：

- 改成 `xl` 正式 profile
- 只关闭确实确认不会在 `demo5` 使用的模块
- 为后续 async / coroutine / task 接入保留必要运行时能力

### 10.3 bridge 归宿统一

原本散落在 `xlrt_generated.h` 的 helper，应按职责拆分：

- runtime attach/detach -> `xl_host_runtime`
- 宿主 API 包装 -> `xl_host_symbols` / runtime bridge
- std.* helper -> native module bridge

代码生成器不应继续持有大段内联宿主代码。

## 11. tcc 接入原则

### 11.1 内嵌优先

正式路线：

- `tcc` 源码参与 `xl.exe` 构建
或
- 先产出内部静态库，再链接进 `xl.exe`

不再保留：

- 默认动态加载 `libtcc.dll`

### 11.2 tcc 作为统一后端

`tcc` 不只是“运行 xlang 生成的 C”的工具，还应成为：

- 运行 C
- 编译 C
- 编译 xlang 生成的 C
- 运行 xlang 生成的 C

的统一宿主后端。

### 11.3 配置集中化

所有 include path / sysinclude path / library path / symbol add 行为，统一收进 `xl_host_tcc`。

不得再出现：

- 多处重复设置 include path
- runtime 模块自行决定 tcc 参数

## 12. 代码迁移要求

### 12.1 从 `xl_exec_ctx` 迁出的内容

应迁出：

- tcc API 装载
- include/library path 设置
- symbol add 列表
- compile / relocate / get_symbol / execute 主流程

`xl_exec_ctx` 后续只保留：

- 运行请求上下文
- trace/result 汇总
- 错误信息承载

### 12.2 从 `xlrt_generated.h` 迁出的内容

应迁出：

- runtime attach/detach
- 文件/路径/时间/进程/string helper
- host env / host name / ip helper

保留在生成头中的内容应尽量收缩到：

- 真正与生成代码 ABI 紧耦合的极少量声明

### 12.3 从 `xlrt_host.c` 迁出的内容

应改造为：

- `xrt` 正式 profile 配置入口
- 或 host runtime 内部实现文件

不再让它作为“把 xrt 整个宏展开进 runtime 目录”的临时容器。

## 13. 统一输出与可观察性

第六阶段仍必须保留强可观察性：

- `run-c` 有独立 trace
- `build-c` 有独立 trace
- `run` / `build` 继续保留原有 dump 路径
- 宿主平台新增：
  - `host.trace.txt`
  - `tcc.trace.txt`
  - `symbols.dump.json`

要求：

- 能清楚看到注册了哪些符号
- 能清楚看到 tcc 的 include/lib 设置
- 能清楚看到最终执行的入口符号

## 14. 非目标

第六阶段明确不是：

- 完成 xlang 正式 ABI
- 完成 xlp 元信息加载
- 完成多版本依赖系统
- 完成 class/custom/native object bridge
- 完成 async/coroutine 正式语义

这些都建立在宿主平台收正之后。

## 15. 第六阶段进度

- [x] `S60` 建立第六阶段 spec，并冻结范围
- [x] `S61` 建立 `src/host/` 宿主平台目录与基础接口
- [x] `S62` 抽离 `xl_host_runtime`，统一 xrt 生命周期
- [x] `S63` 抽离 `xl_host_symbols`，统一符号注册表
- [x] `S64` 抽离 `xl_host_tcc`，终止 runtime 动态加载 `libtcc.dll`
- [x] `S65` 让 `xl.exe` 内嵌 tcc，并跑通内存编译/执行 C 文本
- [x] `S66` 新增 `run-c/build-c` CLI 主路径
- [x] `S67` 让 xlang `run/build` 改走统一宿主 API
- [x] `S68` 收缩 `xlrt_generated.h` 到最小必要边界
- [x] `S69` 重构 `xlrt_host.c` 为正式 xrt profile 接入
- [x] `S6A` 补 host/tcc 符号表 dump 与 trace
- [x] `S6B` 补统一测试与 baseline
- [x] `S6C` 完成一轮 `test + regress` 收口

## 16. 推荐顺序

1. 先做 `S61-S65`
2. 再做 `S66-S69`
3. 再做 `S6A-S6B`
4. 最后做 `S6C`

## 17. 阶段完成判定

以下条件全部满足，才算第六阶段完成：

- `xl.exe` 无需外部 `libtcc.dll` 即可运行 C / xlang 主路径
- `run-c/build-c` 可用
- `run/build` 已接到统一宿主平台
- `xrt` 生命周期由宿主统一控制
- runtime helper 不再大面积散落在生成头里
- `test` 与 `regress` 稳定通过
