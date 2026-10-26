# 参数化总门的 driver 角色修正：2026-10-26

本批只修复验证工具接线，不改变语言、Cgen、驻留运行时、XRT 或发布产物。
完整参数化场景仍须实际运行；74 个参数记录通过不等于 74 个 worker 通过。

## 缺陷和修复

生产总门将 `PublicDriver` 错误绑定到 `$sourceDriver`，后者有意指向
`xlir_code_owner_binding_contract.exe`，是另一类合同程序。两个 C-owned
worker 根据 public driver 所在目录读取完整 public cohort 的 `build.json`，
因此这两个角色不能复用同一可执行文件。

- `PublicDriver` 改为真实 public cohort 下的 `public_source.exe`。
- `SourceDriver` 仍使用原 code-owner 合同程序，不改变它的合同含义。
- `Compiler`／`ContractDriver` 继续采用逐项 reviewed 映射。
- 新增轻量参数守卫，在源码构建前检查生产 runner 的实际元数据，不另建
  一份 worker 或参数清单，不执行 worker／原生回调／网络操作。

守卫只读取生产 AST 中两项元数据声明和两项参数构造函数，使用互不相同的
public／runtime／contract 路径实例化全部记录。它检查必需参数、driver
角色、独立输出资源和输入稳定性；两个已知错误一起收集，不只报告第一个。
新脚本的参数均可省略，不增加第 75 个必需参数 worker。

## 已完成验证

PS5 与 PS7 各运行三种真实进程，共六次，并核对实际退出码、日志、报告和
输入哈希：

1. 当前映射：74 个实际 worker，194 项必需参数／角色检查，通过。
2. 冻结旧映射：准确拒绝 boundary／globals 两个错误角色，不执行 worker。
3. 仅回退这一行的隔离副本：调用真实生产总门，准确拒绝同样两处错误，
   `PhaseReports` 为空且没有创建源码构建目录，确认检查确实位于构建前。

第三项未复制编译器／运行时源或程序，不用替代 provider 制造构建成功。
控制器首轮 PS5 模块搜索路径错误保留在原日志，修正子进程模块目录后重验；
不将该基础设施失败算成通过。

证据：`F:/xlang-validation/five-groups-20261026/p7-parameterized/`，最终回执
`verified-argument-admission-r2.json`；原始六次进程在 `binding-accept-r2/`。

随后仅改变错误的 PublicDriver 和独立输出目录，复用同一冻结源码及完整
public cohort，原两个 worker 实际重验均退出 0：boundary 在 O0／O2 各有
296 次注入失败、1,624 次成功且账本平衡；globals 32 步中 4 次注入失败、
28 次成功，全部 C 分配释放。原首批两处失败保留；不重跑未变的源码构建。
定点证据在 `corrected-c-owned-r1/`，独立复核回执为
`verified-driver-roles-r1.json`。

## 完整场景边界

同批已启动独立冻结源码的完整 Windows 参数化运行：9,776 个 xlang 文件、
862 个独立 XRT 文件及三个普通产品依赖均有哈希记录；不覆盖工作区 release。
两个各 1,275 TU 的 public／runtime 变体和 37 个合同程序已完成构建，
使用 1,274 个真实源对象归档，已进入首阶段 73-worker 批次。

该运行使用修复前的不可变 runner，继续收集全部失败，不在运行中修改输入。
`run-r1/results.json` 及进程终态才决定完整结果；本文件不宣称其已通过，
也不宣称 Windows 专用脚本已在 Linux 全量执行。两个 C-owned 的定点通过
也不抵消原完整报告的失败，其他 worker 和依赖阶段继续逐项处理。

更早的 [合同 cohort 报告](contract-cohort-2026-10-26.md) 中，三个 worker
曾通过单独构造的参数运行；这没有证明生产总门的 `PublicDriver` 接线，
本次修正该缺陷并明确两类证据的区别。完整联合／IDE／嵌套门仍需继续。
未决策 CRT 0／XRT 11，内存池最后。
