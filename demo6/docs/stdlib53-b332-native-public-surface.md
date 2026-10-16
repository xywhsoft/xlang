# 原生容器公开面与默认构造验收

本批核对静态标准库登记、受管理类成员与原生容器方法三种接入路径，
修复并发队列构造的语义准入、SlotMap 默认初始化及 Array 区间删除。
不新增容器算法、运行时 ABI 或标准库命名空间。

## 容量与默认构造合同

SpscQueue、MpscQueue、MpmcQueue、WaitQueue 构造必须显式提供一个整数容量，
Queue 别名采用同一规则。省略参数在语义检查阶段拒绝；实际容量非正报
RangeError，不引入默认值或零容量 rendezvous。

不可空类型的 SlotMap 声明与显式空构造使用相同原生槽布局。布局分类不依赖尚未建立的
nominal 准入集合；可执行构造仍须通过完整的闭合配方。局部、全局及类字段
都记录 payload、transfer 和 may-raise 合同，lowering 绑定准入集合。
测试覆盖 int、string、bytes 和类对象，以及两个独立实例的默认字段。

默认资源初始化有自己的清理计划，必须先填完 producer 的资源信息，再建立
checked 错误分支。通用调用的自动分支不能让后续元数据写到清理指令上；
默认初始化现在显式分离 producer 与错误边，普通调用的自动检查保持不变。

Array.remove(index, count) 复用既有 xl6ArrayMethodRemoveV1。单参数调用在
lowering 中补出 count=1；两种写法统一为 receiver/index/count 三操作数。
Typed 准入、验证与 lowering 共用签名合同，Cgen 只输出这三个已确定的值，
不再补默认参数。List、Set、Dict 的删除仍采用原有单参数合同。

## 公开面核对范围

`check_stdlib_intrinsic_coverage.py` 使用真实 Linux compiler SO。它保留原有
静态登记及受管理类核对，增加 Array/List/Set/Dict、Stack、OrderedMap、
SlotMap 和四种并发队列的方法、参数个数及 length 属性核对。

观测器在标准库注入前记录源码节点，再按结构绑定 Typed IR；接收者必须对应
同一源码表达式。注释、方法声明、同名用户方法不计入原生调用。不可达函数
对照单独核对；泛型模板的 dependent admission 不冒充可执行 ABI。
编译旧模式观测器并比较 catalog 和重载对照，保证原有调用清单行为不变。

此核对证明调用绑定及函数可达性，不证明每个运行时分支都已执行；Windows
没有相同的私有 frontend 导出，不以源文件替代产品伪造同一核对结果。

## 产品验收方式

`check_stdlib_native_defaults.py` 通过公开 compiler API，在无 src/lib/res/include
兜底的目录中执行四个程序的 JIT、模块及迁移后 AOT。保存的真实生成 C 和
原生输入另以 GCC O0/O2 严格编译；检查独立程序不导入 xl/xlc，Linux 不带
RPATH/RUNPATH。五个语义负例包含四种队列与 Queue 别名，运行期容量测试
分别验证四种队列的零／负容量。

Linux 另用真实 SO 验证默认初始化、类字段和执行指令的资源合同，及 Array
删除的三操作数合同。破坏后的 IR 必须同时被 verifier、C 和 header 生成拒绝，
且不能发布部分输出。primitive schema 的空准入集合是合法值，不作为坏合同。

默认构造门已纳入双平台产品总门，原生公开面核对纳入 Linux 总门。旧产品
须被同一套门禁拒绝，错误报告与中间失败批次保留，不改写为通过。

## 证据与完成边界

Windows／Linux 新增专项分别通过 42／46 阶段：每端四条完整产品路径、16 个
严格 O0/O2 对象与五个语义负例。Linux 另通过 269 项 SlotMap 及 16 项 Array
删除合同破坏对照，均检查 verifier 和 C/header 的空输出拒绝。

公开面扫描包含 317 个候选程序。224 个登记的 228 项静态参数签名、所核对的
受管理类可调用成员和 142 项容器原生方法／属性全部找到源码调用绑定，
没有仅靠不可达函数补齐的原生签名；旧模式观测器兼容性对照通过。这些数字
不外推为完整运行分支覆盖。

两端完整产品用例清单各 316 项：307 项执行通过、一项预期拒绝、七项专用
宿主需求和一项平台需求。独立迁移 AOT 清单各 316 项：306 项执行通过、
一项预期拒绝、八项专用宿主和一项平台需求。专用宿主路径由对应专项总门
核对，平台条件仍保留：Windows 的 IANA TZ 用例、Linux 的旧 cmd 用例不计通过。

- 新产品构建：`D:/xlang-validation/stdlib53-b332r3-build-win/build.shipping.json`，
  `/var/tmp/xlang-stdlib53-b332r3-build-linux/build.shipping.json`。
- 产品总门：`D:/xlang-validation/stdlib53-b332r4-products-win/results.json`，
  `/var/tmp/xlang-stdlib53-b332r4-products-linux/results.json`。
- 独立 AOT：`D:/xlang-validation/stdlib53-b332r3-aot-win/results.json`，
  `/var/tmp/xlang-stdlib53-b332r3-aot-linux/results.json`。
- 旧产物拒绝：`D:/xlang-validation/stdlib53-b332-original-r3-win/results.json`，
  `/var/tmp/xlang-stdlib53-b332-original-r3-linux/results.json`。

首轮 Windows 产品总门的终端断言失败保留在 r3 报告：ConPTY 的 VT 重绘可将
`reply:hello` 等打印文本拆开，不能按普通管道的连续字节判断显示结果。
子进程本来就独立检查真实尺寸、完整输入与三个 tty，并以非零退出码拒绝错误；
语言和原生宿主现在使用这一机器合同，不依赖 size/reply 打印串连续出现。
missing/wrong resize 对照必须因子进程失败而被拒绝，不添加 sleep 或重试，
不改变系统 ConPTY 后端或应用就绪要求。

r4 重新执行完整语言清单及两个终端专项；其他专项仅在同一实际产品及其
全部已记录输入、产物、日志指纹复核不变时复用，并标记 Reused。失败或变化的
专项不得复用；r3 的失败记录不被覆盖，也不作为通过证据。
Windows 14 个、Linux 16 个必需门全部通过，包括重跑后的终端重复检查与
原生资源账本；缺失／错误尺寸对照仍被拒绝。

本批不增加默认初始化的完整 OOM 前缀或全部泛型元素组合覆盖声明。发布条件、
公开面运行分支及其余模块的最终审计继续；CRT 0、XRT 19 组未决策及对象池
最后的安排不变。
