# 排队 Future 回调门禁迁移

本批只修改测试与驱动。语言回调继续采用当前 V5 排队合同；显式原生 V1—V4
inline 入口的原有专项人口也保留，没有让 Cgen 输出旧 transport。

门禁现在识别实际合同程序已执行的全部 235 个 C／头文件拒绝变异，覆盖
15 个 Future continuation、10 个 task 及已退役 transport／cleanup／error ABI。
驻留依赖来自实际四份 fixture IR 与显式原生根的规范闭包，GCC／TCC 使用
真实响应文件和退出码。默认仍执行 9 程序 × 3 编译器；可选诊断子集在报告
中明确标为 `FullPopulation=false`，不能替代完整验收。

两个旧程序的合同更新：

- 两次 finally 的拥有图使用当前目录、V6 准入及队列／执行器／Watch 策略。
  无法 freeze 的真实过渡返回 BUSY 并有界重试；未知身份仍拒绝。仍要求
  先完成两次回调、再执行类 finalizer、最后 clear／finish；外部结果 alias
  仍阻止提前回收。测试宿主的引用和事件计数改为原子操作，完整生命周期
  另包含目录本身的分配／释放平衡。
- 生成回调图仍精确要求 11 个节点、14 条边；外部根为 3，包括 source／
  output observer 和一个真实的已接纳队列执行引用。先取得静止读边界；
  终态发布后等待物理 release 完成，再检查捕获释放及内存余额。

该 fixture 的 `extern string` 仍按明确的 C 字符串 ABI 调用 `const char*`；
通过检查生成声明和 `.Data` 实参确认，没有将它改成双字段外部参数。
测试的 preparation 入口改用当前 V4 preflight。

完整 `future-callback-workers-r14` 全部 27 配置、3 个 DLL 构建、6 次合同
发射通过，重复 C／头文件／IR 一致。保留 100 个两-finally 图、50 次外部
结果延期、200 次有序回调和 100 次类析构；1000 次注册竞态、真实 DLL
父／子回调、325 项坏布局及原有 OOM／取消／传递释放人口亦通过。

独立核对 6871 个输入、工具、产物及日志哈希：
`F:/xlang-validation/five-groups-20261027/p7-shipping/future-callback-r14.proof.json`。
使用实际新建的 1295-TU R8 完整源码 cohort，未复用源码对象；其 6615 项
独立证明为同目录 `source-win-r8.proof.json`。R8 源码角色仅复用已证明核心
输入一致的 R7 普通 DLL，不将本批称为新一次普通产品构建或全体门禁通过。
