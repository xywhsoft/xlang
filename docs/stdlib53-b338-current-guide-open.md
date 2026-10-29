# 标准库现行接口与系统打开请求

主标准库说明已同步现行容器、进程和正则接口。旧迁移检查点保留在历史章节，
不再用已删除的输出参数接口指导新程序。系统打开请求新增真实产品与受控
外部应用验证；本批不修改生产实现或 XRT。

## 公开说明修正

OrderedMap 的查询直接返回值或带 found 的多返回值；Queue、WaitQueue 与
Channel 接收返回明确状态和拥有值。已删除的输出变量方法不保留兼容入口，
也不重新引入参数修饰。不可空队列必须显式初始化并提供正容量，延迟创建
使用 nullable。SlotMap 的实例／代际键、键遍历、同次快照键值遍历现在有
明确说明。

Array、List、Set、Dict 的现行驻留存储不再写成待迁移。说明区分整数稀疏
List、字符串键 Dict、有序键 OrderedMap，以及基础值复制和可变对象引用。
Stack batch／FixedStack 的历史待办不覆盖已公开接口。

进程说明修正为 timeout -1 无限、0 立即到期，以及并发真实 pipeline。
启动和捕获失败抛出 checked 错误，不能写成空串或伪造退出码。正则说明
同步默认标准库的 Regex、匹配快照、迭代／替换／拆分和 RegexSet，不继续
要求导入旧扩展，也不虚构 RegexBuilder。

主说明的容器示例与 `stdlib_containers_guide.xl` 逐字核对并实际运行，覆盖
合法 null 与缺失、Queue 别名、WaitQueue 超时、SlotMap 快照／失效键及
Channel 拥有接收。文档一致性门另拒绝六种实际坏文本，包括混淆 Stack
pushBatch 的 bool 与批量取出 Array；该检查不代替执行。

## 系统请求的实际验证

Windows 产品真实经过 ShellExecute 打开带 Unicode 和空格路径的测试程序。
测试程序链接为 GUI 子系统，但不创建任何窗口或控制台，不修改文件关联和
注册表。它核对自身实际路径后写入唯一到达标记。

Linux 产品真实经过 XRT spawn 与 PATH 查找，启动仅在测试私有 PATH 中的
独立 xdg-open launcher。launcher 核对唯一 argv 目标，包括 UTF-8、空格、
引号和 shell 元字符；不把它们当作 shell 命令执行。没有替换 XRT 函数或驻留
提供方，也不启动用户浏览器／编辑器。

外部程序在写入标记后返回 7，语言层的 open 仍返回 true。这个值表示系统
接受请求，不是应用退出码或展示完成。直接执行控制验证标记协议，错误目标
必须返回 17 且不写标记，防止陈旧文件冒充成功。

真实 JIT、模块和迁移后的独立 AOT 都必须取得自己的到达标记。两端同时
严格编译保存的 module 与原生实现 C，检查独立程序不依赖 xl／xlc 动态库，
Linux 还拒绝 RPATH／RUNPATH。

这一验证覆盖实际请求路径，不证明任意桌面应用如何解释文件或 URI；macOS
也不由 Windows／Linux 结果外推。原有前缀、拒绝、错误与分配故障专项继续
保留。Windows 指定时区 DST 仍是单独的平台验收条件。

## 验收入口

`check_stdlib_process_open_shipping.py --root <frozen-demo6> --build-report
<ordinary-build.json> --out <fresh-directory>` 验证上述真实系统请求路径。
`check_stdlib_guide_contracts.py --root <frozen-demo6> --out <fresh-directory>`
核对现行说明与示例。两者均进入产品必需子门。

冻结生产实现继续来自同一普通构建；新增断言通过显式 fixture addition 纳入
完整人口，不改变产品或提供方。完整 53 模块完成审计继续，CRT 未决策 0、
XRT 未决策 19 组及对象池最后不变。

## 实际产品结果

Windows／Linux 的完整人口各 327 项。两端 JIT／模块适用正例各 318 项、
迁移后的独立 AOT 正例各 317 项通过；七／八项专用宿主、一项预期拒绝和
一项平台条件仍分别列出，不作为普通正例成功。

系统请求专项每端 13 个阶段、三条独立到达标记通过；主说明一致性每端七项
正反例通过。容器示例保存 C 并以 O0／O2 编译 module／原生实现，每端五个
阶段通过。返回类型复核修正后，必需产品子门 Windows 19、Linux 21 项
再次全部通过，17／19 项复算后复用；完整 JIT／模块人口、头文件与现行文档门
重跑。独立 AOT 及未改变的签名证据按原输入保留，不重新编译相同程序。

完整产品报告在 `D:/xlang-validation/stdlib53-b338r2-products-win/results.json`
和 `/var/tmp/xlang-stdlib53-b338r2-products-linux/results.json`；独立 AOT 在
同前缀的 `b338-aot-*`。专项报告在 `b338-open-*` 和 `b338-guide-strict-*`。

最终 `/var/tmp/xlang-stdlib53-b338r2-witnesses.json` 复核 32,352 个唯一文件，
699 项目录签名的同源码产品执行用例关联缺口为零，Complete／InputsUnchanged
均为 true。该结果不等同全部分支覆盖或完整 53 模块审计结项。
