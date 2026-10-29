# B143：Thread 真实发布产品、私有声明头闭包与持锁恢复回归

本批继续已批准的三组 53 模块任务，不新增 X15/X16 公开能力。用户选择的
持锁恢复入口仍为宿主 checked 清理／恢复：保留可重试记录，宿主取得句柄、
手动解锁并重试；不自动解锁、abort 或丢弃资源。

## 修复

真实产品批量验收发现源码专项未暴露的共同缺陷：运行时 VFS 的私有头文件
逐文件清单遗漏 Tree/Sequence；Array/Tree 等头还依赖 `src/runtime/` 开发路径。
两端均可复现，包括 Thread、mutex、sem 的 Tree/Array 用例。

- 82 个运行时头中的 103 处同库包含改为同目录引用，仅改包含路径，不改
  ABI、结构布局或算法。驻留源码在开发树及内嵌编译器树中仍使用同一份头。
- Windows/Linux 的 runtime/compiler 四份清单自动收录 `src/runtime/*.h`，
  删除 236 条逐文件登记。130 个头均可从私有声明根访问；不嵌入 `.c`。
- 声明完整不代表实现全量链接。普通产物继续使用 XLIR 组件计划；Thread
  和 Thread Sequence 策略各自按实际可达需求进入产物。
- 更新能力矩阵中 Thread 的实际直接使用计数，修复 Linux 构建的快照检查。
- 隔离导出补齐普通 Windows 构建需要的宿主示例 C 源和旧门禁脚本，
  不复制旧 release 产品，不纳入无关 IDE／扩展库／编译输入重构。

## 真正重建及验收

共同源快照：`E:/xlang-validation/stdlib53-b143-source-win-dev3/frozen/demo6`。
Linux 将这份快照复制到全新的 `/var/tmp/xlang-stdlib53-b143-shipping-dev3/demo6`。
普通 `build.bat` / `build_linux.sh` 从源码重建 TCC、XRT、两份 VFS、两份
共享库及 CLI，无旧 provider 对象或共享库覆盖；主 release 未更新。

最终证据均在 `E:/xlang-validation/` 下，失败的 dev1/dev2 输入和报告保留：

| 证据目录／文件 | 已通过 |
| --- | ---: |
| stdlib53-b143-build-win-dev3 / build-linux-dev3 | 两端完整产品构建 |
| stdlib53-b143-thread-shipping-win-dev2 | 67 阶段 |
| stdlib53-b143-thread-shipping-linux-dev2 | 70 阶段 |
| stdlib53-b143-sync-shipping-win-dev2 / sync-shipping-linux-dev2 | 各 26 阶段 |
| stdlib53-b143-sync-elf-linux-dev2 | 28 阶段 |
| stdlib53-b143-headers-win-dev1.json / headers-linux-dev1.json | 每端两份真实 VFS、130 头 |
| stdlib53-b143-parent-thread-win-dev1.log | 原 Thread ABI 父门真实 CLI／严格 C |

上述运行／CLI 专项共 217 阶段，另有四份 VFS 核对及原父门。JSON 要求
Complete、InputsUnchanged 为 true，Errors/Failures 为空；所有实际输入、
产品、bootstrap、XRT 对象和 VFS 按原始字节重新校验。最终 index 核对为
`stdlib53-b143-index-win-dev2.json`、`stdlib53-b143-index-linux-dev3.json`。

- 每个平台的 runtime/compiler 产品均执行 O0/O2 宿主上的九个实际 JIT
  用例；闭包的 NUL string、bytes、char，Thread 容器及 Sem/Sync 回归，
  源码 image 退役和内存平衡。OOM 命名夹具本批只跑正常路径，分配点注入
  仍属于 B142 的独立证明，不把本批冒充为重新注入 OOM。
- JIT/CLI 产品置于无 src/lib/res/include 的独立目录；仅保留产品和夹具。
  JIT 不添加外部 xlang 头或 TCC 发行目录。Linux 的 libc、CRT、动态链接器
  仍是目标系统输入，不声称整套系统头已内嵌。
- 真实 CLI 的 Typed/Lowered IR 验证六种 RuntimeOp、完整身份、borrow/owned/
  error ABI 和捕获；确定性 C 不含旧 Thread 实现，严格 O0/O2 C 生命周期通过。
  新 JIT/独立宿主无警告豁免，生成 C 保留既有 unused-helper 的明确豁免。
- 三个独立 DLL/SO：普通 Thread、容器 Thread、无 Thread 控制。消费者不
  链接 xlang 库、不注入 provider，实际执行导出函数及 checked module Unit。
  Linux 完整符号表证明 Thread provider／Thread Sequence 策略分别按需存在，
  三个 ELF 均为 RW 非执行栈；Windows 只证明公开导出及独立执行，不把没有
  私有符号表的 PE 导出缺失冒充静态裁剪。
- CODE_LEASE 本身合法需要 XRT thread；无语言 Thread 控制仍可选中底层
  thread 模块。这是语言 provider 裁剪证明，不是底层 thread 内核全部移除。
- 双端两份共享库实际执行持锁失去引用后的 checked 失败、取得句柄、手动
  解锁和重试；含 module/global 清理及原错误保留，寿命平衡。Linux 四个
  同步独立 SO 在 O0/O2 独立消费者中执行成功，不依赖 xlang 动态库。

## 体积与边界

相对同源码、修复前的 Windows dev2，完整私有声明 SDK 的 VFS 使 xl.dll
增加 65,536 字节、xlc.dll 增加 131,584 字节；这是压缩声明资源／对齐开销，
不是向 Cgen 或普通用户产物加入库函数体。最终两库为 7,466,496／12,028,928
字节。普通 Thread／容器 Thread／无 Thread DLL 为 596,480／714,240／510,976
字节；对应 ELF 为 783,621／937,693／677,797 字节。尺寸不是质量或完整裁剪证书。

XRT Wait 是逻辑完成，不是 OS join。独立 Thread DLL/SO 在两个平台均保留
到进程退出，不调用 FreeLibrary/dlclose；本批不宣称运行时镜像可安全卸载。
也不是所有静态 XRT 内核、全仓 RC0、发布推广或 53 模块总验收。剩余生成式
拥有图与跨组总验收继续；19 个未决策组不变，对象池最后，长期任务 active。
