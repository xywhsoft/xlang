# 名义类型判定的错误透明修复

2026-10-14。名义类型判定遇到非整数的内部 class tag 时，会调用报错型
getter，覆盖已有错误或触发诊断分配。这违反了该判定的无分配、错误
透明合同。已在 getter 前加精确类型检查；正常身份、继承闭包和可空
语义不变，不修改 XRT 或公开 API。

## 修复和反例

`xl6ValueAdmitsNominalV1` 读取 `$xlir.class.runtime-type` 后，必须先确认
其为 XVALUE_INT，再调用 xrtValueGetInt。字符串、无符号整数、bool 和
容器不能被当作 runtime tag，也不应为拒绝它们发布一个新的 TypeError。

四类反例在已有 RangeError 及零分配预算下检查返回 false、保留同一个
诊断对象且不触发分配。修复前真实 O0 宿主在错误透明断言处失败，
修复后 Windows O0/O2 与 Linux O0/O2 ASan/UBSan 均通过。原有身份、
可空、消费失败、析构报错及完整 OOM 前缀保持原样，每端 14 项检查、
8 次预算尝试。各路径核对完整内存平衡。

最终验证使用隔离 index 快照，不把其他工作区源修改混入。证据为
`E:/xlang-validation/stdlib53-b268-nominal-win-final1` 和
`/var/tmp/xlang-stdlib53-b268-nominal-linux-final1`；修复前失败为
`stdlib53-b268-nominal-win-before2`。这证明独立原生判定修复，不代替
完整编译器、CLI/VFS 或发布库重新验收；生产源码改变后，先前产品门
不能冒充本次新产品验证。

## 新确认的 Native 动态调用缺口

真实 Windows/Linux compiler 产品均拒绝
`var buffer = Buffer(...); int(buffer.len())`。统一 IR 保留了 len 的
完整声明，但其 Native extern 行被标为 unsupported；Lowered 动态
注册和 C 输出合同仍仅接通 INTERNAL 目标。不是 Buffer 静态方法缺失，
也不能由上一批静态签名覆盖推出 var 调用已完成。

复现保存在 `test/cases/xlir_stdlib_dynamic_native.pending.xl`，实际两端
失败日志及输入哈希为 `E:/xlang-validation/stdlib53-b268-dev/dynamic-known-gap.json`。
pending 用例未计为通过，不进入既有标准库常规清单；此缺口继续作为
完整目标的必修项。新用例的后续方法尚未执行，不能把首项失败外推
为它们已经验证失败或通过。

下一步 Native 动态调用需要统一 IR 的物理调用合同：明确 receiver 与
各参数的原生字段投影、借入／拥有转移、结果转换、默认值和错误边界，
并复用标准直接调用的准备、调用、退役和结果提交事务。仅放宽
INTERNAL 限制不够：bytes、普通 var、名义 class 和容器参数必须有
不同的纯匹配／checked 投影规则，不能仅按指针载体或同名方法判断。
不得为 Buffer.len 增加专用 Cgen 分支或标准库转接函数来掩盖该缺口。

内建容器清单、动态与字段的完整执行证明、跨线程／对象图 OOM、生成
模块组合及发布总门仍需继续。长期目标不关闭；CRT 无未决策，XRT
19 组、WaitQueue MPMC 及对象池最后不变。
