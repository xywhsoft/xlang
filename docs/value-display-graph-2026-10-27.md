# 原生容器显示的 IR 合同与驻留依赖

本批解决 `string(value)`、插值、直接 `print/println` 以及派生类文本中，
原生 List／Set／Dict 与旧 wire 值显示入口不一致的问题。只补足已有容器
的语言文本表示，不新增动态值图的公开 API，也不代替尚未决策的 X07。

## 实现边界

`XL6_IrInstruction.displayGraphProviders` 独立于深复制合同，记录显示所需
的 List／Set／Dict 三个位。Lowering 一次生成事实；Verifier 独立重算，
错误的位集合不能进入 C 或头文件输出。JSON IR 同时输出显示与复制集合。

域分析读取静态类型和 SSA 中明确的装箱、保留、表示转换；对于求值锚点，
只穿过同一基本块里相邻、类型一致、指向同一临时槽的 STORE／LOAD。
普通变量加载和开放返回值保持保守的完整域。递归深度和工作量都有上限，
预算耗尽取完整集合；这一分析不推导所有权、生命周期或借用。

| 显示输入域 | List／Set／Dict 位集合 |
|---|---:|
| 标量、字符串、`array<int>` | 0 |
| `list<int>` | 1 |
| `set<int>` | 2 |
| `dict<int>` | 4 |
| 开放 `var` | 7 |
| 派生类：Dict 字段，Set 字段标记 `@string(skip)` | 4 |

类文本按既有 `stringDeriveIncluded` 字段合同选取依赖，不执行 getter。
直接输出统一走既有 C-owned print IR，保留参数求值、错误边和清理顺序；
也处理零参数输出，避免零长 C 数组及空 operand 的读取。

Cgen 仅输出 `xl6ValueTextNativeV1`／`xl6ClassValueTextNativeV1`／
`xl6CPrintNativeV1` 调用和显式 `XL6_ValueGraphPlanV1`，不从 AST 或名称
追溯容器实现。驻留 formatter 复用现有原生图投影与 XLON；独立擦除参数包
入口 `xl6_xrt_print` 的开放合同需要全部三个 provider，其单独的产物依赖表
已经列全。常规标量和文本输出不会因此带入这三个组件。

现有标量和文本 append 快路径保留。复合值仍需图快照、序列化及其相关
分配，本批没有建立相对手写 C 的零开销或性能提升结论。

## 已验证的范围

- 七个独立域；28 个错误 provider 元数据变异均被 IR、C、头文件拒绝。
- 八个独立语言片段检查实际 artifact plan 及 source 列表；开放参数包另验
  三个 provider 的完整依赖。跳过的 Set 字段不会引入 Set 图实现。
- 五类显示操作的 640 个分配前缀：错误类型、失败不改结果、源内容不变。
- Windows／Linux 的显示、分配失败、擦除参数包三个程序均通过 JIT、AOT、
  GCC `O0/O2`。每个平台 27 项联合检查；原生程序预热后连续三次完成模块
  生命周期，LiveCount／LiveBytes、分配释放差额和错误释放计数保持平衡。
- 完整字符串专项通过 364 项检查，保留原有 ABI 负例、真实前端 fuzz、
  多线程 join、插值分配失败、模块清理及 C／xlang 示例一致性。
- 同一批、同一内容哈希的 536 个旧源码样本仍有 502 个成功，零回退；
  34 个旧拒绝项未变。没有降低原有 514 的全量接纳下限。

## 构建与证据

从新的 IR 布局完整构建 Windows 1295、Linux 1294 个源码编译单元；最终
版本在各自一致的基础上替换 IR、Lowering、Cgen、语义、参数包 formatter、
artifact 依赖表六个实现单元。AOT 的编译器相邻 include 包也同步实际驻留
源码，避免使用旧实现。TCC 核心复用已固定的真实运行库依赖，构建角色为
源码编译器验收，尚不代表新发布 DLL／VFS 的全量交付。

Windows 证据根目录为
`E:/xlang-validation/five-groups-20261027/p7-shipping`；Linux 为
`/home/ubuntu/xlang-validation/20261027-p7-shipping`：

| 证据 | 内容 |
|---|---|
| `source-win-r10-display/build.json` | Windows 完整源码构建 |
| `source-linux-r11-display/build.json` | Linux 完整源码构建 |
| `display-cohort-r26/build.json` | 最终六单元及一致的源码部署 |
| `display-native-r28/results.json` | 每平台 27 项 JIT／AOT／原生／裁剪检查 |
| `display-formal-r31/evidence/results.json` | Windows 完整字符串专项 |
| `display-corpus-r18/results.json` | 536 个固定输入及全部结果 |
| `display-proof-r29.json` | 独立读取并重算源码、对象、日志和产物哈希 |

早期隔离尝试及日志保留：头文件搜索路径、DLL 相邻部署、生成 C 与输入
同名导致的伴随源重复、AOT include 包旧源码、原生编译的未使用 helper
警告，以及专项脚本硬编码对象位置。最终脚本从 build manifest 取得 XRT
对象路径；早期失败未计入通过结果，也未删除 fixture 或放宽 IR 校验。

复跑 Windows 专项使用：

```powershell
./tools/check_xlir_string_join.ps1 -Root <demo6源码根目录> `
  -PublicSourceDirectory <完整源码编译器目录> -WorkDirectory <全新证据目录>
```

`xlir_value_display_pruning_contract.c` 接受若干对 `预期位集合 源码路径`；
`xlir_value_display_host.c` 通过 `-include <生成C>` 验证模块完整清理。
`xlir_value_display_erased_pack.xl` 专门验证独立参数包入口的实际 JIT／AOT。

本批不宣告全部 74 个参数化 guard、514 接纳下限或整个五组长期任务完成。
不包含攻击、扫描、外网探测及网络安全专项测试。
