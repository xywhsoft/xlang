# 参数化守卫与统一合同编译 cohort：2026-10-26

状态：工具收口已在新的冻结源码快照中重验通过。先前并行 Git 合并改变
编译器输入时，旧 cohort 被哈希守卫正确拒绝；该失败保留，不计为通过。
本次结果绑定冻结快照和工具字节，不宣称持续变化的整个工作区已通过。

本批补齐三个既有 C-owned worker 的总门注册，并移除合同构建器的第二套
不完整编译器源清单。它是测试前置整合，不修改语言、Cgen、runtime 或 XRT。

## 不能只放开库存检查

原参数化库存拒绝三个未登记脚本。逐项核对后，显式登记其必需参数：

| worker | 实际依赖 |
| --- | --- |
| `check_xlir_c_owned_boundary_fault.ps1` | 当前完整 public driver＋C-owned IR 合同编译器 |
| `check_xlir_c_owned_globals_fault.ps1` | 同上 |
| `check_xlir_c_owned_exports.ps1` | 当前完整 public cohort，自建其导出合同宿主 |

Compiler 使用明确的 reviewed 映射，不猜测可执行文件名；PublicDriver 指向
同一实际 public driver；各 worker 保留独立输出目录。PS5／PS7 的实际
库存均为 74 项，无遗漏或重复。此处不等于 74 项全部执行通过。

## 统一构建底座

实际调用原总门构建助手时，旧 22 文件清单缺少当前 Cgen 的 component
owner 和 completion 实现，无法链接。继续扩大另一套清单还会与实际编译器
依赖漂移，因此改用已完成的同一完整源码 cohort，核对所有输入、部署头、
对象和 driver 哈希，再用正常静态归档按符号提取所需实现。

- 不替换编译对象，不提供 stub、不改合同断言，也不解析生成 C 猜语义。
- 参数化总门复用它已经构建的 public cohort，不重复编译一套部分前端。
- 独立默认调用没有 cohort 时，按原 public builder 构建完整当前源码；
  单项和批量都走相同归档／链接／输入复核路径。
- 保留 `Objects` 的 compiler-object 含义，新增独立 `DriverObjects`；
  归档、驱动及实际 DLL 部署分别记录，现有合同消费者仍能核对其依赖。
- DLL 按实际目标目录部署；相同内容只读复用，不覆盖不同内容的既有 DLL。
  单项首次执行发现的部署遗漏已修正，失败证据保留。
- 不改变公共对象缓存策略，不通过放开未知 flag 或忽略输入漂移获得通过。

## 验证边界

最终冻结 O0 的 1,273 TU public cohort 中，排除 public main 后的 1,272 个真实
对象构成归档；原生产 runner 的全部 37 个合同程序均成功构建。三项新登记
worker 使用这条实际构建路径得到的合同编译器执行，而非另建替代工具：

- callable 边界在 O0／O2 各覆盖 296 次注入失败、1,624 次成功，分配账本平衡；
- 全局初始化 32 步：4 次注入失败、28 次成功，C 分配均释放；
- 导出边界 13 项检查通过。

此前默认独立路径另完成完整 O0 源码构建；最终冻结快照再次验证单项程序
实际执行及 DLL 部署。早期 O2 的 1,272 TU 基线仅作为阶段证据，不混计。
六种非法输入／输出条件拒绝，包括未完成／runtime-only cohort、输入哈希
变化、已存在目标、不同内容 DLL 和缺失 driver，隔离标记文件不被覆盖。
测试控制器首轮模块搜索路径、数组展开，以及旧部分构建尝试的失败均保留。

证据位于 `F:/xlang-validation/five-groups-20261026/p7-tls-close/`，包含原失败、
多轮 cohort、归档、37 程序、实际参数生成与三项 worker、单项部署和拒绝记录。
最终回执为 `verified-current-contracts.json`，关联 `contract-current-source-r3`、
`contract-current-public-r3`、`contract-current-accept-r3` 和
`contract-current-admission-r3`。独立 reader 核对全部输入、对象、归档、日志、
结果与真实计数，并确认两份工作区工具与冻结验收字节一致。

完整 74-worker 场景运行、跨组逐项矩阵、嵌套编译／IDE 与 RC0 条件仍需
接续验收；本批不把注册通过、链接通过或三项通过当成完整总门通过。
未决策 CRT 0／XRT 11，内存池最后。
