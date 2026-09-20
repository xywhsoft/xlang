# XLIR 原生 Future 拥有关系（v229）

本批补齐 TCP Listener／Stream 与 UDP 等待节点的真实引用和结果载荷策略。
修复位于原生运行时，不在 Cgen 中按函数名推断拥有语义。

## 当前已完成的定向验证

- 原先失败的契约发现用例保持不变，在 Windows／Linux 的 GCC、GCC O2、TCC
  六种配置下缺失生产者及载荷策略均归零。
- 新的持久用例核对激活、生产者、取消注册三个独立引用；队列、Promise、
  Watch、Engine 和结果对象使用规范物理身份。
- 逐分配点 OOM、UDP 模式冲突回滚、正常 TCP／UDP 结果、BatchTake、取消与
  完成竞争、活动取消回调与 Unwatch 交接均已覆盖。
- 保留节点跨越 transport／Engine 创建者退出后仍可安全访问；保留内存引用
  不阻止操作性 LiveObjects 归零。
- 六种配置及 Linux ASan／UBSan／泄漏检查通过，内存计数精确平衡。

定向报告：
`E:/xlang-goal-artifacts/xlir_launch_v229_future_tests_focused_handle/results.json`。
契约报告：
`E:/xlang-goal-artifacts/xlir_launch_v229_future_candidate_initial/results.json`。
这些报告不等于扩大回归或整个 XLIR 长期任务验收。

## v229 扩大回归已封存

修复批次的 Windows 348 个程序 × 2 种配置、Linux 274 个程序 × 3 种配置，
合计 1,518 次执行全部通过，零失败、零未执行；七组 XLIR 规划器集成也通过。
封存报告：`E:/xlang-goal-artifacts/xlir_launch_v229_checkpoint/results.json`。
其中 `BatchComplete=true`，但整个长期目标仍为 `Complete=false`。

第一次扩大回归暴露了 UDP 无 Future 裁剪配置的未使用声明，以及缓存测试把
Future 终态误当成生产者尾部已归还。后续重复验证还发现 waiter 统计归零与
实际取包之间的采样窗口。运行时只补齐声明的特性开关；测试改用所属 Worker
的完成屏障，保留缓存命中、队列为空等原有严格断言，并增加放弃 Future 后
内部操作仍然存在的断言。修复后 12 种配置 × 32 次重复验证全部通过。
原始失败、未执行记录和修复后报告分别保留，没有覆写成通过。

## 保留的失败与修正

初版测试错误地要求非阻塞 Prepare 永远立即 READY，已改为有截止时间的 BUSY
重试，仍拒绝 FAILED；另一个断言误把协作取消请求当作必胜终态，已按 API 的
真实契约修正。失败记录完整保留，没有修改运行时来满足错误测试。

Windows TCC 的一次访问异常已在不包含 XRT 的最小 C 程序中复现。反汇编显示，
传递 32 字节结构体后处理比较布尔参数时覆盖了隐式复制目标地址。测试句柄
改用只读指针传递；编译器缺陷本身仍未修复，不得宣布全部语言／FFI 工具链
验收通过。诊断与复现：
`E:/xlang-goal-artifacts/xlir_v229_tcc_argument_diagnosis/results.json`。

## 仍未完成

完整 Engine／服务／传输／Dial／TLS 对象拥有图、回调与父图的收集集成、语言与 FFI
联动、混合生命周期和 DLL 卸载仍需完成。生产头文件、release、默认 Cgen
和旧路径保持原状。对不完整父图继续拒绝检查，不能以空 Trace 冒充完整叶子。

回调绑定参与者的后续工作见《XLIR原生回调收集.md》，不追溯计入 v229 验收。
