# 随包编译器源码与独立构建

`xlang/` 保留显式冻结清单中的完整源码输入、构建资源、头文件种子及根构建脚本。
`xlang/SOURCE-MATERIALS.json` 将相对源码路径、摘要与随包 xl.exe/xl.dll/xlc.dll 的摘要关联。
`../provenance/compiler-source-inputs.json` 保留原始来源记录；最新修改描述另列于 provenance。
提供了独立构建报告时，报告与原始日志也保存在 provenance；该报告记录实际本机运行，不是发行签名。

构建需要 Windows x64、MinGW GCC 与 PowerShell。将整个 `xlang` 目录复制到新的可写工作目录，
打开该目录的命令行，运行 `cmd /d /c build.bat`。脚本从包含的 TCC 源码引导构建工具，
再生成 xl.dll、xlc.dll 和 xl.exe；生成结果位于该副本的 release，临时对象位于 build。
保持 XL6_* 构建环境覆盖未设置，采用所附脚本默认配置；源码中已有生成的词法/语法文件，
默认构建不要求重新生成语法。仅在源码副本操作，避免覆盖正在使用的随包工具链。

可以在副本中修改 lib/tcc 等源码后按同一入口重建；本材料不把重新编译描述为逐字节复现保证。
runtime 的实际行为与兼容范围仍按 IDE 验收记录核对。TCC 原始 COPYING 及其他已有文本按原字节保留。
这份源码、构建入口和日志提供技术材料；完整来源、分发条款及重链接许可核对仍单独进行，
relinkReviewComplete 与 legalDistributionReviewComplete 保持 false。
