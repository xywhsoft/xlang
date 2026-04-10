# xlang 第五版 Demo 后续实施规范（第四阶段）

本文档用于承接 [spec_phase3.md](/D:/Git/x-lang/demo5/spec_phase3.md) 完成后的继续开发。第三阶段已完成，本文档聚焦“脚本工具型标准库”。

## 1. 文档规则

- 本文件自创建起进入冻结管理。
- 后续默认只允许：
  - 勾选/取消勾选进度项
  - 补一行极短状态说明
  - 修复编码、错字、错误路径
- 未经用户批准，不向本文档新增范围、阶段或需求。

## 2. 总体目标

- 提供更适合本地脚本和工具脚本的标准库能力。
- 继续优先复用 `xrt` 现成能力，避免在 `demo5` 里重复堆积 runtime 逻辑。
- 保持现有统一测试和回归链稳定。

## 3. 实施约束

- 新增接口优先选择只返回 `int/string/bool/time` 的能力。
- 每一项都必须具备：
  - native module 声明
  - `xlrt_*` 包装
  - `cgen` 产物检查
  - `run` 样例
  - 统一 runner 覆盖

## 4. 第四阶段进度

- [x] `Q00` 建立第四阶段 spec，并冻结范围
- [x] `Q01` 扩展 `std.time`：`now_text/date_text/time_text/relative/parse`
- [x] `Q02` 建立 `std.host`：`name/ip`
- [x] `Q03` 建立 `std.fmt`：`int_text/float_text`
- [x] `Q04` 补样例进入 `examples`
- [x] `Q05` 补生成 C 检查用例覆盖新增标准库路径
- [x] `Q06` 补 runtime 用例与 baseline
- [x] `Q07` 完成一轮 `test + regress` 收口

## 5. 推荐顺序

1. 先做 `Q01-Q03`
2. 再做 `Q04-Q06`
3. 最后做 `Q07`
