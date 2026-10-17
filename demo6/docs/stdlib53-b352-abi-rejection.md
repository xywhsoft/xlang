# 标准库错误声明的产品拒绝验收

批次 B352，2026-10-17。Windows 与 Linux 的同源实际产品均拒绝全部
142 份错误声明，在 CLI 生成 C、编译器 DLL／SO 生成 C 和生成头文件三个
入口通过，共各 426 次预期拒绝。错误声明没有产生 C 或头文件。

## 覆盖范围

验收对象是三组标准库既有的声明安全反例，包括文本、数字、Unicode、编码、
Buffer、时间、环境、signal、process、Pattern 和 Regex。另纳入其共享机制
中的原生容器 ABI 准入、参数默认值及五种队列的容量和初始化约束。

| 分类 | 夹具数 | 拒绝条件 |
| --- | --- | --- |
| 精确 ABI | 114 | 宽度、结果、product、回调、owner、nullable 或效果合同不匹配 |
| 原生参数借入 | 2 | point／int 不能代替带有精确 owner 的原生字段借入 |
| 私有成员 | 10 | 不允许直接访问或调用私有原生字段／消费方法 |
| 队列容量 | 5 | 必须显式提供正容量 |
| 队列初始化 | 5 | 不可空的全局、字段和局部声明必须初始化 |
| 参数默认值 | 4 | 拒绝错误默认值类型与可选参数后的必选参数 |
| 未声明容器 ABI | 2 | 不允许任意 typed 容器直接跨原生 class ABI |

人口由 `check_stdlib_abi_rejection_shipping.py` 的固定范围规则发现，并核对
工作区与构建冻结源一致；不在失败后筛选通过子集。控制台 text_output 的
四份反例不属于本次 53 模块范围，未借此纳入。

这些声明反例补充既有运行、所有权、故障和裁剪验收，不能替代 typed 容器
的真实并发／资源策略测试，也不代表每个运行分支覆盖。

## 验证入口与判定

新增观察程序 `test/cases/stdlib_abi_rejection_product_host.c` 只调用公共 API，
不链接编译器实现对象，不执行错误声明中的原生调用。

`xlcCompile` 只完成前端处理，部分错误 ABI 在此时仍可接受；必须继续调用
`xlcEmitXlirC` 或 `xlcEmitXlirHeader` 才验证这里的输出合同。测试专门保留一个
“前端通过、不能当作 ABI 通过”的真实控制。CLI 对应 `cgen --emit --emit-header`。

每次拒绝必须同时满足退出码为 1、命中该类预期诊断、没有生成文件；崩溃、
超时、语法错误或其他无关失败不能冒充预期拒绝。两份原生参数反例按现行验证器
更早给出的精确 native field borrow 诊断判断，不放宽为任意错误。

每端还有 39 项控制：十份合法夹具在三个入口各通过，另外九项确认前端处理、
无关诊断、崩溃、超时、部分文件、三种真实语法拒绝及错误镜像路径不会被误判。
每端共 462 个实际子进程阶段，包含观察程序构建；不是 462 份语言夹具。

## 产品镜像与测试隔离

Windows 观察程序旁明确放置被验证的 DLL。它通过系统模块查询核对实际加载的
编译器完整路径；Linux 使用 dladdr 与规范化路径核对 SO。仅设置 tccDllPath
不能决定原生观察程序的导入库来自哪里。首轮 Windows 正例暴露了 PATH 中旧
编译器被导入的问题，原生调试器确认后修正测试部署；该轮保留为失败记录。
新增的镜像控制以相同字节、不同路径验证身份检查确实拒绝不匹配。

独立目录只放实际产品和选定语言输入，不提供项目 src/lib/res 或私有 SDK
回退；公共头只供构建外部观察程序。报告固定原始及复制的产品、夹具、观察程序、
日志与生成文件摘要。运行时及编译器生产实现未改动，未重建或替换 B350 产品。

## 运行与证据

```text
python tools/check_stdlib_abi_rejection_shipping.py --root <frozen-demo6> --build-report <build.shipping.json> --out <new-directory> --jobs 4
```

最终 Windows 报告为 `D:/xlang-validation/stdlib53-b352-abi-win-r2/results.json`，
Linux 为 `/var/tmp/xlang-stdlib53-b352-abi-linux-r2/results.json`。两端均完整通过；
原始不带 r2 的失败记录不改写为通过。

该检查已加入 `check_stdlib_current_products.py` 的必需子门 `abi-rejection`，
后续完整产品验收自动执行；本批单独验收新门，不把已有 B350 父报告改成包含
新门，也不重复整套未变化的正例矩阵。原有严格 C、独立 AOT、运行与裁剪证据
保持各自范围。

本批补齐错误声明的当前产品验收。最终逐项完成审计继续；Windows DST 按用户
决定独立待验收，CRT 未决策为零、XRT 其余十九组及对象池最后的顺序不变。
