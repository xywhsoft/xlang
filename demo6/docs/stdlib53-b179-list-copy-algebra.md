# List 浅复制与代数操作

已知 List 的 `copy`、`+`、`-`、`+=`、`-=` 使用驻留原生实现。
生成 C 只调用已验证 XLIR 操作，不再生成 IntMap 合并和删键算法。
这是源工厂切换前的消费端迁移，不代表整个 List 载体迁移完成。

## 值与所有权语义

浅复制创建独立 typed 存储，保留经过认证的完整布局、不可变准入
策略和投影绑定。受管理引用元素保持浅复制语义；复制容器不是递归
复制所有子对象。string 保留精确字节长度、UTF-8 和内嵌 NUL；
nullable null 与空字符串不同。

合并保留完整 int64 键，冲突由右侧覆盖，结果使用左侧元素域。
原生双输入必须具有相同布局及等价准入策略；原生左侧与旧 IntMap
混用时，逐项按照实际目标域准入。删键不存在时正常成功，仍返回
独立存储。所有输入借用，所有结果拥有；失败不发布部分结果，也
不修改输入。外部 HANDLE、游标、未绑定投影及 BUSY 对象 checked
拒绝，不猜测载体布局。

旧 IntMap 只作为明确的迁移载体。该兼容分支有真实 XRT collection
模块依赖，不把它隐藏在默认全量链接中；工厂、深复制和持久化切换
后应继续检查能否退出该分支。

## XLIR 异常与提交合同

旧代数操作曾缺少失败结果合同，合并分配失败后，生成函数可能把
空结果作为成功返回。现在 List 加减及复合赋值的效果为 MAY_RAISE，
物理结果为 NULL_RESULT。共同 lowering 在结果分支之后才提交拥有
值，Cgen 不增加特殊补救分支。

四处代数操作的 16 项损坏 IR 在输出 C 和声明头前拒绝，覆盖丢失
错误测试、合并成功失败目标、错误结果操作数和伪造 no-raise 效果。
同步、async 和 generator 共用驻留入口选择。新增浅复制与代数组件
按需选择；标量程序与不调用 List 代数的 Array/open-index 程序不会
附带这一新组件。开放索引仍可合法依赖 List 读取，不能误称为完全
没有 List 能力的程序。

## 验收范围

生产冻结源码分别位于
`E:/xlang-validation/stdlib53-b179-source-dev2/frozen/demo6` 和
`/var/tmp/xlang-stdlib53-b179-source-dev2`。Windows 重编译 267 个翻译
单元，Linux 重编译 266 个，仅复用供应商 TCC 引擎。

双端浅复制与代数专项各通过 126 阶段、82 条闭合故障前缀和 536 次
尝试。原生 11 种输入/操作组合在 O0/O2 下覆盖分配、错误作用域和
无诊断 provider 失败；真实生成函数检查结果、输入不变、finally
恰好执行一次及每个独立所有权账本。三个真实 provider 负对照分别
编译后被相同合同拒绝，分别破坏存储独立性、合并覆盖规则和删键。

确定性生成 C、O0/O2 生成模块、真实原生语言输入、JIT 以及按
artifact 声明闭包独立链接的脚本均通过。async/generator 的生成 C
与 JIT 通过；这里不把脚本裁剪执行等同于完整原生源工厂迁移。
Linux ASan/UBSan 仅覆盖选定原生闭包，不认证整个编译器。

正式专项报告为
`E:/xlang-validation/stdlib53-b179-algebra-win-final5/results.json` 和
`/var/tmp/xlang-stdlib53-b179-algebra-linux-final5/results.json`。
最终合同测试修正通过显式 test-root 记录哈希；生产冻结源码没有
替换。早期失败报告不是通过证据。

最终源码上的双端存量回归全部通过，报告位于
`E:/xlang-validation/stdlib53-b179-regressions-win-final2` 和
`/var/tmp/xlang-stdlib53-b179-regressions-linux-final2`。

| 回归门 | 每端通过阶段 |
| --- | ---: |
| 拥有值索引与 continuation | 27 |
| 细化容器键 | 33 |
| List 原生快照迭代 | 64 |
| 存量 foreach | 64 |
| List 方法与 continuation | 84 |
| Closed/Open 恢复与分配失败前缀 | 96 |
| 静态 List 索引 | 141 |
| 开放索引与查询裁剪 | 166 |

完整驻留目录双端通过 228 个组件、1,218 个精确导出和 13 个负对照。
文档门仍有 14 项历史拒绝，本批新增为零，不是全仓文档门通过。

## 剩余语言与发布工作

开放 var 的浅复制与公开递归 clone 消费端已由
[开放复制与 List 递归克隆](stdlib53-b180-open-copy-list-clone.md)接通。
List 源工厂、准入/cast、内部复制与持久化恢复仍需联动切换。
Set/Dict 迁移、发布 DLL/SO、VFS、打包 AOT 和三组总验收
仍未完成，本批不关闭长期任务或宣称发布级通过。

JSON/XLON 的共享默认子对象仅按声明 clone 能力隔离，否则 checked
拒绝，不修改外部原对象。CRT 未决策为零，XRT 19 个未决策组及
对象池最后不变，见 [标准库决策清单](标准库决策清单.md)。
