# 五组接入 P7：声明依赖与真实发布候选验收

本批修复声明选择并建立真实产品证据，不等于五组长期任务完成，也不等于
整个当前工作区或全项目 RC0 已通过。固定输入基线为 `e8240484`；后续其他
工作区修改和提交不能自动继承本批结论。

## 实现与测试修订

- Cgen 不再比较 `xl6DictValueRemoveV1` 名称来选择 Dict 声明，而是查询
  已有驻留符号所属组件。没有新增运行时算法或 ABI，生成端不输出方法体。
- 新正例仅通过显式外部声明调用 resident Remove；不调用 count/length 等
  可能间接拉入 Dict 方法组件的入口。负对照不使用该组件，检查声明未被拉入。
- 完整 `xlir_cgen.c` 单元恢复默认执行：修正旧文本输出返回值、整数位宽，
  为拥有读测试填写独立的构造/求值/投影效果；负例必须命中拥有读诊断。
  未放松生产 verifier，也未用缩小后的测试子集冒充完整单元。
- 更新核心静态守卫的异步 frame、native preamble 和网络拥有目录位置见证。
  全局守卫仍有其他历史生成函数体断言待整理，本批不声明它已全绿。

## 真实产品与验证范围

证据根：`F:/xlang-validation/five-groups-20261025/p7-component-contract`。

归档 `shipping-source-r1.tar` 固定 9943 个普通构建输入，不复制旧 CLI、DLL、
SO、XRT 对象或 VFS 映像。两端分别使用正常 `build.bat` / `build_linux.sh`
构建自己的 TCC、XRT、CLI、运行库、编译库及两套 VFS；不替换源对象。

| 验收面 | Windows | Linux | 限定 |
|---|---|---|---|
| 正常源码构建 | `build-windows-r2/build.shipping.json` | `/var/tmp/xlang-five-groups-20261025-p7-shipping-build-r1/build.shipping.json` | 两端独立真实三件套，不是 source driver |
| 声明组件正例/负对照 | `component-windows-r4/results.json` | `/var/tmp/xlang-five-groups-20261025-p7-component-declarations-r4/results.json` | 各 21 阶段；JIT、module、typed/lowered IR、C、严格 GCC O0/O2、AOT 与导入检查 |
| IDE 查询协议 | `ide-windows-r1/results.json` | `/var/tmp/xlang-five-groups-20261025-p7-ide-cli-r1/results.json` | 各 10 类证明；补充 CLI/API 协议，不代表 GUI IDE 整体验收 |
| source-free 嵌套编译/AOT | `nested-windows-r1/results.json` | `/var/tmp/xlang-five-groups-20261025-p7-nested-aot-r1/results.json` | 隔离真实产品及独立 AOT；Linux 仍需系统 CRT、libc 头文件和链接器 |
| 完整 Cgen 单元 | `../p7-baseline-gates/unit-windows-r3/results.json` | `../p7-baseline-gates/unit-linux-r3/results.json` | 各 O0/O2 编译、链接、执行；复用已核哈希的前批源码对象，并非发布库单元 |

声明专项只链接完整编译库，不能先链接同时导出公共 context 名称的 runtime
库，让 ELF interposition 错选只有运行时资源的 VFS。AOT 使用公共 API 的
原样输出路径，Windows 也不假定其会自动补 `.exe`。

首轮 Windows 构建内存不足、Linux 测试宿主链接次序错误、Windows 测试
后缀错误的原始报告均保留；修订后使用新目录，没有覆盖失败证据。
独立 reader `verify.py` 复核归档、构建输入/产物、验收输入/产物及日志哈希，
汇总为 `verified-r4.json`。本批只做离线普通编译和生命周期验证，不涉及
公网、DNS、攻击模拟或网络安全性质专项。

## 后续仍须闭合

五组联合正常链路、当前网络旧生成实现/fixture 清退、跨模块 checked 退役、
完整相关守卫及所需 IDE 集成验收继续推进；不得用本批候选代替这些证据。
本批也不更新发布目录的二进制，不升级 XRT 时间 ABI，不纳入无关扩展库改动。

未决策保持 CRT 0 项、XRT 11 组：核心/错误/所有权、内存、动态值图、
剩余平台信息、HTTP、WebSocket、xruntime 类型转换、对象图、调用值桥接、
typed 容器、内部支撑。内存池仍最后处理。
