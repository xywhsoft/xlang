# xlang 第五版 Demo 后续实施规范（第三阶段）

本文档用于承接 [spec_phase2.md](/D:/Git/x-lang/demo5/spec_phase2.md) 完成后的继续开发。第二阶段已完成，本文件聚焦“标准库扩展以支撑更复杂脚本”和“继续复用 `xrt` 现成能力”。

## 1. 文档规则

- 本文件自创建起进入冻结管理。
- 后续默认只允许：
  - 勾选/取消勾选进度项
  - 补一行极短状态说明
  - 修复编码、错字、错误路径
- 未经用户批准，不向本文件新增范围、阶段或需求。

## 2. 总体目标

- 扩大 `demo5` 可直接使用的标准库范围，让 `xlang` 可以承担更像脚本语言的本地任务。
- 优先把 `xrt` 已有现成能力通过 `xlrt_*` 和 native module 暴露给生成 C，而不是重复自写 runtime 逻辑。
- 保持 `lex -> parse -> sem -> lower -> cgen -> run -> test -> regress` 主链持续稳定。

## 3. 实施约束

- 优先扩已有模块：`std.fs`、`std.path`、`std.str`；新模块首选 `std.rand`。
- 新能力必须满足：
  - 有最小 native module 声明
  - 有 `cgen` 可见产物
  - 有 `run` 可执行样例
  - 有统一 runner 用例
- 不引入依赖当前语言尚未具备的数据结构能力，例如数组、字典、对象系统。

## 4. 第三阶段进度

- [x] `R00` 建立第三阶段 spec，并冻结范围
- [x] `R01` 扩展 `std.fs`：`is_file/is_dir/size/copy/move/delete`
- [x] `R02` 扩展 `std.path`：`name_ext/random`
- [x] `R03` 扩展 `std.str`：`lower/upper/replace`
- [x] `R04` 建立 `std.rand`：`int32/range/text`
- [x] `R05` 补标准库扩展样例进入 `examples`
- [x] `R06` 补生成 C 检查用例覆盖新增标准库路径
- [x] `R07` 补 runtime 用例与 baseline
- [x] `R08` 完成一轮 `test + regress` 收口

## 5. 推荐顺序

1. 先做 `R01-R04`
2. 再做 `R05-R07`
3. 最后做 `R08`
