# List 快照迭代与 checked 载体边界

原生 List 的 typed 快照现在可以进入通用语言 foreach 游标。静态 List
与开放 var 共用 Advance、View、Destroy 和拥有图合同，不把整表转换
成 wire。语言 List 工厂仍生成 IntMap；工厂、索引、类型准入、其余
动态分发和 List 恢复尚未联动切换，List 整体迁移仍未完成。

## 驻留入口

Cgen 消费已验证的源类型和逻辑 payloadType，直接选择驻留创建入口。
List 与开放 var 的入口放在独立可裁剪组件；已细化为 Array 的 var
使用基础游标组件的 checked Array 入口，不引入 List。Set、Dict
继续使用精确 wire 家族。生成 C 中旧的标签检查函数已删除，sync、
async、generator 共用同一套操作渲染。

这修复了原生 Array 在 `var` 类型判断后，仍被旧 wire 标签检查拒绝
的问题。运行时以精确私有 ops、实际角色、投影及操作 pin 准入原生
对象；相同 TypeId 的外来 handle、游标和未绑定投影不是有效容器。
这里的回调记录必须是不可变驻留记录，不接受语言或模块所有的回调。

List 快照保留完整 int64 键、元素身份和精确文本长度。原容器清空或
释放不改变已建立的快照。游标分别拥有快照和当前元素投影；View
只是借用，不能增加重复拥有边。失败状态独立于 EOF，失败后的游标
不会在下一步恢复为正常终止。私有快照输出仅在成功时一起发布。

## 验证结果

Windows 从 259 个翻译单元重建编译器，Linux 从 258 个翻译单元重建。
候选源码为 `E:/xlang-validation/stdlib53-b175-source-dev4/frozen/demo6`，
Linux 对应 `/var/tmp/xlang-stdlib53-b175-source-dev4/demo6`。

双端分别通过 64 阶段迭代专项和 65 阶段存量回归。专项包括真实原生
输入调用实际生成函数、严格 C O0/O2、四个真实 TCC JIT 用例、完整
键、内嵌 NUL、快照独立性和 finally。每端 20 个故障批次合计 216
次独立尝试，覆盖完整分配前缀及无诊断失败直到首个未命中点；每次
都单独初始化、清理并核对内存账本。生成函数的无诊断注入位于真实
快照和 Advance 状态边界，不代表全部 OS 失败或任意清理失败覆盖。

三个真实实现负对照均先严格编译成功，再被行为合同拒绝。32 项
XLIR 元数据损坏在 C 和头文件输出前拒绝。实际 artifact plan 确认
List 选择新组件，而含细化 var 的 Array 用例不包含 List 组件。
双端驻留目录各通过 220 组件、1,202 导出及 13 个拒绝负对照。

同源 List 方法及 continuation 回归各通过 84 阶段，Closed/Open
恢复回归各通过 92 阶段。本批未重跑恢复的完整语言 OOM 前缀。
Linux ASan/UBSan 只覆盖所选原生闭包，不代表整个编译器或生成程序
已 instrument。源码驱动测试不认证发布 DLL/SO、VFS 或打包 AOT。

## 重跑入口

`check_stdlib_list_iterator.py --root <frozen-demo6> --public-directory
<fresh-build> --out <fresh-output>` 运行完整专项；Linux 可加 `--sanitize`。
省略 public-directory 只验证原生闭包，不能作为语言接入验收。

`check_stdlib_list_iterator_regression.py` 使用相同三个必需参数，运行
八个存量夹具的确定性 C、精确入口选择和 O0/O2 内存账本。它使用
int64_t 形式的原有 extern 断言 ABI，避免 Linux long 与 long long
混用。最终 runner 修正的只是测试调用数，源码冻结与最终 index 的
差异由 provenance 明确登记，不替换编译器或重写冻结源文件。

## 仍待完成

本批验收时，`var` 判断为 List 后的双变量遍历仍被 XLIR 拒绝：语义
键为 i64，动态循环合同却要求 var 键。后续的
[细化键与 List 动态调用](stdlib53-b176-refined-list-consumers.md) 已关闭
这一缺口，原待修夹具由实际运行的正向及损坏 IR 用例替代。这里的
65 阶段是历史批次结果，不代表后续回归仍应拒绝该语言形状。

接下来完成 List 工厂、索引、类型与动态分发、恢复，再继续 Set/Dict
及三组发布总验收。共享默认对象仍按声明 clone 隔离，否则 checked
拒绝，不修改外部共享原对象。本批没有新增公开能力决策；CRT 无
未决策，19 个 XRT 未决策组和对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
