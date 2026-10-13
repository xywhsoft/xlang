# 原生 SSA 跨 await 的独立拥有值取得

async 的独立原生 SSA spill 已接通 C 输出。帧能够按统一 IR 的
`copy-native-value` 合同取得独立拥有值；复制失败进入该 await 已有的
清理与错误 CFG，成功后才发布值并设置初始化位。受活跃拥有帧保护的
借入值仍采用 `borrow-storage`，不额外复制。

本批关闭 async 路径的合同遗漏，不新增标准库公开 API 或语言引用语法。
generator 的独立原生 SSA 复制失败边界、工作线程和对象图 OOM、模块
卸载压力及完整 53 模块函数覆盖仍须继续；对象池仍最后。

## Cgen 合同修复

原 async SSA 支持检查遗漏了 IR 已定义的原生复制 transfer；合法
Lowered IR 因而在 Cgen 被拒绝。SSA 取得的通用失败检查还将所得值
当作指针，与原生结构体及双字段 string 的表示不符。

取得失败判断现与 activation 参数捕获共用同一 transfer 合同：
原生复制使用 checked bool，string 使用 Data 字段，保留型指针使用
空指针状态。Cgen 不按语言类型名猜测，不追溯 AST 修补所有权。
现有失败诊断保留，独立取得成功前不设置帧初始化位。资源回滚由
checked 原生复制、既有清理 CFG 和驻留帧退役共同执行。

## 合法 IR 与语言源码的覆盖范围

普通源码会把不稳定借入值保护到拥有的临时槽。本批同时验证这条
正常路径，以及需要独立取得的闭合 IR 路径。后者将一个无用标量观察
替换为精确的内部 storage address 观察，使贷款分析保守地要求复制；
所有派生机器均从指令重新构建，并通过完整 IR 验证器。

这个测试不增加 source byref，不伪造帧元数据，也不绕过指令或 CFG
生命周期检查。不能据此声称普通源码已覆盖所有可能的独立 spill。
四类损坏帧合同分别被 IR、按需原生计划、C 和声明头输出拒绝。

## 双端执行与失败验收

Windows 完整源码编译包含 326 个单元，Linux 包含 325 个单元。
consumer 树逐项核对编译输入，使用实际按需 XRT 和 xruntime 提供方，
没有替换生产原生实现或用旧 compiler 单对象作为通过证据。

Windows 最终专项通过 325 阶段、48 个完整组合及 20 条故障前缀。
每个组合执行两个完整语言生命周期，覆盖两条 transfer 路径、O0/O2、
ready／真实 pending Watch／Commit，以及成功、拒绝、取消和关闭。
Linux 通过 479 阶段、72 个完整组合及 30 条故障前缀，其中包括所选
生成程序与原生闭包的 ASan/UBSan 组合。

每次正常执行，保护借入路径有两次既有原生复制，独立 spill 路径
恰好多一次；每个优化级别核对此差值。普通生命周期和故障后的重试
核对存活分配、字节数、分配释放计数、非法释放和重复释放。
终态宿主另外核对 use-after-free 计数。

故障前缀包括复制返回 false、false 带错误、取得部分资源后 false、
true 带错误，以及分配失败。checked false 即使诊断对象为空也必须
失败；带 RangeError 的路径必须保留精确首错。复制失败路径执行
一次 finally，清理后重新初始化能正常运行，不能以剩余资源或坏状态
换取错误返回。

三个普通源码夹具另通过 JIT、module 和独立 AOT：原生借入 spill、
SlotMap async keys 和 string async ownership。Linux 未打包源码驱动
显式使用其构建已固定的真实 TCC 支持 archive；这不是 shipping VFS
验收，也不替代发布库重建。ASan/UBSan 不覆盖整个编译器和所有 runtime。

最终证据为 `E:/xlang-validation/stdlib53-b260-ssa-win-final3` 与
`/var/tmp/xlang-stdlib53-b260-ssa-linux-final3`。完整长期目标保持 active，
CRT 无未决策项，XRT 的 19 个未决策组和对象池最后的安排不变。
