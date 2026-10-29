# B145：真实产品的 Tree／Queue 拥有图及 continuation 验收

本批补齐 B135 的循环拥有图与 B133/B134 continuation 的真实产品证据，
不新增 API，不修改 XRT／xruntime／语言生产代码。持锁失去语言引用的
恢复入口仍采用宿主 checked 清理／恢复，不自动解锁、abort 或丢弃资源。

恢复入口位于正式 `xl_sync_retirement.h`：SnapshotChecked、AcquireChecked、
UnlockChecked、RetryChecked 四项 V1 接口。Token 不是地址；Acquire 返回
真正 owned handle，mutex 必须在原取得锁的原生线程解锁，Retry 仅在原生
Unit 成功后移除记录。原失败的 checked module finalizer 还须由宿主重试，
不能把记录修复等同于确认模块清理成功。错误对象分配失败不影响 checked
布尔结果；传入原错误仍保留为首错。

## 验证入口修正

- 两个旧图宿主引用了不存在的 `include/xlc.h`，改用正式 SDK 的同目录
  `xlc.h`／`xrt_decl.h`，避免依赖临时 staging 目录的特殊布局。
- 真实 runtime 产品提供 `xl` 编译 API，不提供 compiler 专用 `xlc` API。
  新 shipping lane 通过 `xlCompileScriptFileToModule` 构造真实模块，runtime
  和 compiler 两份产品分别独立链接；保留旧 XLC 开发验证分支，不伪造 API。
- Shipping JIT 目录没有 src/lib/res/include；tccDllPath 明确为该 lane 的
  产品，tccLibPath 为 NULL，不注册外部头文件或 TCC 发行目录。
- 生成的析构器、对象图 resolver、Tree／Queue provider 没有被替换。Queue
  continuation 的测试钩子仅在实际 Watch／Commit 返回后完成真实 Promise；
  不模拟 Future 通知，不用 sleep 或 readiness race 推测时序。
- 每个运行模式单独收集失败，保持 Tree 12 次、Queue 25 次被测完整生命周期，
  每模式另有两次预热；不减少旧验证强度，不丢弃困难模式。

## 源码与产品边界

本批没有修改生产输入，复用 B144 已普通重建的同源候选产品：

- Windows：`E:/xlang-validation/stdlib53-b144-source-final-dev1/frozen/demo6`，
  `stdlib53-b144-build-win-final-dev1/build.shipping.json`。
- Linux：`/var/tmp/xlang-stdlib53-b144-final-dev1`，
  `stdlib53-b144-build-linux-final-dev1/build.shipping.json`。

新门 `tools/check_stdlib_container_graph_shipping.py` 在运行前逐字节核对上述
构建清单的全部输入、产物、当前测试宿主和夹具，运行后再核对。使用真正
xl/xlc DLL／SO 和 CLI，不链接诊断 object island，不覆盖 resident provider。
新测试文件不是旧产品的编译输入；提交前 index 门必须显式允许两个测试宿主
差异，且要求本轮通过报告中存在与最终 index 一致的当前测试哈希。B144 冻结
早于其 Sequence 发布验证脚本的 --root 修复，该既有 test-only 差异继续引用
B144 最终 publication-win/linux-final-dev2/results.json，并重新核对全部输入；
不放宽生产输入比较，也不把仅重新核对的历史测试算作本轮执行阶段。

## 批量结果

以下报告均 Complete=true、InputsUnchanged=true，Failures 为空，位于
`E:/xlang-validation/`；dev1/dev2 的旧入口失败和运行中输入变化报告保留，
不纳入最终通过结论。

| 最终证据 | 阶段／模式 |
| --- | ---: |
| stdlib53-b145-graphs-win-dev3/results.json | 255／188 |
| stdlib53-b145-graphs-linux-dev3/results.json | 255／188 |
| stdlib53-b145-sync-win-dev1/sync.shipping.json | 26 |
| stdlib53-b145-sync-linux-dev1/sync.shipping.json | 26 |

两端共 562 执行阶段。每端 Tree 64 模式 × 12 = 768 次被测 JIT 生命周期，
Queue 76 × 25 = 1,900 次，continuation 48 × 2 = 96 次激活。合计 5,336 次
被测实际编译／JIT／退役及 192 次 continuation 激活；这不是数千个不同
公开函数，也不将预热周期计为被测周期。包含两份产品、O0/O2 的重复执行。

最终 `stdlib53-b145-index-win-final.json` 与 `index-linux-final.json` 逐项
比较暂存 index 与冻结生产输入，并重新核对构建／测试输入及产物哈希。
本轮两个测试宿主的差异显式列入 test-only-change，不豁免任何生产源、
SDK 头、构建脚本或 VFS 输入。没有发布推广或更新主 release。

## 覆盖与后续边界

- Tree：五类自环／重复引用／Array 环／字段别名／clone owner 图，真实
  nominal／Tree／clone 外部根；观察完整字段及内嵌 NUL 模块文本的析构器。
- Queue：SPSC／MPSC／MPMC、自持 Future、重复 Array 边及捕获闭包六类图；
  原生 Retain／单独 DeepClone shell 外部根，释放后 weak ref 过期。
- 清理验证分配／释放计数、live count／bytes、invalid/double free、UAF，
  并核对每个语言析构器只运行一次；不能把堆调试计数当成全运行时 sanitizer。
- 三个 await 跨 string／bytes／Array／closure：成功、拒绝、取消及关闭，
  ready、pending Watch、pending Commit 三种时序，finally 每激活恰好一次。
- 宿主为严格 C11／Wall／Wextra／Werror，图宿主另启 Wshadow；生成 C
  保留既有 unused-helper／missing-field-initializers 豁免，未掩盖新 native 警告。
- 此门不证明独立 AOT 图退役、镜像 MEM_FREE/unmap、整个运行时 sanitizer、
  全调度器 OOM、并发 stress 或完整方法/组件裁剪。B143/B144 已有独立产物
  和持锁恢复证据也不能据此外推成全组验收。
- 仍需处理 list／dict／set 的原生 typed 迁移、SlotMap，WaitQueue／Channel
  生成式拥有者及遗留 Into/输出参数，OrderedMap 公开命名与多返回值合同，
  最后执行 53 模块跨组总门。19 未决策组不变，对象池最后，目标保持 active。

复现：指定上述 `--root`、`--build-report` 和全新的 `--out`；Linux 与 Windows
并行执行相同批次。新证据位于 E 盘；不清理已有失败证据，不更新主 release。
