# 私有编译器默认位域布局查询

本补丁叠加在固定 native-c-scalars-v2 源码上，保留原有标量与声明查询。新增私有 tcc_xlang_record_layout，读取实际配置后 TCCState.ms_bitfields；环境查询使用已有 xlCreateTccState 与 xlVisitNativeSources，按实际编译器的默认、上下文和原生源配置设置状态。

同次 nativeEnvironment 可选 recordLayout 字段为 schema 1、scope=tcc-win64-bitfield-default、msBitfields 布尔值。其范围只是默认位域布局选项，不代表复杂 ABI、每种属性或调用约定都等价。原 cPreprocessorGraphComplete 与 abiEquivalenceGuaranteed 均保持 false。

changes.json 绑定三份源码前后摘要、精确替换字节和原种子输入清单；before/after 保存实际源码。准备工具只生成源码副本，真实 build.bat / build_dll.bat 退出码才是构建证据。原始 demo6、现有工具链和先前候选不被覆盖。
