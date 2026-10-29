# B144：Stack 批量接口、独立 Array 发布与真实产品验收

本批关闭 B52/B65/B72 的原生批量底座与语言公开面之间的缺口，不重写
XRT 的批量算法，不批准新的线程／调度器 API。持锁对象失去语言引用
仍采用宿主 checked 清理／恢复；19 个未决策组不变，对象池最后。

## 公开合同与实现

新增 `Stack<T>.pushBatch(array<T>) -> bool`、`popBatch(maximum) -> array<T>`、
`peekBatch(maximum) -> array<T>`、`peekBatchAt(depth, maximum) -> array<T>`。
详见《标准库设计》的 B144 节。最大数量截取实际剩余数量；负数及
depth > count 抛 RangeError，depth == count／零请求返回独立拥有的空结果。
不恢复 Into／写回参数或 byref/byval，不重新标记已有 Array 的元素域。

- 统一 IR 的单一批量 schema 同时检查逻辑签名和闭合原生指令：Stack／
  Array 的元素身份、slot、transfer、codec、精确整数参数及结果身份；
  泛型模板只在真正的泛型函数中保留 dependent admission，具体实例须闭合。
- receiver／输入 Array 为 borrow，数量为 value，提取结果为 owned；
  may-raise 与 false-result／null-result 的错误物理 ABI 明确登记，拥有结果
  只在成功边发布。Cgen 复用 IR 检查并渲染固定原生调用，无批量库算法。
- `xl6StackExtractArrayV1` 先取得源 pin、共享的不可变准入元数据，准备
  独立 Array owner／projection／storage，再复用 Sequence/XRT 的尾部
  复制或移动。结果的容器元数据不持有源 Stack，也不登记生成 image
  回调；引用元素自身所需的对象／代码拥有者仍按原合同保留。
- pop 的分配失败保留源 count／capacity／地址／内容；push 失败保留
  count／逻辑内容，但 capacity 可以已经扩大。string/bytes 保持值语义
  和内嵌 NUL 的完整长度；对象元素保留同一对象引用。
- 原生结果发布保留原诊断及首错，失败退役未发布结果。OS 错误上下文
  初始化失败发生在取得 owner 前，返回 checked 失败，不直接终止进程。
  这是审阅关闭的边界；堆 OOM 注入不冒充 OS TLS/FLS 失败注入。
- 组件计划及 TCC 符号登记只在可达 batch 时选择 STACK_BATCH 和它的
  FOREIGN_ERROR 依赖。普通 Stack 不因此选择 batch provider；XRT 和
  xruntime 算法快照未在本批修改。

## 最终源与证据

最终源：`E:/xlang-validation/stdlib53-b144-source-final-dev1/frozen/demo6`，
由选定 Git index 导出 5534 个输入，不包含无关 IDE／扩展库／编译输入修改。
Linux 复制至 `/var/tmp/xlang-stdlib53-b144-final-dev1`，在产品构建前逐文件
diff 确认完全一致。源码专项和普通 build.bat／build_linux.sh 均使用此源。
两端从源码重建 TCC、XRT、runtime/compiler VFS、两份共享库和 CLI；没有
旧 provider 对象、共享库或 VFS 覆盖，没有更新主 release。

最终证据位于 `E:/xlang-validation/`；早期失败及修复前报告保留但不计入验收。
以下 JSON 均须 Complete／InputsUnchanged 为 true，Errors/Failures 为空：

| 证据目录 | 执行检查／闭合分配失败前缀 |
| --- | ---: |
| stdlib53-b144-build-win-final-dev1 / build-linux-final-dev1 | 真正产品重建与原始字节指纹 |
| stdlib53-b144-native-win-final-dev1 / native-linux-final-dev1 | 每端 636／128 |
| stdlib53-b144-shared-win-final-dev1 / shared-linux-final-dev1 | 每端 636／128 |
| stdlib53-b144-shared-xlc-win-final-dev1 / shared-xlc-linux-final-dev1 | 每端 636／128 |
| stdlib53-b144-publication-win-final-dev2 / publication-linux-final-dev2 | 每端 194／50 |
| stdlib53-b144-public-win-final-dev1 / public-linux-final-dev1 | 64／70 阶段 |
| stdlib53-b144-failures-win-final-dev1 / failures-linux-final-dev1 | 1495／1499，分别 4 个完整前缀 |
| stdlib53-b144-sync-win-final-dev1 / sync-linux-final-dev1 | 每端 26 阶段 |

这些是优化级别／平台／产品的重复执行检查，不是数千个不同的公开函数。
最终 `stdlib53-b144-index-win-final-dev3.json` 和 `index-linux-final-dev3.json`
逐项核对当前 index、编译输入、最终测试输入及所有产品／报告指纹。
冻结源中的测试文件若与最终 index 不同，必须显式说明且有当前报告哈希
证明，不能将旧夹具或旧产品偷偷算入通过结论。
最终 provenance 核对拒绝了旧 Sequence 脚本顺带记录的主工作区 SDK 头；
脚本增加显式 frozen root 后双端重跑，旧混入无关输入的结果不计入最终验收。

## 覆盖与验收边界

- 原生 8 域覆盖 int8 逻辑值／I64 物理槽、双字段 string、nullable string、
  ANY、bytes、nullable nominal、nullable int8 和 C uint16。每 lane 128 个
  OOM 前缀、630 个位置，包含 owned pop／peek／零请求；向量、旧诊断、
  原生拥有图和结果在源释放后的寿命平衡。
- Linux 选定源码及 Sequence 发布回归启用 ASan/UBSan、泄漏和 UAR 检查；
  真实共享库不带 sanitizer，不能把宿主检测写成产品全量插桩。
- 正式 CLI 检查 Typed／Lowered IR 和确定性 C；严格 O0/O2 生成 C、实际
  JIT 及 AOT 覆盖泛型、nullable/char/C scalar/enum、bytes、名义对象、
  Thread、Tree、Queue 和嵌套 Stack 的 owned Array 边界，四个负例在语义
  阶段拒绝且不覆盖既有输出文件。资源夹具先等待 Thread 完成，非并发压测。
- 两份实际产品分别执行 O0/O2 的完整同步 init/main/fini 堆失败前缀，
  first-no-hit 均为 370，四 lane 共 1484 个位置／平台；失败是 MemoryError
  且拥有者平衡。不含其他线程分配点或全组 OOM 证明。
- Linux 真实 libxlc 的 O0/O2 宿主对 11 个闭合 batch 调用，各拒绝 187 次
  错误 ABI／元素／拥有结果／非法元数据／失败边篡改，C 与 header 均零输出。
  Windows DLL 不导出私有 IR API，此项不冒充 Windows 私有编译器篡改测试。
  compiler SDK 既有 bytes typedef shadow，因此该宿主不加 Wshadow；仍为
  Wall/Wextra/Werror 无豁免。生成 C 保留既有 unused-helper 的显式豁免。
- JIT 放在无 src/lib/res/include 的产品目录，不注入 xlang 声明头或外部
  TCC 发行目录。目标系统 libc、CRT 和链接器仍是 Linux 平台输入。
- 三个独立 DLL/SO：batch、普通 Stack、无 Stack；消费者不链接 xlang、
  不注入 provider，实际执行导出函数及 checked Unit。Linux 完整符号表
  证明仅 batch 产物含 xl6StackExtractArrayV1，GNU_STACK 为 RW；Windows
  不以公开导出缺失冒充私有静态符号裁剪。image 保留到进程结束，不宣称
  XRT 动态镜像可安全卸载。
- 两端两份新产品均重跑持锁失去引用：checked 失败、保留记录、取得对象、
  宿主手动解锁、重试，含 module/global 清理、原错误保留及寿命平衡。
  不自动解锁、丢弃或 abort，mutex 仍须在取得锁的原生线程解锁。

本批不是全仓 RC0、发布推广、跨线程完整竞争验证或 53 模块总验收。
生成式拥有图／continuation 跨容器边界与跨组总门继续，长期目标保持 active。
