# xlang 第五版 Demo 后续实施规范（第五阶段）

本文档用于承接 [spec_phase4.md](/D:/Git/x-lang/demo5/spec_phase4.md) 完成后的继续开发。第四阶段已完成，本文档聚焦“脚本上下文与目录工具型标准库”。

## 1. 文档规则

- 本文件自创建起进入冻结管理。
- 后续默认只允许：
  - 勾选/取消勾选进度项
  - 补一行极短状态说明
  - 修复编码、错字、错误路径
- 未经用户批准，不向本文档新增范围、阶段或需求。

## 2. 总体目标

- 提供更适合本地工具脚本的环境、目录和工作目录控制能力。
- 继续优先复用 `xrt` 现成能力，避免在 `demo5` 内部重复堆积运行时实现。
- 继续保持统一测试与回归基线稳定。

## 3. 实施约束

- 接口优先只返回 `int/string/bool`。
- `std.dir` 先用“换行分隔文本”承载结果，不在本阶段引入容器返回值。
- `std.process` 只补工作目录版本，不在本阶段扩展完整环境变量注入接口。
- 每一项都必须具备：
  - native module 声明
  - `xlrt_*` 或 host bridge 包装
  - `cgen` 产物检查
  - `run` 样例
  - 统一 runner 覆盖

## 4. 第五阶段进度

- [x] `R00` 建立第五阶段 spec，并冻结范围
- [x] `R01` 建立 `std.env`：`get/exists/cwd`
- [x] `R02` 建立 `std.dir`：`list/files/dirs/tree`
- [x] `R03` 扩展 `std.process`：`exec_in/capture_in`
- [x] `R04` 补样例进入 `examples`
- [x] `R05` 补生成 C 检查用例覆盖新增标准库路径
- [x] `R06` 补 runtime 用例与 baseline
- [x] `R07` 完成一轮 `test + regress` 收口

## 5. 推荐顺序

1. 先做 `R01-R03`
2. 再做 `R04-R06`
3. 最后做 `R07`
