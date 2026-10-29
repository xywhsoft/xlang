# 参数化 Future 门的正式驻留闭包

原先 Wait oracle 和 Aggregate code-owner 门将源码 cohort 中所有驻留单元
一并编译，实际链接到了与测试无关的 xlang Context／XACME。其后果分别是
缺少不属于该测试的宿主 provider，以及触发无关 C 单元的 TCC 初始化器问题。
不能靠假造 `xlDestroyContext` 或重新引入退役 API 来使这些门变绿。

现在 `test/support/xlir_resident_closure.c` 直接查询实际 artifact registry：
公开符号通过同一符号 owner 表选根；为了观察私有 solver 而直接包含的
生产 C 单元以确切 source 根登记。调用正式传递闭包算法，不扫描生成 C，
不按文件名前缀猜测依赖，不维护第二份逐渐膨胀的源文件清单。

Wait oracle 要求 18 个逻辑单元。solver 仍直接编入原有测试文件，错误 TLS
由已审计 `tools/xrt_single_header_impl.c` 提供，另编译 16 个传递单元。
Aggregate code-owner 要求 16 个逻辑单元，错误 TLS 同样由原生组合单元
提供，另编译 15 个单元。组合 provider 必须具有确切、无条件的 include；
不提供宽泛的“符号缺失忽略”或可随意指定的替代 provider。

两个实际测试 C 文件均未修改，原有工作负载全部保留：

- 每配置 66,064 个独立 oracle 图、197,648 个根决策、增长与分配失败检查。
- 每配置 100 个真实 DLL 生命周期、100 个跨线程拒绝、500 次 scoped 调用、
  300 个物理拥有图，以及 128 个 mapper OOM 预算和 200 个精确代码拥有链。

Windows GCC O0／O2／TCC 各三配置全部通过；并未因较小闭包而减少测试场景。
未知根、先合法再未知的根、缺失 C 单元共六次检查均在输出部分闭包前拒绝。
独立 reader 重算 5,400 个输入、对象、日志和产物。首轮错误 TLS 重复定义的
失败记录保留，最终由显式物理组合角色去重，未关闭或模拟错误边界。

证据位于 `F:/xlang-validation/five-groups-20261027/p7-shipping/`：
`wait-oracle-r2`、`aggregate-owner-r2` 和 `resident-guards-proof.json`。
这是两个既存 Windows 门的修复，不代表 74 个参数化门全通过，也不是 Linux
对应测试、发布产品或五组整体完成证明；其他存量阻塞继续保留。
