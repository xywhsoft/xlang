# 真实产品与独立程序的标准库验收

本批把系统与进程、文本与 Unicode、容器三组的现有夹具，放入普通构建产出的
CLI、运行库和编译器库中验收。JIT／模块与独立 AOT 分别执行，不用源码驱动
成功代替产品链接或退出清理。标准库公开面没有增加，原有安全数值转换合同不变。

## 普通构建与声明目录

Windows 普通 DLL 构建补齐 ProcessFuture 拥有域与 TaskGroup 异步等待的两个
翻译单元，并纳入原生 Future 载荷策略的新拥有域。每个共享产品使用明确的
已编译对象清单，不用旧目录中的对象或文件名通配符补足缺失实现。

构建前检查以驻留组件注册表为准，核对 Windows runtime／compiler 和 Linux
runtime／compiler 四个源闭包。单独检查缺失、重复和未知原生特性声明；
`MEMORY`／`ERROR` 是始终存在的基础能力旧标签，不作为可选实现模块。

IO 的内部静态函数从 `xl6_io_private.h` 改为 `.inc`，仅由实现单元包含。
私有 SDK 仍只打包声明头；编译器源 VFS 则继续携带内部 `.inc`。没有把整份
实现放回 SDK，也没有给生成程序增加源码目录回退。

## 原生 Future 和 signal 的精确依赖

不可变原生叶值 Future 的载荷策略与普通 xvalue 结果不同。新增独立能力域，
按实际 `NATIVE_FUTURE_PAYLOAD` 提供方登记同一个驻留策略身份，物理 Future
仍由共同 NativeFuture 域准备和退役。未知载荷保持拒绝，不按叶子兜底。

原 SlotMapKey Future、task、nullable 捕获夹具的断言不变；修复的是退出清理
缺失的策略声明。没有增加 Cgen 特例、按生成 C 猜测依赖或整库回退。

signal 的 TaskGroup 桥接调用 `xrtValueGetFuture`，实际实现属于
`XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE`，不是普通 XRT Value 模块。
声明目录现在选择准确的提供方，JIT 和独立程序使用同一合同。

## Array 入口与 checked 清理

宿主入口接受旧 boxed Array 和经过精确驻留角色验证的原生 Array；不把任意
Handle 当作数组。生成入口继续负责元素合同与必要导入。错误类型返回 checked
失败及退出码 1，不能留下之前调用的成功状态。

`stdlib_array_entry_values` 必须由专用原生宿主提供 NULL、含 NUL 的字符串、
bytes、null 和原生 Array。操作系统 argv 包含程序名，无法携带任意二进制值，
不能替代该宿主。独立 argv 夹具另检查空参数、中文、emoji、引号和带空格路径。

clear 的完整内存账本在模块／上下文销毁并成功执行
`xlRuntimeDrainRetiringChecked` 后检查。异步退役尚未结束不等于泄漏，也不能在
检查前强行释放代码。原有析构、首错、finally、续体和完整内存断言保持不变。

## 双平台产品结果

两端从同一份选定源码重新构建 TCC、XRT、CLI、运行库、编译器库和两份嵌入
VFS，不复用旧产品或替换原生提供方。正式工作区中的发布二进制未覆盖。

| 验收 | Windows | Linux |
|---|---|---|
| 普通产品构建 | 通过 | 通过 |
| JIT／模块产品总门 | 627 阶段，8 个子门通过 | 628 阶段，9 个子门通过 |
| 独立迁移 AOT 清单 | 908 阶段，311 个候选，0 个意外失败 | 908 阶段，311 个候选，0 个意外失败 |
| 原生 Future 精确策略宿主 | 两个 DLL，O0／O2，4 项证明 | 两个 SO，O0／O2，4 项证明 |

JIT／模块清单每端分别记录 303 项普通通过、1 项预期拒绝、6 项需专用宿主和
1 项平台条件。独立 AOT 每端记录 302 项普通通过、1 项预期拒绝、7 项需专用
宿主和 1 项平台条件；这些状态没有被改写成普通通过。

专用门另覆盖真实对象图、clear 的错误注入与续体、Array 的显式二进制参数、
线程、同步及独立循环对象图退出，Linux 另有 ELF 门。原生 Future 宿主验证缺失
策略时拒绝、准确登记后准入、冻结探测不分配和完整生命周期内存平衡。

Linux 目录单元另执行完整创建故障前缀：26 个预算点，以及忽略载入失败的
4 个预算点；O0／O2、两个产品均通过。它只证明调用线程上的目录合同，
不代表所有工作线程的故障注入已完成。

构建源清单四个闭包各提供 422／422 个驻留组件。最新检查器另通过五个拒绝
对照，包括漏编、重复提供方及未知原生特性声明。构建后的选定源码增量仅为
文档与该检查器的增强，C、声明头和模块元数据保持与测试产品相同。

Windows 的 IANA DST 夹具仍需要适用的原生时区宿主；Linux 不执行旧的
cmd／findstr／more 夹具，已另执行可移植进程夹具。没有修改系统时区，也没有
把这两项记为运行通过。

## 证据与后续范围

选定源码及构建输入前后指纹一致，子报告、日志和提供方身份复核通过。
主工作区中无关扩展库、IDE、API 叠加和构建响应模板修改未纳入本批提交。
初始构建和 AOT 失败报告仍保留，没有覆盖成通过结果。

- Windows：`D:/xlang-validation/stdlib53-b324-build-final3-win/build.shipping.json`、
  `stdlib53-b324-products-final3-win/results.json`、`stdlib53-b324-aot-final3-win/results.json`。
- Linux：`/var/tmp/xlang-stdlib53-b324-build-final3-linux/build.shipping.json`、
  `stdlib53-b324-products-final3-linux/results.json`、`stdlib53-b324-aot-final3-linux/results.json`。
- 两端复核摘要：`D:/xlang-validation/stdlib53-b324-dev/certificate-win.json` 和
  `certificate-linux.json`；选择边界见同目录 `commit-selection.json`。

可复测入口为 `build_stdlib_windows_shipping.py`／`build_stdlib_linux_shipping.py`、
`check_stdlib_current_products.py`、`check_stdlib_three_group_aot.py`、
`check_stdlib_array_entry_shipping.py`、`check_stdlib_native_future_policy_shipping.py` 和
`check_stdlib_shipping_sources.py`。每次使用新的输出目录，并保留真实构建报告。

本批关闭现有清单暴露的产品链接、SDK、精确策略和宿主边界问题，不将夹具数
解释为函数或分支覆盖率。完整 53 模块的剩余覆盖、工作线程故障注入、终端
压力稳定性及发布总验收继续；未决策 19 组和对象池最后的顺序不变。
