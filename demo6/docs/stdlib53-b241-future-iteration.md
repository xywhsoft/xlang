# Future 元素循环的拥有与清理

SlotMap、Array、List、Dict、Set 的 Future 元素已接通统一循环合同，
nullable Future 能区分空值与正常引用。循环取得独立的 Future 消费者
引用，不复制 Future 状态或结果；清空源容器不会使当前绑定失效。

## 统一 IR 合同

新增 future-copy 和 nullable-future-copy 两个循环元素 ABI。
类型分类复用已有 Future 投影配方，核对完整 payload 类型、拥有类别
和 nullable 表示；不根据源码名称或相同指针宽度猜测类型。
迭代绑定使用 owned-iteration-release，成功进入循环体后拥有自己的
引用，正常推进、continue、break、return、异常和 finally 都复用
既有退出清理。取得 Future 后立即返回的结果也能独立于游标存活。

lowering 使用现有借入 Value 到精确 Future 的投影，并把该拥有值
提交到迭代存储。游标创建、推进和 checked 类型边界的失败不冒充
EOF；投影目标的错误计划在绑定成为活跃拥有者之前确定。
nullable Future 的空值仍是一个已找到的元素，而不是遍历结束。

SlotMap 保留四种已批准的工厂：配对、仅键、仅值、仅计数。
单变量仍遍历完整 SlotMapKey；仅键和计数不捕获值通道。
Future 元素能跨 await；生成器能 yield Future，消费者提前退出时
仍按已有拥有帧和游标合同清理，不取消仍由其他引用持有的源 Future。

生产改动仅涉及三个 IR 文件。没有增加 Cgen 函数体、运行时组件、
导出符号或 XRT／xruntime 算法，也没有增加公开标准库命名空间。
生成 C 复用已有投影、引用获取、普通调用、CFG 和释放操作。

## 验证结果

Windows 317、Linux 316 个完整源码编译单元通过。新专项分别通过
921 和 1061 阶段；每端 438 次故障位置检测，16 组完整前缀覆盖
四个 SlotMap 工厂、推进、键复制、typed 复制和实际分配失败。
注入只作用于调用线程；同步夹具在遍历前等待其内部生产者完成，
不把这些结果称为 executor 线程故障注入。失败后重新执行成功，
模块清理使用既有 checked busy 状态的有限重试，资源按快照核对。

三份完整语言程序覆盖五类容器、三处 nullable Future、源容器清空、
重复 await、内嵌 NUL 字符串、原生键结果、提前返回、continue／break、
yield 与提前关闭、task 异常及嵌套 finally。普通 task 失败仍按已有
合同映射为 AsyncError；本批没有改变 task 与 async 函数的错误映射。
每份程序每个优化等级执行 200 次模块生命周期。

20 个 Future 循环、20 个投影和 260 项实际 IR 篡改通过验证。
错误拥有类别、nullable 模式、错误／资源合同和投影 payload 不能
越过验证器、C／header 输出门及原生计划。两个实际坏编译器实现，
分别把绑定降为平凡值、写入错误 payload，编译和链接后被合同拒绝。

三份完整程序与仅键、仅计数程序分别独立链接 O0／O2，共十个程序，
不链接编译器或 MODULE_ALL。五份原生计划分别选择 39、38、51、34、
29 个供应方，执行与 nm 核对可选工厂裁剪。公共 SlotMap 核心仍含
共享快照事务及小型 values 包装，不宣称该公共单元内逐符号裁剪。
公共 JIT、module、实际 AOT 执行通过；Linux ASan／UBSan 覆盖三份
完整程序各自的所有选定原生供应方。

旧 SlotMap 循环双端各通过 881 阶段、510 次故障位置检测和 786 项
游标合同篡改。相邻回归 Windows 71、Linux 47 阶段保持通过，含旧
泛型／nullable、task／callback、producer／取消竞态及拥有环压力。
常驻目录仍为 278 个组件、1437 个真实导出，13 个拒绝对照通过。

## 后续范围

Channel 的公开切换与旧 Into 清退、WaitQueue backend、独立 borrowed
原生 SSA spill 的完整失败边界、并发队列压力，以及 53 模块与正式
DLL／VFS 发布总验收继续。对象池仍最后，CRT 0、XRT 19 组未决策
范围不变。复测入口为 tools/check_stdlib_future_iteration.py。
