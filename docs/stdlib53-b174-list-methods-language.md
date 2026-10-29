# List 方法接入与集合异常合同

List 的计数、contains、remove、take、clear、trim 和 trimTo 现在由
同一套 checked 驻留入口执行。Cgen 根据已验证的 XLIR 类型和操作合同
选择入口，sync、async、generator 共用渲染，不实现容器算法。
语言工厂仍产生 IntMap；原生工厂、索引、var 分发、迭代和 List 恢复
尚未联动切换，这一步不代表 List 整体迁移完成。

## 原生边界

驻留方法只接受旧稀疏 wire carrier 或具有精确私有 ops 的原生 List。
family probe 不读取外来 payload，也不代替准入：选中的原生操作仍
验证实际 owner、投影和 BUSY pin。伪造相同 TypeId 的外来 handle、
游标、未绑定投影及正在操作的 owner 不会被当作有效 List。

所有键保留完整 int64。take 在原生槽的投影、身份绑定和准入成功后
才移除键；缺失键返回拥有的 null，remove 返回 false。计数的零、
查询的不相等和缺失都不能掩盖 checked 失败，错误对象无法分配时
仍将失败位传到调用者。私有输出只在成功时发布，先前诊断保持隔离。

contains 按 Value 相等语义查询，不要求查询值属于元素写入域。
原生遍历保持存储 pin，逐槽校验并生成拥有的投影；没有整表 wire
复制。标量投影可能分配，不能据此宣称 contains 已达到零分配性能。
trimTo 的负数及不可表示页数现在产生可捕获的 RangeError。

## XLIR 异常合同

既有 contains、remove 的 intrinsic 及 trimTo 被错误地标成 no-raise。
新的驻留范围错误因此曾跳过 catch。它们现在明确为 may-raise，
相应 runtime operation 使用 foreign-scope 物理错误边界，lowering
在完成操作、释放表达式借用前建立异常边。

集合查询和删除的修正也覆盖 Array、Set、Dict，避免比较回调、
析构或无诊断失败被误当作正常 bool。缺失返回 false 的语义不变。
验证器独立拒绝错误的 no-raise 合同，不由 Cgen 猜测成员拼写。
其他既有错误作用域入口中的 abort 路径不在此批清理范围内。

## 验证

Windows 与 Linux 分别通过 75 阶段方法专项和 9 阶段 continuation
补充：确定性生成 C、严格 O0/O2 执行及账本、真实 TCC JIT、catch
和 finally。原生闭包另在 O0/O2 覆盖字符串内嵌 NUL、uint64、char、
可空字符串、bytes、完整键及拒绝边界。每端 32 个完整故障批次共
检查 22 次分配尝试和 56 次无诊断作用域入口尝试，每点独立核账。
三个编译成功的真实方法实现负对照被拒绝；8 项 no-raise 元数据
损坏在 XLIR 验证阶段拒绝。

实际 artifact plan 验证 List 选择新组件，而 Array/Dict 不包含 List。
双端目录各核对 219 组件、1,195 精确导出及 13 个拒绝负对照。
Closed/Open 恢复回归各通过 92 阶段，本批未重跑语言恢复的完整
OOM 前缀。Linux ASan/UBSan 覆盖所选原生闭包，不代表整个编译器
或全部运行时已 instrument。发布 DLL/SO、VFS、打包 AOT、其余载体
及三组总验收仍分别待完成。

候选源码为 `E:/xlang-validation/stdlib53-b174-source-dev3/frozen/demo6`，
Linux 镜像为 `/var/tmp/xlang-stdlib53-b174-source-dev3/demo6`。
`check_stdlib_list_methods.py --root <frozen-demo6> --public-directory
<fresh-build> --out <fresh-output>` 运行完整专项；Linux 可加 `--sanitize`。
`--continuation-fixture <current-test> --continuation-only` 只补验该显式
源码，不替换编译器或原生实现。后加入的测试及 runner 差异由最终
index provenance 明确登记，不改写冻结输入。

## 后续

继续联动 List 的工厂、索引、动态分发、迭代及恢复，再处理 Set/Dict
和跨组发布总门。共享默认子对象依声明 clone 隔离，否则 checked
拒绝，绝不原地修改外部共享对象。本批没有新增公开能力决策；CRT
无未决策，19 个 XRT 未决策组和对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
