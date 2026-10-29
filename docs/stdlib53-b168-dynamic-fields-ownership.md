# DynamicFields 原生拥有图与退役合同

DynamicFields 已有完整的原生物理拥有节点和分阶段退役适配器，补上了
Open 恢复迁移所需的底座。xlang 的 DynamicFields handle 仍未切换驻留
合同，Open 恢复及 opaque 模块屏障仍保留。本批不是语言载体切换或发布封板。

## 拥有图与准入

新增低层 `xrtDynamicFieldsOwnership` 和
`xrtDynamicFieldsOwnershipAdapterV1`。节点使用真实对象强引用计数，
每一个字段槽保留一条 Value 外壳边；重复引用、集合 backing 和循环不被
折叠。外部原生对象引用仍算外部根，不估算计数、不增加虚拟节点。

准入先检查精确驻留 Ops，再检查内建类型、对象状态、Value 元素类型、
驻留析构器及 context、默认键策略和存储门禁。相同 TypeId 的结构仿冒、
任意绑定的 Object Trace、原始 C 改装的析构策略及旧 object-only 收集器
节点都拒绝。拒绝前不会调用外国的 Count、Trace 或存储析构回调。

公开动态字段操作、对象强引用增减、弱引用提升和控制块回收参与核心
ownership mutation 域。原始 payload、直接 typed dictionary 和旧 object
graph 操作仍要求调用者覆盖完整转移并串行化；这不是并发容器或通用
Object 自动认证。其他运行时对象没有因为计数参与而获得生命周期准入。

## 清理与体积

Claim 隔离弱引用提升；已有真实强拥有者仍能在提交前复活。Abort 恢复
弱提升并保持字段原样。Clear 只把现有槽标为不可访问的退役拥有，无分配、
析构或回调。Finish 先在 mutation 内移出并清空字典头，再在域外释放。
清理诊断保留首次错误，重试不重复已完成的物理释放。

payload 仍等于 `sizeof(xtypeddict)`；不保留第二张字典头、逐字段退役
数组或全局注册表。对象控制块增加 claim token 和 cleared 状态。语言
宿主后续仍须用 checked 边界捕获无诊断的语言清理失败，不能把原生
void Drop 的无诊断返回当作完整语言成功证书。

未添加 xlang 标准库声明、Cgen 函数体或新的驻留组件。封闭恢复的既有
共享默认子对象规则不变：按声明 clone 隔离，再认证私有图；没有 clone
或复制结果仍然共享则 checked 拒绝，不修改原对象。

## 双端验收

最终原生证据为 `stdlib53-b168-native-win-final4/results.json` 和
`/var/tmp/xlang-stdlib53-b168-native-linux-final4/results.json`。双端各
16 阶段：严格 O0/O2 模块化与单头，非 graph Object、17 项相邻原生
回归、完整拥有图发现 OOM 前缀、零分配 claim/clear、平衡分配账本，
以及设置字段、强保留、弱提升和释放的实际 native thread freeze 排斥。
Linux 所选原生闭包 O0/O2 全部使用 ASan/UBSan，不代表整个语言运行时
已 sanitizer 验收。

五个真实实现变异分别删除字段边、篡改计数、放开弱提升、放开 Clear 后
强保留、放开外国存储析构策略。每项都必须编译成功并被原合同拒绝。
早期 final1/final2 原生人口和 final3 测试失败不作为最终证据。

xlang 最终源码为 `stdlib53-b168-source-final2/frozen/demo6` 和 Linux
对应 `demo6` 镜像。完整编译器源构建分别为 254、253 个翻译单元；
双端 `stdlib53-b168-language-*-final2/results.json` 各 59 阶段，包含
五个恢复用例的严格生成 C O0/O2、平衡账本、真实 TCC JIT 和 IR 拒绝
变异。五份生成 C 及其重复输出与上一批逐字节一致。

只复用既有 TCC 工具核心；不宣称本批重建发布 DLL/SO、VFS、AOT
裁剪产物或完整语言 OOM 前缀。已有独立文档索引的旧引用问题见
[前批发布限制](stdlib53-b167-resident-catalog.md)，不能外推为全仓 RC0。

## 下一步

迁移 DynamicFields 的生成 handle 适配为驻留合同，接通宿主认证及 checked
清理，再为 Open 增加真实动态字段的恢复事务节点。只有实际拥有图及语言
恢复通过后才撤除对应 IR 屏障；未知资源继续拒绝，不降为叶节点。
