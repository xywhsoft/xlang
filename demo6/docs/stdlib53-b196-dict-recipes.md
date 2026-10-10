# Dict 统一构造配方与嵌套准入

更新日期：2026-10-11。Dict 的原生目标构造进入统一的 List、Set、Dict
值域抽象；语言 Dict 工厂、方法配方及 JSON/XLON 恢复仍需继续迁移。
本专项不改变 DEV-XRT-53 的完整目标、19 个未决策组或对象池最后的顺序。

## 声明合同

`XL6_IrValueCollectionRecipe` 统一承载 owner、规范 Array 值域见证、
元素、槽、转换、转移和名义准入集合。Array 只描述完整值域，不作为
List、Set、Dict 的存储。Dict 新增独立的 `EXACT_STRING_VIEW_V1` 键合同：
键使用 Data 和 Size，允许空键、内嵌 NUL 和 UTF-8 字节；不调用 strlen，
不进行 locale 比较或隐式规范化。Set 的键合同保持独立：基础值按内容，
可变对象与容器按引用身份，Future 按原生控制块身份。

类型缓存、完整配方与模块 verifier 都从规范类型结构重新推导。
缺失或错误的 schema、键合同、槽、codec、转移、默认值、子域、名义
集合及循环类型不能以 ANY 回退。拒绝时配方输出保持不变，C、头文件
和 artifact plan 不发布半成品。

## 原生目标和嵌套准入

Cgen 使用封闭的声明表选择 List、Set、Dict 的原生 layout、构造、投影
和子域工厂；未知 family 不默认为 Set。生成代码只安装 checked 边界、
传递声明数据并调用驻留 provider，不包含哈希表、容器算法或 wire 适配实现。

`xl6SequenceDictCreateTakeV1` 只接受具有私有来源的原生 Dict，并消费输入
的两个结果路径。它保存完整不可变 layout/policy，不保存原始子对象或
存储。准入比较完整的逻辑策略，而非只比较 ID、物理槽或错误 retag。
Array、Stack、List、Set、Dict、Tree、Queue 共用有限递归预算；驻留
混合嵌套在深度 64 接受，65 拒绝。nullable 只改变 absence，不弱化子域。
入口拒绝没有诊断时也必须传播失败，清理不得制造成功或覆盖首个错误。

新增可选 DICT_SCHEMA 组件与一个导出。精确符号访问者、needs、声明头、
名义身份闭包、artifact 依赖及源码构建清单同步更新。仅出现未使用的类型
行不会选择 Dict 构造；原生 Dict schema 不隐式链接 List/Set 算法。

## 验证入口

- `tools/check_stdlib_dict_recipes.py`：实际 AST、语义分析、XLIR 和 Cgen；
  完整类型配方、变异拒绝、真实生成目标及未使用工厂裁剪。目标 C 使用
  新构建的完整编译器对象；这一部分不充当按需 shipping 闭包证明。
- `tools/check_stdlib_dict_schema.py`：真实 XRT typed_dict 和 typed_list，
  标量、精确文本、引用、Future 子域，伪造及弱化策略拒绝，nullable、
  混合深度、所有分配失败前缀、入口及清理拒绝，实际错误实现对照。
- Set 配方、原生消费者和语言专项，以及 List、动态字段、迭代器与宿主
  resolver 作为邻接回归；完整驻留目录核对实际导出与选择闭包。

## 专项结果

Windows 和 Linux 均从隔离源码构建完整 compiler/runtime，分别为 292
和 291 个编译单元。Dict 目标验证各 69 个完整配方、1380 次合同变异拒绝、
12 次子域变异、35 组同槽不同逻辑域、一个默认 erased 域和三个未支持域。
真实目标 C 对应 78 次 Dict 工厂调用；O0/O2 各完成 624 次分配前缀尝试
和 86 次无诊断入口前缀尝试，直至第一个未命中点，内存台账平衡。

原生嵌套 schema 在两端 O0/O2 各完成五条完整前缀、454 次分配尝试，
三种实际错误 provider 均被拒绝；Linux 对该所选闭包的 ASan/UBSan
通过。Set 目标邻接回归保留 68 个配方和 1428 次合同变异拒绝。

## 尚未完成的范围

语言 Dict 工厂和方法必须在后续迁移中显式获得生产者 authority，不能
因本专项的原生 target 配方完整就把旧 wire Dict 当作原生 Dict。
Dict 导入链、持久化恢复、完整资源策略、性能和发布验收继续。
严格 O0/O2 的目标执行不代表所有编译器对象采用 O2；所选原生闭包
的 Linux ASan/UBSan 不代表整个动态库或编译器已经被 instrument。
既有静态文本 guard 和文档 guard 的历史失败必须单独记录，不能称为全仓通过。
