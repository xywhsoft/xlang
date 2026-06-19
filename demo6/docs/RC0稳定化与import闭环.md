# DEMO6 RC0 稳定化与 import/XAPI 闭环

本轮目标是进入 RC0 稳定化，不新增未设计语法，只把已经存在的 import/XAPI 机制打通到可用状态。

## RC0 边界

- import 读取本地 package/path resolver 的结果。
- resolver 找到 XAPI 接口文件后，编译器读取其中的导出信息。
- XAPI 中可直接落到第一阶段 C ABI 的函数/变量，会转换为当前模块的 `extern` 声明。
- xlang 源码仍然必须显式 `import`，不会自动污染环境。
- 不解析 C 文件，不从 C 自动生成接口文件。
- 不引入 `.xli`。

## 当前实现策略

- `CompileUnit` 阶段顺序为 parse -> import -> sem。
- import 展开只处理机器生成的 `xlang-xapi-1` 接口。
- 当前读取器是严格 XAPI 读取器，只消费 DEMO6 生成字段；后续可迁入 XRT JSON/XSON。
- package manifest 与 XAPI 分离：`xlang.package.json` 是包描述，`package.xapi.json` 是接口元数据。

## 第一阶段可导入内容

- `cdecl` 函数。
- 非可选、非可变参数。
- 0 或 1 个返回值。
- C ABI 可表达的基本类型。
- `var` 导出转换为 `extern var`。

以下内容暂不自动注入：

- 多返回值导出。
- 可选参数/可变参数导出。
- class/custom/object 类型导出。
- export const 的只读语义。
- package 自动链接参数。

## 主要风险

- `xl6_cgen.c` 仍然过大，是 RC0 前最大的维护风险。
- 编译器本体暂未直接链接 XRT，因此 XAPI 读取器暂时没有复用 XRT JSON。
- package 自动链接尚未完成，导入函数的实现仍需通过 `--link` 或后续 package links 字段提供。
- XAPI 消费侧目前只完成 C ABI extern 注入地基，xcall/function object/class/custom 的元数据消费还需后续阶段推进。
