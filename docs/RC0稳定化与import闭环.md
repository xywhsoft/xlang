# DEMO6 RC0 稳定化与 import/package 闭环

本文件记录 RC0 阶段 import/package 闭环的任务边界和当前状态。它属于阶段记录；最新设计入口仍以 `README.md`、`扩展库系统设计.md`、`C混编设计.md` 和 cgenrule 文档为准。

## RC0 边界

- import 读取本地 package/path resolver 的结果。
- resolver 找到 ABI 元数据 接口文件后，编译器读取其中的导出信息。
- ABI 元数据 中可直接落到第一阶段 C ABI 的函数/变量，会转换为当前模块的 `extern` 声明。
- xlang 源码仍然必须显式 `import`，不会自动污染环境。
- 不解析 C 文件，不从 C 自动生成接口文件。
- 不引入 `.xli`。

## 当前实现策略

- `CompileUnit` 阶段顺序为 parse -> import -> sem。
- import 展开只处理机器生成的 `xlang-abi-1` 接口。
- 当前读取器是严格 ABI 元数据 读取器，只消费 DEMO6 生成字段；未知字段按前向演进策略跳过。
- package manifest 与 ABI 元数据 分离：`xlang.package.json` 是包描述，`xlang.package.json` 是ABI 元数据。
- package resolver 会收集 `links` 与 `loadlibs`，CLI 构建阶段会把它们并入链接命令。
- 导入符号进入私有命名空间，使用侧通过显式 `import` 和命名空间访问，不污染全局环境。

## 当前可导入内容

- `cdecl` 函数。
- 非可选、非可变参数函数。
- 0 或 1 个返回值。
- C ABI 可表达的基本类型。
- `var` 导出转换为 `extern var`。
- `class` 类型声明、字段和继承布局。
- `union` 类型声明、字段和 inline union 布局。
- native-backed class 类型名占位声明。
- package dependency 元数据和版本约束。

以下内容暂不自动注入：

- 多返回值导出。
- 可选参数/可变参数导出。
- native-backed class 方法实现体或生命周期语义。
- object 动态对象类型。
- export const 的只读语义。
- xcall/function object 的完整元数据消费。

## 当前风险与维护要求

- cgen 已拆成多个分片，但少数分片仍偏大；后续优化应继续沿职责边界细拆。
- ABI 元数据 读取器仍是编译器内部严格读取器，后续如果迁入 XRT JSON/XLON，需要保持错误诊断和未知字段兼容行为不退化。
- package 自动链接已进入 CLI 链接流程，后续需要继续压测相对路径、重复链接去重和动态库加载顺序。
- ABI 元数据 消费侧已经覆盖函数、变量、class、union 和 native-backed class 占位；xcall/function object/native-backed class 方法的完整消费仍属于后续深化项。

### ABI 元数据/package 破坏性测试清单

以下内容属于扩展库系统进入发布候选前必须补齐的边界压测，不应通过隐式兼容路径掩盖问题：

- 相对路径：同一个 package 从不同工作目录、入口文件目录和 extlibs 目录解析时，链接输入必须稳定。
- 重复链接：同一路径通过 package 依赖链重复出现时，必须精确去重；路径规范化策略需要先定稿再实现。
- 动态库顺序：loadlib 与静态链接输入同时存在时，必须记录并验证加载顺序。
- 读取器行为：未知字段、字段类型错误、缺失必填字段必须给出稳定诊断，不把坏 ABI 元数据 当作空声明继续编译。
