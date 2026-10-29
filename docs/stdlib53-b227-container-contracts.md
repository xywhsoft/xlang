# 隐式容器类型修复与剩余核心缺口

擦除到 `var` 的 Set 字面量现在先创建真实的原生 Set，再完成值擦除。
修复不依赖另一个显式 Set 声明，不增加 Cgen 特判或 wire 容器回退。
容器批量发现仍显示 union 元素工厂和原生 Dict 点访问存在核心缺口；
三组接入、53 模块及发布总验收继续，不因本专项通过而关闭。

## 规范生产类型

原 typed 类型池注册了隐式 var／Array／List／Dict，却遗漏 Set。
`var items = set{1, 2}` 因而无法取得完整的默认 Set 配方，多个已存在
的动态 Set 入口不能 lowering。现在以一个封闭词汇表统一注册五类
生产类型，在源类型之后、派生构造配方之前闭合。源可见 TypeId 的
顺序不变，后端不根据声明是否偶然出现来补充类型。

新用例只使用 `var` Set 字面量，覆盖空容器、混合基础值、UTF-8、
内嵌 NUL、任意 bytes 及插入后继续修改的对象身份键。原生合同
核对默认 Set 的拥有槽、键规则与精确身份；六种破坏必须在输出
C／header 前被拒绝。单纯注册隐式类型不选入 Set 运行时，未使用
它的程序保留裁剪。

## 测试合同修正

动态容器的既定边界是缺失 map 键返回 null／false，空 Array pop
及越界 take 抛 RangeError。旧夹具错误地期待后两者返回 null；
现在只消费预期 RangeError，其他异常原样重抛，MemoryError 不被
测试吞掉。没有修改生产运行时的错误语义。

测试侧车的 `expect_i64` 改为精确 `int64_t`，不再把 Linux LP64 的
`long` 与 `long long` 当作同一个 C 类型。字符串侧车仍是显式
C 字符串互操，不能用于证明内嵌 NUL；相关精确字节行为由语言
断言验证。生命周期用例继续链接真实 weak 观察侧车，没有删掉观察。

## 验收范围

`check_stdlib_container_contract_batch.py` 是明确标记的七用例专项，
双端各 120 阶段，包含完整源码／SDK 比较、严格 C O0／O2、真实
TCC JIT、七份独立 AOT／导入表、十四个完整同步分配前缀 lane、
确定性 C 以及真实裁剪计划。测试侧车独立编译，只在需要时链接。

完整源码驱动为 Windows 304、Linux 303 编译单元，不替换对象或
回退到旧发布编译器。隐式类型闭包影响全模块，因此同时复测既有
四格式持久化整批，而不是只验证新 Set 用例。提交前再次核对实际
输入、日志和产物；这些结果不是 shipping DLL／SDK 验收。

证据在 `E:/xlang-validation/stdlib53-b227-check-win-dev3`、
`/var/tmp/xlang-stdlib53-b227-check-linux-dev3`；全持久化回归在同一
验证根的 `stdlib53-b227-persistence-*-dev3`，先前失败目录保留。

## 下一批语言核心工作

批量发现覆盖 32 个原有及新增语言用例。未加载侧车产生的链接
失败、刻意拒绝的 Set 二元迭代用例，与以下运行缺口严格分开：

- `array<union>` 尚无完整 native Sequence 配方，两个对象／函数
  容器用例在 union 构造处被拒绝。应建立准确的 trivial union
  值槽与复制／投影合同，不能回退为不透明 wire Array。
- `var` 原生 Dict 的点访问仍进入只接收 Object 的借用字段路径。
  Dict 原生读取需要拥有结果，不能通过 TLS 临时借用或隐藏泄漏
  伪装成旧 ABI。后续统一命名读写与索引的常驻路由，保留已知类
  布局的专用合同和按需组件。

33 条静态诊断仍待逐组验证，不等于 33 个运行缺陷。部分门仍要求
已退役的内联 Cgen 算法或使用 released CLI；后续迁移必须保留真实
语言、IR 损坏拒绝、生命周期与独立产物证明。容器成对迭代、拥有
值消费、WaitQueue 拓扑／backend、并发压力和发布总门继续。
没有新增未决策 API 或修改供应方 XRT；CRT 0、XRT 19 组未决策
及对象池最后的顺序不变。
