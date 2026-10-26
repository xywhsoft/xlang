# 参数化源码构建的 GCC 选择

同一个 GCC 目录以斜杠和反斜杠两种形式重复出现在进程 PATH 时，
PowerShell 的应用发现可能返回多个结果。旧 helper 将 `.Source` 数组当成
一个可执行文件路径，导致源码构建在查询 GCC 的 cc1/as 之前失败。

现在明确选择 PATH 顺序中的第一个实际应用，编译器伴随程序的应用查找
也采用相同规则。选择后的路径仍参与完整工具链哈希；没有拼接、猜测
另一个 GCC，也没有更改用户保存的 PATH 或普通产品源码。

`check_c_tool_selection.ps1` 在独立进程中使用真实重复 PATH：
Windows PowerShell 5.1 和 PowerShell 7 均复现旧实现的路径拼接失败；
新实现均完成实际 GCC 编译。冷构建不命中缓存，相同输入命中并保持对象
字节一致，修改被包含的头文件后则必须真正重编译，预处理和对象哈希均改变。
旧控制与新控制四个回执全部通过，测试最后恢复原进程 PATH。

证据位于 `E:/xlang-validation/five-groups-20261027/p7-shipping/` 下的
`tool-selection-ps5-r3`、`tool-selection-ps7-r3` 和对应 `*-old-r3`。
冻结的常规产品与在途验证保持原样；这是一项构建工具回归证明，
不代表参数化 74-worker 总门或整个长期任务已经完成。
此检查可独立运行，不属于需要共享编译器场景参数的 `check_xlir_*` worker；
单独命名以保持受审查的 74-worker 清单完整。
两个 PowerShell 版本也分别通过实际清单的 74-worker／194 项参数与角色检查，
该静态检查不代替 worker 的实际执行。
