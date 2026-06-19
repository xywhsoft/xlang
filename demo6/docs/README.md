# DEMO6 设计文档口径

本目录是 DEMO6 的唯一设计口径目录。

后续讨论、设计、重构与实现默认只参考本目录下的文档：

- `设计理念.md`：记录 xlang DEMO6 的基础设计理念、第一绝对原则、语言取舍原则和用户明确确认的方向。
- `XRT集成原则.md`：记录 DEMO6 的 XRT 优先、禁止重复实现、XRT 缺口补全层和同步流程硬约束。
- `语言基础设计.md`：记录 DEMO6 的源码编码、标识符、注释、保留字、字面量、基础类型等语言基础口径。
- `表达式与控制流设计.md`：记录 DEMO6 的基础运算符、表达式、优先级和控制流口径。
- `函数设计.md`：记录 DEMO6 的函数声明、返回类型、多返回值、匿名函数和无返回值口径。
- `函数重载设计.md`：记录 DEMO6 的函数、方法、构造、custom 方法重载解析口径。
- `类型与隐性类设计.md`：记录 DEMO6 的隐性类、类型标识符三重角色和 `type` 特殊类型口径。
- `类型系统设计.md`：记录 DEMO6 当前阶段的 `class`、`custom` 设计和 C 混编定位；动态对象能力暂不进入当前方案。
- `泛型设计.md`：记录 DEMO6 的泛型声明、类型参数、显式实例化和 C 混编限制口径。
- `ABI设计.md`：记录 DEMO6 的内部 ABI、C ABI、符号生成、类型映射和 ABI dump 口径。
- `命令行设计.md`：记录 DEMO6 的 CLI、编译阶段划分、调试 dump、C 混编命令口径。
- `C混编设计.md`：记录 C 与 xlang 互相声明、导出、生成头文件和 C ABI wrapper 的设计口径。
- `第二阶段功能清单.md`：记录 DEMO6 第二阶段准备实现的基础语义夯实目标、XRT 改造边界、xvalue/type desc/type ops、静态泛型容器和 xcall 设计口径。
- `本轮开发任务_XAPI_import_混编.md`：记录 DEMO6 本轮成熟化开发任务，包括 XAPI、import、零配置 C 混编、main 入口和 `xlInit`/`xlUnit` 生命周期口径。
- `本轮工程骨架优化任务.md`：记录 DEMO6 本轮工程骨架优化任务，包括 CompileUnit、XAPI 内部模型、独立临时目录和 package resolver 地基。
- `工程收口10项任务.md`：记录 DEMO6 当前工程收口的 10 项目标，包括 canonical type、XAPI 权威化、package/import、callable descriptor、后端接口和生命周期强化。
- `RC0稳定化与import闭环.md`：记录 DEMO6 RC0 稳定化目标，以及 import 读取 XAPI 并注入 extern 声明的第一版边界。

除非用户明确要求“对照历史版本”或“回查 DEMO5 实现”，否则不再将 `demo3`、`demo4`、`demo5`、`dev` 目录中的旧文档作为 DEMO6 设计依据。

历史资料只允许作为事实背景或迁移参考，不允许自动提升为 DEMO6 标准。
