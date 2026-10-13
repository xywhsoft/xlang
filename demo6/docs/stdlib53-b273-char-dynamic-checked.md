# 动态 char 的转换与类型断言

2026-10-14。此前被拒绝的 `char(var)` 与 `var as char` 已接入统一 IR。
构造转换和类型断言保留不同合同，借用输入，返回平凡的 Unicode scalar。
成功状态独立于码点，U+0000 不再可能与无诊断失败混淆。实际转换逻辑
属于按需原生提供方，不作为标准库函数体拼进 Cgen。

## 输入与错误合同

`char(var)` 接受动态 char、Int 和 UInt。整数须在 U+0000 至 U+10FFFF
范围内，且不能是 surrogate；负值和越界报 RangeError。Bool、Float、
String、null 和容器报 TypeError，不隐式采用数值截断或文本首字符。

`var as char` 要求 CHAR tag，并核对显式运行时身份。没有显式身份的
XRT char 使用其基础 tag；带不同身份的伪造 char 不会因为相同物理
形状而被接纳。整数 65 可以构造 `'A'`，但不能当成已经是 char 的值
通过类型断言。该区别沿用构造转换与 `as` 的既有语义。

原生入口只在完整成功后提交输出。getter 返回失败或报告独立失败时，
包括错误对象为 NULL 的情况，输出保持不变；晚来的错误不能替换首个
无诊断失败。统一 IR 把两条路径登记为 STATUS_OUTPUT，生成代码调用
同一个驻留入口后，以独立布尔结果走 ERROR_TEST，不检查“码点是否为零”
或仅依赖诊断指针。输入不被消费、复制或修改。

checked 整数转换的三份接纳矩阵已收敛到统一 IR 的同一合同。新增
char cast 保持原有枚举编号，节点与指令用 CAST_COUNT 检查闭合范围。
纯 typed char 构造继续使用既有轻量路径；其实际 AOT 不包含新的动态
转换入口。动态 char-only 转换不引入通用字符串／浮点整数解码函数体。

## 验收结果

Windows 完整源码构建 329 个翻译单元，Linux 构建 328 个，无对象覆盖。
每端 107 项检查覆盖九份夹具：确定性 C、严格 C11 O0/O2、JIT、module、
独立 AOT、原有 char／bytes 字面量、标量及可空槽投影、普通整数构造、
Stack 动态值和原综合 pending。十二处转换与四处断言产生的 52 个 IR
变异均在 C/header 输出之前被拒绝。

四条同步调用线程分配故障扫描各有 23 次预算尝试，其中 22 次触发，
直到首次未触发；每次完成模块退役并核对完整内存账本。真实生成程序
验证合法零值、失败零值、无诊断失败和错误后清理。删除生成代码中的
成功状态判断后，实际故障宿主断言失败。

原生专项的 O0/O2、零分配预算、身份与 Unicode 边界测试双端通过，
Linux 对所选实现执行 ASan/UBSan。四份实际错误实现分别丢弃无诊断
失败、忽略身份、截断高整数及接纳 surrogate，全部由运行期断言拒绝，
不以编译失败充数。成功转换不进行堆分配。

最终语言证据为 `E:/xlang-validation/stdlib53-b273-language-win-final1/results.json`
和 `/var/tmp/xlang-stdlib53-b273-language-linux-final1/results.json`；原生专项为
同批 `native-win-final1` 与 `native-linux-final1`。报告须保持 Complete 和
InputsUnchanged。源码构建为 `stdlib53-b273-build-win2` 与
`xlang-stdlib53-b273-build-linux2`，旧 TCC 只作为真实 C 编译前置。

## 剩余范围

本批完成这两种非可空 char 转换，不等同于所有动态／可空 cast 的完整
覆盖。下一步继续 Stack 动态 try、批量和 equals 的拥有结果及类型合同，
以及其他内建容器公开面、Native factory／callback 编码、工作线程／
对象图 OOM、模块组合和发布总门。19 个未决策分组、MPMC 和对象池最后
不变，长期目标保持进行中。
