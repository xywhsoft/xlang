# xlang 第五版 Demo 后续实施规范（第二阶段）

本文档用于承接 [spec.md](/D:/Git/x-lang/demo5/spec.md) 完成后的后续开发。第一阶段 `S00-S30` 已封口，本文件只覆盖两个后续重心：

- `xrt` 接管与 runtime 去重
- 标准库扩展与复杂样例测试支撑

## 1. 文档规则

- 本文件自创建起进入冻结管理。
- 后续默认只允许：
  - 勾选/取消勾选进度项
  - 补一行极短状态说明
  - 修复编码、错字、错误路径
- 未经用户批准，不向本文件新增范围、阶段或需求。
- 第一阶段 spec 继续保留冻结状态，不回流扩项。

## 2. 总体目标

- 把当前 `demo5` 中与 `xrt` 重叠的 runtime helper 逐步迁移到统一 `xlrt` 适配层。
- 让生成的 C 代码明确进入 `xrt` 运行时环境，而不是继续长期停留在“纯手写最小 helper”状态。
- 建立第一批真正可用的标准库模块，支撑文件、路径、时间、字符串、进程等更复杂功能测试。
- 保持现有 `lex -> parse -> sem -> lower -> cgen -> run -> test -> regress` 主链稳定，不因迁移造成整链退化。

## 3. 实施约束

- 优先抽象 `xlrt_*` 适配层，再把具体能力迁到 `xrt`。
- `codegen` 不直接扩散大量 `xrt` 细节；优先调用稳定的 `xlrt_*` 包装。
- 每迁一类 runtime 能力，都必须保留：
  - 可查看生成 C 的路径
  - 可运行样例
  - 可纳入统一测试 runner
- 标准库优先 native module 形态落地，后续再考虑更丰富的包组织。
- 新增标准库模块必须先提供最小样例，再纳入统一测试。

## 4. 第二阶段进度

- [x] `P00` 建立第二阶段 spec，并冻结范围
- [x] `P01` 建立生成 C 使用的 `xlrt` 适配层骨架
- [x] `P02` 让生成 C 入口显式接入 `xrt` 线程运行时
- [x] `P03` 将 `print/println` 从内嵌 helper 迁到 `xlrt`
- [x] `P04` 建立 `std.io` 模块并接通输出主路径
- [x] `P05` 将 `std.time` 主路径迁到 `xrt` 时间接口
- [x] `P06` 建立 `std.path` 模块并接通 `xrtPath*`
- [x] `P07` 建立 `std.fs` 模块并接通 `xrtFile*`
- [x] `P08` 建立 `std.str` 第一批字符串工具
- [x] `P09` 建立 `std.process` 最小进程能力
- [x] `P10` 清理与 `xrt` 重叠的旧 runtime helper
- [x] `P11` 补 `xrt` 接管后的 C 产物检查用例
- [x] `P12` 补标准库样例进入统一 runner
- [x] `P13` 完成一轮 `test + regress` 基线更新

## 5. 推荐顺序

1. 先做 `P01-P04`
2. 再做 `P05-P07`
3. 接着做 `P08-P09`
4. 最后做 `P10-P13`

## 6. 第一批标准库目标

- `std.io`
  - `print`
  - `println`
- `std.time`
  - 当前最小能力迁到 `xrtNow/xrtTimeToStr` 路径
- `std.path`
  - `join`
  - `name`
  - `ext`
  - `dir`
  - `is_abs`
- `std.fs`
  - `exists`
  - `read_all`
  - `write_all`
  - `append_all`
  - `mkdir_all`
- `std.str`
  - `contains`
  - `starts_with`
  - `ends_with`

## 7. 当前阶段说明

- 当前先从 `print/println` 切入，是因为这条路径已经存在、最容易验证，也最适合用来把生成 C 接到 `xrt`。
- `std.core` 中现有输出能力暂时保留兼容；第二阶段以 `std.io` 作为正式标准库入口。
