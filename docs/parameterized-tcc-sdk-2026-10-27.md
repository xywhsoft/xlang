# 参数化验收的独立 TCC SDK

完整总门把源码构建的 `tcc_bootstrap.exe` 直接交给各个场景。
bootstrap 故意没有内嵌 SDK，附近也没有头文件及导入库，因而多个场景
在 `<command line>` 的 `tccdefs.h` 处失败，尚未进入真正的被测代码。

总门现在先准备一次私有 standalone 布局，再把它的实际编译器路径交给所有
需要 TCC 的 worker。编译器逐字节复制，保留所选程序的相邻 DLL 和可选 manifest；
Windows 头和库来自当前冻结项目的 `res/tcc/win64/x86_64`，编译器标准头
来自当前项目 `lib/tcc/include`，优先级与正常源码构建相同。
不回退到另一安装版本或环境头路径。清单绑定全部输入和复制后的哈希，
禁止覆写旧目录，所有 worker 的测试、链接和运行仍会重新执行。

永久 `check_tcc_distribution.py` 在带空格的独立目录实际编译并执行 C11 原子、
Windows SDK 常量及 CRT 程序；检查 117 个实际部署文件、输入不变，
并拒绝已有输出、缺编译器、缺 SDK 三类错误，后两者不创建半成品目录。
PowerShell 5/7 的参数检查继续为 74 个 worker、194 个必要参数/角色检查。

此外，原有 aggregate code-owner 场景使用同一普通 r5 源码集合和这个精确
TCC 二进制/SDK 重新执行：GCC O0/O2 与 TCC 均通过，真实 DLL 父子拥有链、
关闭后调用、跨线程准入拒绝、物理图、OOM 和卸载的原断言保持不变。
这直接修复该场景原先的 TCC 头文件失败，不能据此宣称其他 worker 均通过。

证据位于 `E:/xlang-validation/five-groups-20261027/p7-shipping/`：
`tcc-distribution-gate-r1/results.json`、`aggregate-code-owner-sdk-r1/results.json`；
独立复核位于 F 盘同名根的 `tcc-distribution.proof.json`。
完整 74 项总门及最终常规产品仍待集中复测，本批不改变语言或运行时 ABI。
