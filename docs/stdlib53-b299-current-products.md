# 当前产品构建与安全数值转换验收

Windows CLI／DLL 和 Linux CLI／SO 已从提交源码重新构建，TCC、XRT
及两份内嵌 VFS 均为新产物。三组清单各有 311 项：303 项通过真实
产品 run／module，六项专用宿主完整通过，一项按预期拒绝，另有
一项平台条件保留。数值构造另外通过十个既有用例，覆盖真实 CLI、
模块与独立 AOT。本批不增加语言 API，也不提升仓库发布产品。

## 快照守门修复

首次 Linux 普通构建因能力矩阵过期停止。源码中的直接调用计数已
变为 runtime_call 16、runtime_convert 3，生成文件仍是 15、1。
同步这两行后，原有 Linux 快照检查与普通构建通过；没有修改核心
快照、符号表、API 数目或延期清单，也没有放宽检查。

Windows 的快照守门新增相同的能力矩阵检查。独立专项在正确输入
上通过，并分别拒绝过期矩阵计数、延期清单、符号表和元数据。
Windows 的 Python／PowerShell 两个入口共十个阶段，Linux 五个
阶段；专项只修改自己的验证副本，输入源码保持不变。

Windows 候选构建发生在新增守门之前，保留原构建输入；新增守门
随后独立实际执行。认证逐项比较编译生产输入与最终选择性快照，
并明确保留这一构建脚本差异和旧矩阵计数，不将旧输入改写成新输入。

## 产品运行矩阵

产品消费者目录没有 src、lib、res、include 或 TCC 分发目录；生成
程序使用产品自身内嵌 VFS，没有 provider 或源码回退。原生 GCC
测试宿主仍使用明确的声明头，这不代表生成程序从磁盘取得这些头。

| 验收 | Windows | Linux | 范围 |
| --- | --- | --- | --- |
| 三组父门 | 625 行 | 626 行 | 311 项清单及子门调度，不与子门内部阶段混加 |
| 私有 VFS 头 | 219 个 | 219 个 | 两个真实资源映像中的声明与 sibling 引用闭合 |
| 嵌套编译及 AOT | 32 阶段 | 32 阶段 | CLI、compiler import、内嵌 VFS 和独立消费 |
| 图及 continuation | 279 阶段 | 279 阶段 | 204 模式，实际 Stack／Tree／Queue／Promise |
| clear 错误 | 30 阶段 | 30 阶段 | 原有首错、finally、async 和 generator 断言 |
| Thread 产品 | 67 阶段 | 70 阶段 | 两库 VFS、CLI、独立产物与限定裁剪 |
| Sync checked 恢复 | 26 阶段 | 26 阶段 | 持锁最后引用、宿主恢复和真实产品 |
| 独立 Sync ELF | 不适用 | 28 阶段 | 四个独立程序及非可执行栈 |
| 安全数值构造 | 61 阶段 | 61 阶段 | 十个既有用例的 CLI、模块、独立 AOT 及依赖检查 |

六项专用宿主每端共 3364 次计量生命周期；完整产品／优化／终态／
挂起时机矩阵及五个错误报告反例独立验证。带空格路径的 shell 用例
使用本轮真实 CLI，两端通过，不再把源码驱动缺少产品路径当作缺陷。

## 安全数值合同

静态和动态 c_* 显式构造均允许数值转换，有限溢出返回 RangeError；
浮点转整数先验证范围与有限性，再截断。精确 as 和 typed 读取不
采用这套宽松转换。既有用例同时覆盖特殊浮点、反射令牌拒绝、
泛型实例、原生字段、完整文本长度和 nullable 精确投影。

十个 AOT 程序搬到独立目录后实际执行，Windows 导入表与 Linux
动态依赖逐个检查：不依赖 xl／xlc 产品库，Linux 没有 RPATH／
RUNPATH。这里只验收这十个数值程序，不外推为全部对象图的独立
AOT 退役或运行时插桩验收。

## 证据与剩余工作

构建证据位于 E:/xlang-validation/stdlib53-b299-build-win1 和 Linux
/var/tmp/xlang-stdlib53-b299-build-linux2；产品父门为 products-win1
及 products-linux1，数值专项为 scalar-win2 及 scalar-linux2。
首次 Linux 失败及最初数值运行证据均保留，不覆盖旧报告。

Windows 的 IANA DST 用例仍需要相应原生时区条件验证；Linux 的
旧 cmd／findstr／more 用例不适用，portable process 用例已实际
执行。工作线程及对象图 OOM、真实模块取消映射、独立 AOT 对象图
退役、完整公开函数覆盖和发布总门继续。此前 Stack O2 补充插桩
编译超时仍未完成，Windows ConPTY 全进程句柄增长的系统观察项
也不由本批通过消除。CRT 0、XRT 19 组未决策与对象池最后不变。
